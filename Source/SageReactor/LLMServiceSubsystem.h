#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LLMTypes.h"
#include "LLMProviderBase.h"
#include "LLMServiceSubsystem.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnLLMResponseReceived, const FLLMResponse&, Response);

UCLASS()
class SAGEREACTOR_API ULLMServiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "LLM")
	void SetProvider(ELLMProvider ProviderType, const FLLMProviderConfig& ProviderConfig);

	UFUNCTION(BlueprintCallable, Category = "LLM")
	void SendChatRequest(const FLLMRequest& Request, const FOnLLMResponseReceived& OnComplete);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "LLM")
	ELLMProvider GetCurrentProviderType() const { return CurrentProviderType; }

private:
	UPROPERTY()
	TObjectPtr<ULLMProviderBase> ActiveProvider;

	ELLMProvider CurrentProviderType = ELLMProvider::Ollama;

	ULLMProviderBase* CreateProvider(ELLMProvider ProviderType);
};
