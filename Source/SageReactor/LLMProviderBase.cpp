#include "LLMProviderBase.h"

void ULLMProviderBase::Initialize(const FLLMProviderConfig& InConfig)
{
	Config = InConfig;
}

FString ULLMProviderBase::BuildRequestBody(const FLLMRequest& Request)
{
	return TEXT("");
}

FLLMResponse ULLMProviderBase::ParseResponse(const FString& RawJSON)
{
	FLLMResponse Response;
	Response.RawJSON = RawJSON;
	return Response;
}
