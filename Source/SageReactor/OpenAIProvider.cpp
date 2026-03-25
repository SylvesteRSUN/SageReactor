#include "OpenAIProvider.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void UOpenAIProvider::SendRequest(const FLLMRequest& Request, const FOnLLMRequestComplete& OnComplete)
{
	RequestStartTime = FPlatformTime::Seconds();

	FString Url = Config.ApiUrl.IsEmpty() ? TEXT("https://api.openai.com/v1/chat/completions") : Config.ApiUrl;
	FString Body = BuildRequestBody(Request);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Config.ApiKey));
	HttpRequest->SetContentAsString(Body);
	HttpRequest->SetTimeout(30.0f);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UOpenAIProvider::OnHttpRequestComplete, OnComplete);
	HttpRequest->ProcessRequest();
}

FString UOpenAIProvider::BuildRequestBody(const FLLMRequest& Request)
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	FString ModelName = Request.Model.IsEmpty() ? Config.ModelName : Request.Model;
	if (ModelName.IsEmpty())
	{
		ModelName = TEXT("gpt-4o-mini");
	}
	RootObject->SetStringField(TEXT("model"), ModelName);
	RootObject->SetNumberField(TEXT("temperature"), Request.Temperature);
	RootObject->SetNumberField(TEXT("max_tokens"), 256);

	TArray<TSharedPtr<FJsonValue>> MessagesArray;

	if (!Request.SystemPrompt.IsEmpty())
	{
		TSharedPtr<FJsonObject> SystemMsg = MakeShared<FJsonObject>();
		SystemMsg->SetStringField(TEXT("role"), TEXT("system"));
		SystemMsg->SetStringField(TEXT("content"), Request.SystemPrompt);
		MessagesArray.Add(MakeShared<FJsonValueObject>(SystemMsg));
	}

	for (int32 i = 0; i < Request.MessageHistory.Num(); i++)
	{
		TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), (i % 2 == 0) ? TEXT("user") : TEXT("assistant"));
		Msg->SetStringField(TEXT("content"), Request.MessageHistory[i]);
		MessagesArray.Add(MakeShared<FJsonValueObject>(Msg));
	}

	if (!Request.UserMessage.IsEmpty())
	{
		TSharedPtr<FJsonObject> UserMsg = MakeShared<FJsonObject>();
		UserMsg->SetStringField(TEXT("role"), TEXT("user"));
		UserMsg->SetStringField(TEXT("content"), Request.UserMessage);
		MessagesArray.Add(MakeShared<FJsonValueObject>(UserMsg));
	}

	RootObject->SetArrayField(TEXT("messages"), MessagesArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);
	return OutputString;
}

FLLMResponse UOpenAIProvider::ParseResponse(const FString& RawJSON)
{
	FLLMResponse Response;
	Response.RawJSON = RawJSON;

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJSON);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Failed to parse JSON response");
		return Response;
	}

	const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
	if (JsonObject->TryGetArrayField(TEXT("choices"), ChoicesArray) && ChoicesArray->Num() > 0)
	{
		const TSharedPtr<FJsonObject>* ChoiceObject;
		if ((*ChoicesArray)[0]->TryGetObject(ChoiceObject))
		{
			const TSharedPtr<FJsonObject>* MessageObject;
			if ((*ChoiceObject)->TryGetObjectField(TEXT("message"), MessageObject))
			{
				Response.Content = (*MessageObject)->GetStringField(TEXT("content"));
				Response.bSuccess = true;
			}
		}
	}

	if (!Response.bSuccess)
	{
		Response.ErrorMessage = TEXT("Failed to extract content from OpenAI response");
		const TSharedPtr<FJsonObject>* ErrorObject;
		if (JsonObject->TryGetObjectField(TEXT("error"), ErrorObject))
		{
			FString ErrorMsg;
			if ((*ErrorObject)->TryGetStringField(TEXT("message"), ErrorMsg))
			{
				Response.ErrorMessage = ErrorMsg;
			}
		}
	}

	return Response;
}

void UOpenAIProvider::OnHttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FOnLLMRequestComplete OnComplete)
{
	double ElapsedMs = (FPlatformTime::Seconds() - RequestStartTime) * 1000.0;

	FLLMResponse Response;
	Response.ResponseTimeMs = static_cast<float>(ElapsedMs);

	if (!bSucceeded || !HttpResponse.IsValid())
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("HTTP request failed");
		OnComplete.ExecuteIfBound(Response);
		return;
	}

	int32 ResponseCode = HttpResponse->GetResponseCode();
	FString ResponseBody = HttpResponse->GetContentAsString();

	if (ResponseCode != 200)
	{
		Response.bSuccess = false;
		Response.RawJSON = ResponseBody;
		Response.ErrorMessage = FString::Printf(TEXT("HTTP %d: %s"), ResponseCode, *ResponseBody);
		OnComplete.ExecuteIfBound(Response);
		return;
	}

	Response = ParseResponse(ResponseBody);
	Response.ResponseTimeMs = static_cast<float>(ElapsedMs);
	OnComplete.ExecuteIfBound(Response);
}
