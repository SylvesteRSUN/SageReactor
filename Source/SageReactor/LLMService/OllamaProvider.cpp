#include "OllamaProvider.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void UOllamaProvider::SendRequest(const FLLMRequest& Request, const FOnLLMRequestComplete& OnComplete)
{
	RequestStartTime = FPlatformTime::Seconds();

	FString Url = Config.ApiUrl.IsEmpty() ? TEXT("http://localhost:11434/api/chat") : Config.ApiUrl;
	FString Body = BuildRequestBody(Request);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(Body);
	HttpRequest->SetTimeout(30.0f);
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, OnComplete](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSucceeded)
		{
			double ElapsedMs = (FPlatformTime::Seconds() - RequestStartTime) * 1000.0;

			FLLMResponse Response;
			Response.ResponseTimeMs = static_cast<float>(ElapsedMs);

			if (!bSucceeded || !Resp.IsValid())
			{
				Response.bSuccess = false;
				Response.ErrorMessage = TEXT("HTTP request failed - is Ollama running?");
				OnComplete.ExecuteIfBound(Response);
				return;
			}

			int32 ResponseCode = Resp->GetResponseCode();
			FString ResponseBody = Resp->GetContentAsString();

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
		});
	HttpRequest->ProcessRequest();
}

FString UOllamaProvider::BuildRequestBody(const FLLMRequest& Request)
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	FString ModelName = Request.Model.IsEmpty() ? Config.ModelName : Request.Model;
	if (ModelName.IsEmpty())
	{
		ModelName = TEXT("qwen3.5:9b");
	}
	RootObject->SetStringField(TEXT("model"), ModelName);
	RootObject->SetBoolField(TEXT("stream"), false);

	TArray<TSharedPtr<FJsonValue>> MessagesArray;

	// System prompt
	if (!Request.SystemPrompt.IsEmpty())
	{
		TSharedPtr<FJsonObject> SystemMsg = MakeShared<FJsonObject>();
		SystemMsg->SetStringField(TEXT("role"), TEXT("system"));
		SystemMsg->SetStringField(TEXT("content"), Request.SystemPrompt);
		MessagesArray.Add(MakeShared<FJsonValueObject>(SystemMsg));
	}

	// Message history (alternating user/assistant)
	for (int32 i = 0; i < Request.MessageHistory.Num(); i++)
	{
		TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), (i % 2 == 0) ? TEXT("user") : TEXT("assistant"));
		Msg->SetStringField(TEXT("content"), Request.MessageHistory[i]);
		MessagesArray.Add(MakeShared<FJsonValueObject>(Msg));
	}

	// Current user message
	if (!Request.UserMessage.IsEmpty())
	{
		TSharedPtr<FJsonObject> UserMsg = MakeShared<FJsonObject>();
		UserMsg->SetStringField(TEXT("role"), TEXT("user"));
		UserMsg->SetStringField(TEXT("content"), Request.UserMessage);
		MessagesArray.Add(MakeShared<FJsonValueObject>(UserMsg));
	}

	RootObject->SetArrayField(TEXT("messages"), MessagesArray);

	// Options
	TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetNumberField(TEXT("temperature"), Request.Temperature);
	Options->SetNumberField(TEXT("num_predict"), 256);
	RootObject->SetObjectField(TEXT("options"), Options);

	// Disable thinking mode for qwen3 models (returns content in thinking field otherwise)
	RootObject->SetBoolField(TEXT("think"), false);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);
	return OutputString;
}

FLLMResponse UOllamaProvider::ParseResponse(const FString& RawJSON)
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

	const TSharedPtr<FJsonObject>* MessageObject;
	if (JsonObject->TryGetObjectField(TEXT("message"), MessageObject))
	{
		Response.Content = (*MessageObject)->GetStringField(TEXT("content"));

		// Fallback: if content is empty but thinking field exists (qwen3 thinking mode)
		if (Response.Content.IsEmpty())
		{
			FString ThinkingContent;
			if ((*MessageObject)->TryGetStringField(TEXT("thinking"), ThinkingContent))
			{
				Response.Content = ThinkingContent;
			}
		}

		Response.bSuccess = !Response.Content.IsEmpty();
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Response missing 'message' field");

		FString ErrorStr;
		if (JsonObject->TryGetStringField(TEXT("error"), ErrorStr))
		{
			Response.ErrorMessage = ErrorStr;
		}
	}

	return Response;
}

