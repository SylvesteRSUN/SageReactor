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
	// Assign a CharacterProfile DataAsset in the editor Details panel
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
	TObjectPtr<UCharacterProfile> CharacterProfile;

	// Scene context description
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative", meta = (MultiLine = true))
	FString SceneContext = TEXT("The player approaches the main gate of Ironhaven at dusk. The guard stands blocking the entrance.");

	// The player's test message
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
	FString PlayerMessage = TEXT("Good evening! I'd like to enter the city, please.");

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnLLMResponse(const FLLMResponse& Response);

private:
	UPROPERTY()
	TObjectPtr<UDialogueSession> Session;
};
