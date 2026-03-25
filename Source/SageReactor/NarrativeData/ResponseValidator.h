#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LLMTypes.h"
#include "LLMServiceSubsystem.h"
#include "ResponseValidator.generated.h"

USTRUCT(BlueprintType)
struct SAGEREACTOR_API FValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bIsValid = true;

	UPROPERTY(BlueprintReadOnly)
	FString FailureReason;
};

// Internal helper object that manages retry state
UCLASS()
class URetryHandler : public UObject
{
	GENERATED_BODY()

public:
	void Start(ULLMServiceSubsystem* InLLMService, const FLLMRequest& InRequest,
		const FOnLLMResponseReceived& InOnComplete, int32 InMaxRetries);

	UFUNCTION()
	void OnResponse(const FLLMResponse& Response);

private:
	UPROPERTY()
	TObjectPtr<ULLMServiceSubsystem> LLMService;

	FLLMRequest Request;
	FOnLLMResponseReceived OnComplete;
	int32 MaxRetries = 3;
	int32 AttemptCount = 0;
};

UCLASS(BlueprintType)
class SAGEREACTOR_API UResponseValidator : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Narrative|Validation")
	static FValidationResult ValidateResponse(const FString& ResponseContent);

	UFUNCTION(BlueprintCallable, Category = "Narrative|Validation", meta = (WorldContext = "WorldContextObject"))
	static void SendWithRetry(
		UObject* WorldContextObject,
		const FLLMRequest& Request,
		const FOnLLMResponseReceived& OnComplete,
		int32 MaxRetries = 3
	);

	static int32 MaxResponseLength;
	static TArray<FString> ForbiddenPhrases;

private:
	static bool CheckNotEmpty(const FString& Content, FString& OutReason);
	static bool CheckLength(const FString& Content, FString& OutReason);
	static bool CheckForbiddenWords(const FString& Content, FString& OutReason);
};
