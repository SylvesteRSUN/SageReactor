#include "LLMServiceSubsystem.h"

void ULLMServiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void ULLMServiceSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void ULLMServiceSubsystem::SendChatRequest(const FLLMRequest& Request, const FOnLLMResponseReceived& OnComplete)
{
	// TODO: Implement HTTP POST to Ollama /api/chat
}
