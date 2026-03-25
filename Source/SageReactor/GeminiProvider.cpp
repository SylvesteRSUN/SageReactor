#include "GeminiProvider.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void UGeminiProvider::SendRequest(const FLLMRequest& Request, const FOnLLMRequestComplete& OnComplete)
{
	RequestStartTime = FPlatformTime::Seconds();

	FString ModelName = Request.Model.IsEmpty() ? Config.ModelName : Request.Model;
	if (ModelName.IsEmpty())
	{
		ModelName = TEXT("gemini-2.0-flash");
	}

	FString Url;
	if (!Config.ApiUrl.IsEmpty())
	{
		Url = Config.ApiUrl;
	}
	else
	{
		Url = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s"),
			*ModelName, *Config.ApiKey);
	}

	FString Body = BuildRequestBody(Request);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(Body);
	HttpRequest->SetTimeout(30.0f);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UGeminiProvider::OnHttpRequestComplete, OnComplete);
	HttpRequest->ProcessRequest();
}

FString UGeminiProvider::BuildRequestBody(const FLLMRequest& Request)
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> ContentsArray;

	// System instruction as a separate field
	if (!Request.SystemPrompt.IsEmpty())
	{
		TSharedPtr<FJsonObject> SystemInstruction = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Parts;
		TSharedPtr<FJsonObject> Part = MakeShared<FJsonObject>();
		Part->SetStringField(TEXT("text"), Request.SystemPrompt);
		Parts.Add(MakeShared<FJsonValueObject>(Part));
		SystemInstruction->SetArrayField(TEXT("parts"), Parts);
		RootObject->SetObjectField(TEXT("systemInstruction"), SystemInstruction);
	}

	// Message history: Gemini uses "user" and "model" roles
	for (int32 i = 0; i < Request.MessageHistory.Num(); i++)
	{
		TSharedPtr<FJsonObject> Content = MakeShared<FJsonObject>();
		Content->SetStringField(TEXT("role"), (i % 2 == 0) ? TEXT("user") : TEXT("model"));

		TArray<TSharedPtr<FJsonValue>> Parts;
		TSharedPtr<FJsonObject> Part = MakeShared<FJsonObject>();
		Part->SetStringField(TEXT("text"), Request.MessageHistory[i]);
		Parts.Add(MakeShared<FJsonValueObject>(Part));
		Content->SetArrayField(TEXT("parts"), Parts);

		ContentsArray.Add(MakeShared<FJsonValueObject>(Content));
	}

	// Current user message
	if (!Request.UserMessage.IsEmpty())
	{
		TSharedPtr<FJsonObject> Content = MakeShared<FJsonObject>();
		Content->SetStringField(TEXT("role"), TEXT("user"));

		TArray<TSharedPtr<FJsonValue>> Parts;
		TSharedPtr<FJsonObject> Part = MakeShared<FJsonObject>();
		Part->SetStringField(TEXT("text"), Request.UserMessage);
		Parts.Add(MakeShared<FJsonValueObject>(Part));
		Content->SetArrayField(TEXT("parts"), Parts);

		ContentsArray.Add(MakeShared<FJsonValueObject>(Content));
	}

	RootObject->SetArrayField(TEXT("contents"), ContentsArray);

	// Generation config
	TSharedPtr<FJsonObject> GenConfig = MakeShared<FJsonObject>();
	GenConfig->SetNumberField(TEXT("temperature"), Request.Temperature);
	GenConfig->SetNumberField(TEXT("maxOutputTokens"), 256);
	RootObject->SetObjectField(TEXT("generationConfig"), GenConfig);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);
	return OutputString;
}

FLLMResponse UGeminiProvider::ParseResponse(const FString& RawJSON)
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

	// Gemini format: candidates[0].content.parts[0].text
	const TArray<TSharedPtr<FJsonValue>>* CandidatesArray;
	if (JsonObject->TryGetArrayField(TEXT("candidates"), CandidatesArray) && CandidatesArray->Num() > 0)
	{
		const TSharedPtr<FJsonObject>* CandidateObject;
		if ((*CandidatesArray)[0]->TryGetObject(CandidateObject))
		{
			const TSharedPtr<FJsonObject>* ContentObject;
			if ((*CandidateObject)->TryGetObjectField(TEXT("content"), ContentObject))
			{
				const TArray<TSharedPtr<FJsonValue>>* PartsArray;
				if ((*ContentObject)->TryGetArrayField(TEXT("parts"), PartsArray) && PartsArray->Num() > 0)
				{
					const TSharedPtr<FJsonObject>* PartObject;
					if ((*PartsArray)[0]->TryGetObject(PartObject))
					{
						FString Text;
						if ((*PartObject)->TryGetStringField(TEXT("text"), Text))
						{
							Response.Content = Text;
							Response.bSuccess = true;
						}
					}
				}
			}
		}
	}

	if (!Response.bSuccess)
	{
		Response.ErrorMessage = TEXT("Failed to extract content from Gemini response");
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

void UGeminiProvider::OnHttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FOnLLMRequestComplete OnComplete)
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
