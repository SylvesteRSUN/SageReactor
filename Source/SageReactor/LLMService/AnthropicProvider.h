#pragma once

#include "CoreMinimal.h"
#include "LLMProviderBase.h"
#include "AnthropicProvider.generated.h"

UCLASS()
class SAGEREACTOR_API UAnthropicProvider : public ULLMProviderBase
{
	GENERATED_BODY()

public:
	virtual void SendRequest(const FLLMRequest& Request, const FOnLLMRequestComplete& OnComplete) override;
	virtual FString BuildRequestBody(const FLLMRequest& Request) override;
	virtual FLLMResponse ParseResponse(const FString& RawJSON) override;
};
