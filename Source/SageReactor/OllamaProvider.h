#pragma once

#include "CoreMinimal.h"
#include "LLMProviderBase.h"
#include "OllamaProvider.generated.h"

UCLASS()
class SAGEREACTOR_API UOllamaProvider : public ULLMProviderBase
{
	GENERATED_BODY()

public:
	virtual void SendRequest(const FLLMRequest& Request, const FOnLLMRequestComplete& OnComplete) override;
	virtual FString BuildRequestBody(const FLLMRequest& Request) override;
	virtual FLLMResponse ParseResponse(const FString& RawJSON) override;

private:
	void OnHttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FOnLLMRequestComplete OnComplete);
};
