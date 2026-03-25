#include "ResponseValidator.h"

int32 UResponseValidator::MaxResponseLength = 500;

TArray<FString> UResponseValidator::ForbiddenPhrases = {
	TEXT("as an AI"),
	TEXT("language model"),
	TEXT("I'm an AI"),
	TEXT("I am an AI"),
	TEXT("as a large language model"),
	TEXT("I cannot assist"),
	TEXT("I'm just a computer program")
};

// --- URetryHandler ---

void URetryHandler::Start(ULLMServiceSubsystem* InLLMService, const FLLMRequest& InRequest,
	const FOnLLMResponseReceived& InOnComplete, int32 InMaxRetries)
{
	LLMService = InLLMService;
	Request = InRequest;
	OnComplete = InOnComplete;
	MaxRetries = InMaxRetries;
	AttemptCount = 0;

	FOnLLMResponseReceived Callback;
	Callback.BindDynamic(this, &URetryHandler::OnResponse);
	LLMService->SendChatRequest(Request, Callback);
}

void URetryHandler::OnResponse(const FLLMResponse& Response)
{
	AttemptCount++;

	if (!Response.bSuccess)
	{
		// Network/provider error — pass through, don't retry
		OnComplete.ExecuteIfBound(Response);
		return;
	}

	FValidationResult Validation = UResponseValidator::ValidateResponse(Response.Content);
	if (Validation.bIsValid)
	{
		OnComplete.ExecuteIfBound(Response);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Response validation failed (attempt %d/%d): %s"),
		AttemptCount, MaxRetries, *Validation.FailureReason);

	if (AttemptCount >= MaxRetries)
	{
		UE_LOG(LogTemp, Error, TEXT("Max retries reached after %d attempts. Returning last response."), AttemptCount);
		OnComplete.ExecuteIfBound(Response);
		return;
	}

	// Retry
	FOnLLMResponseReceived Callback;
	Callback.BindDynamic(this, &URetryHandler::OnResponse);
	LLMService->SendChatRequest(Request, Callback);
}

// --- UResponseValidator ---

FValidationResult UResponseValidator::ValidateResponse(const FString& ResponseContent)
{
	FValidationResult Result;

	if (!CheckNotEmpty(ResponseContent, Result.FailureReason))
	{
		Result.bIsValid = false;
		return Result;
	}

	if (!CheckLength(ResponseContent, Result.FailureReason))
	{
		Result.bIsValid = false;
		return Result;
	}

	if (!CheckForbiddenWords(ResponseContent, Result.FailureReason))
	{
		Result.bIsValid = false;
		return Result;
	}

	return Result;
}

void UResponseValidator::SendWithRetry(
	UObject* WorldContextObject,
	const FLLMRequest& Request,
	const FOnLLMResponseReceived& OnComplete,
	int32 MaxRetries)
{
	UGameInstance* GI = nullptr;
	if (AActor* Actor = Cast<AActor>(WorldContextObject))
	{
		GI = Actor->GetGameInstance();
	}
	else if (UActorComponent* Comp = Cast<UActorComponent>(WorldContextObject))
	{
		GI = Comp->GetOwner()->GetGameInstance();
	}

	if (!GI)
	{
		FLLMResponse ErrorResponse;
		ErrorResponse.bSuccess = false;
		ErrorResponse.ErrorMessage = TEXT("SendWithRetry: Could not get GameInstance from WorldContext");
		OnComplete.ExecuteIfBound(ErrorResponse);
		return;
	}

	ULLMServiceSubsystem* LLMService = GI->GetSubsystem<ULLMServiceSubsystem>();
	if (!LLMService)
	{
		FLLMResponse ErrorResponse;
		ErrorResponse.bSuccess = false;
		ErrorResponse.ErrorMessage = TEXT("SendWithRetry: LLMServiceSubsystem not found");
		OnComplete.ExecuteIfBound(ErrorResponse);
		return;
	}

	// Create a retry handler that persists across async callbacks
	URetryHandler* Handler = NewObject<URetryHandler>(WorldContextObject);
	Handler->Start(LLMService, Request, OnComplete, MaxRetries);
}

bool UResponseValidator::CheckNotEmpty(const FString& Content, FString& OutReason)
{
	if (Content.TrimStartAndEnd().IsEmpty())
	{
		OutReason = TEXT("Response is empty");
		return false;
	}
	return true;
}

bool UResponseValidator::CheckLength(const FString& Content, FString& OutReason)
{
	if (Content.Len() > MaxResponseLength)
	{
		OutReason = FString::Printf(TEXT("Response too long (%d chars, max %d)"), Content.Len(), MaxResponseLength);
		return false;
	}
	return true;
}

bool UResponseValidator::CheckForbiddenWords(const FString& Content, FString& OutReason)
{
	FString LowerContent = Content.ToLower();
	for (const FString& Phrase : ForbiddenPhrases)
	{
		if (LowerContent.Contains(Phrase.ToLower()))
		{
			OutReason = FString::Printf(TEXT("Response contains forbidden phrase: \"%s\""), *Phrase);
			return false;
		}
	}
	return true;
}
