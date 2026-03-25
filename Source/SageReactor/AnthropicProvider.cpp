#include "AnthropicProvider.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void UAnthropicProvider::SendRequest(const FLLMRequest& Request, const FOnLLMRequestComplete& OnComplete)
{
	RequestStartTime = FPlatformTime::Seconds();

	FString Url = Config.ApiUrl.IsEmpty() ? TEXT("https://api.anthropic.com/v1/messages") : Config.ApiUrl;
	FString Body = BuildRequestBody(Request);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("x-api-key"), Config.ApiKey);
	HttpRequest->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
	HttpRequest->SetContentAsString(Body);
	HttpRequest->SetTimeout(30.0f);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UAnthropicProvider::OnHttpRequestComplete, OnComplete);
	HttpRequest->ProcessRequest();
}

FString UAnthropicProvider::BuildRequestBody(const FLLMRequest& Request)
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	FString ModelName = Request.Model.IsEmpty() ? Config.ModelName : Request.Model;
	if (ModelName.IsEmpty())
	{
		ModelName = TEXT("claude-sonnet-4-20250514");
	}
	RootObject->SetStringField(TEXT("model"), ModelName);
	RootObject->SetNumberField(TEXT("max_tokens"), 256);
	RootObject->SetNumberField(TEXT("temperature"), Request.Temperature);

	// Anthropic uses a separate system field, not in messages array
	if (!Request.SystemPrompt.IsEmpty())
	{
		RootObject->SetStringField(TEXT("system"), Request.SystemPrompt);
	}

	TArray<TSharedPtr<FJsonValue>> MessagesArray;

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

FLLMResponse UAnthropicProvider::ParseResponse(const FString& RawJSON)
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

	// Anthropic format: content[0].text
	const TArray<TSharedPtr<FJsonValue>>* ContentArray;
	if (JsonObject->TryGetArrayField(TEXT("content"), ContentArray) && ContentArray->Num() > 0)
	{
		const TSharedPtr<FJsonObject>* ContentBlock;
		if ((*ContentArray)[0]->TryGetObject(ContentBlock))
		{
			FString Text;
			if ((*ContentBlock)->TryGetStringField(TEXT("text"), Text))
			{
				Response.Content = Text;
				Response.bSuccess = true;
			}
		}
	}

	if (!Response.bSuccess)
	{
		Response.ErrorMessage = TEXT("Failed to extract content from Anthropic response");
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

void UAnthropicProvider::OnHttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FOnLLMRequestComplete OnComplete)
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
