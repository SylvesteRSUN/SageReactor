#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LLMTypes.h"
#include "LLMProviderBase.generated.h"

DECLARE_DELEGATE_OneParam(FOnLLMRequestComplete, const FLLMResponse&);

/**
 * Abstract base class for LLM provider implementations.
 * Each provider (Ollama, OpenAI, Anthropic, Gemini) handles its own
 * request formatting, HTTP transport, and response parsing.
 */
UCLASS(Abstract)
class SAGEREACTOR_API ULLMProviderBase : public UObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(const FLLMProviderConfig& InConfig);

	virtual void SendRequest(const FLLMRequest& Request, const FOnLLMRequestComplete& OnComplete) PURE_VIRTUAL(ULLMProviderBase::SendRequest, );

	virtual FString BuildRequestBody(const FLLMRequest& Request);

	virtual FLLMResponse ParseResponse(const FString& RawJSON);

protected:
	UPROPERTY()
	FLLMProviderConfig Config;

	double RequestStartTime = 0.0;
};
