#pragma once

#include "CoreMinimal.h"
#include "LLMTypes.generated.h"

USTRUCT(BlueprintType)
struct SAGEREACTOR_API FLLMRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString Model;

	UPROPERTY(BlueprintReadWrite)
	FString SystemPrompt;

	UPROPERTY(BlueprintReadWrite)
	TArray<FString> MessageHistory;

	UPROPERTY(BlueprintReadWrite)
	FString UserMessage;

	UPROPERTY(BlueprintReadWrite)
	float Temperature = 0.7f;
};

USTRUCT(BlueprintType)
struct SAGEREACTOR_API FLLMResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite)
	FString Content;

	UPROPERTY(BlueprintReadWrite)
	FString RawJSON;

	UPROPERTY(BlueprintReadWrite)
	float ResponseTimeMs = 0.0f;

	UPROPERTY(BlueprintReadWrite)
	FString ErrorMessage;
};
