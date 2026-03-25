#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LLMTypes.h"
#include "DialogueSession.h"
#include "TestActor.generated.h"

UCLASS()
class SAGEREACTOR_API ATestActor : public AActor
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnLLMResponse(const FLLMResponse& Response);

private:
	UPROPERTY()
	TObjectPtr<UDialogueSession> Session;
};
