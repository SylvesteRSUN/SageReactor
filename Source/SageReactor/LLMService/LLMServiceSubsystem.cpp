#include "LLMServiceSubsystem.h"
#include "OllamaProvider.h"
#include "OpenAIProvider.h"
#include "AnthropicProvider.h"
#include "GeminiProvider.h"

void ULLMServiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Default to Ollama with default config
	FLLMProviderConfig DefaultConfig;
	DefaultConfig.Provider = ELLMProvider::Ollama;
	SetProvider(ELLMProvider::Ollama, DefaultConfig);
}

void ULLMServiceSubsystem::Deinitialize()
{
	ActiveProvider = nullptr;
	Super::Deinitialize();
}

void ULLMServiceSubsystem::SetProvider(ELLMProvider ProviderType, const FLLMProviderConfig& ProviderConfig)
{
	ActiveProvider = CreateProvider(ProviderType);
	if (ActiveProvider)
	{
		ActiveProvider->Initialize(ProviderConfig);
		CurrentProviderType = ProviderType;
		UE_LOG(LogTemp, Log, TEXT("LLMService: Provider set to %d"), static_cast<int32>(ProviderType));
	}
}

void ULLMServiceSubsystem::SendChatRequest(const FLLMRequest& Request, const FOnLLMResponseReceived& OnComplete)
{
	if (!ActiveProvider)
	{
		FLLMResponse ErrorResponse;
		ErrorResponse.bSuccess = false;
		ErrorResponse.ErrorMessage = TEXT("No LLM provider configured. Call SetProvider first.");
		OnComplete.ExecuteIfBound(ErrorResponse);
		return;
	}

	ActiveProvider->SendRequest(Request, FOnLLMRequestComplete::CreateLambda(
		[OnComplete](const FLLMResponse& Response)
		{
			OnComplete.ExecuteIfBound(Response);
		}
	));
}

ULLMProviderBase* ULLMServiceSubsystem::CreateProvider(ELLMProvider ProviderType)
{
	switch (ProviderType)
	{
	case ELLMProvider::Ollama:
		return NewObject<UOllamaProvider>(this);
	case ELLMProvider::OpenAI:
		return NewObject<UOpenAIProvider>(this);
	case ELLMProvider::Anthropic:
		return NewObject<UAnthropicProvider>(this);
	case ELLMProvider::Gemini:
		return NewObject<UGeminiProvider>(this);
	default:
		return NewObject<UOllamaProvider>(this);
	}
}
