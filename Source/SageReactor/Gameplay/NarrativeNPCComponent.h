#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterProfile.h"
#include "DialogueSession.h"
#include "NarrativeState.h"
#include "LLMTypes.h"
#include "LLMServiceSubsystem.h"
#include "NarrativeNPCComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCResponseReady, const FString&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerEnteredRange);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerExitedRange);

/**
 * Actor component that gives an NPC LLM-driven dialogue capability.
 * Manages player proximity detection, dialogue sessions, and LLM request routing.
 * Attach to any Actor with a CharacterProfile to enable AI-powered conversations.
 */
UCLASS(ClassGroup=(SageReactor), meta=(BlueprintSpawnableComponent))
class SAGEREACTOR_API UNarrativeNPCComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNarrativeNPCComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
	TObjectPtr<UCharacterProfile> CharacterProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative", meta = (MultiLine = true))
	FString SceneContext = TEXT("An NPC stands before the player.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionRadius = 300.f;

	// --- State ---

	UPROPERTY(BlueprintReadOnly, Category = "Narrative")
	bool bPlayerInRange = false;

	UPROPERTY(BlueprintReadOnly, Category = "Narrative")
	bool bInDialogue = false;

	// --- Events (for Blueprint binding) ---

	UPROPERTY(BlueprintAssignable, Category = "Narrative")
	FOnNPCResponseReady OnNPCResponseReady;

	UPROPERTY(BlueprintAssignable, Category = "Narrative")
	FOnPlayerEnteredRange OnPlayerEnteredRange;

	UPROPERTY(BlueprintAssignable, Category = "Narrative")
	FOnPlayerExitedRange OnPlayerExitedRange;

	// --- Functions ---

	UFUNCTION(BlueprintCallable, Category = "Narrative")
	void StartDialogue();

	UFUNCTION(BlueprintCallable, Category = "Narrative")
	void EndDialogue();

	UFUNCTION(BlueprintCallable, Category = "Narrative")
	void SendPlayerMessage(const FString& Message);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative")
	FString GetCharacterName() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative")
	UDialogueSession* GetDialogueSession() const { return DialogueSession; }

	// Get formatted dialogue history for UI display
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative")
	FString GetDialogueHistoryText() const;

private:
	UPROPERTY()
	TObjectPtr<UDialogueSession> DialogueSession;

	UPROPERTY()
	TObjectPtr<UNarrativeStateManager> StateManager;

	UPROPERTY()
	TObjectPtr<class USphereComponent> InteractionSphere;

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnLLMResponse(const FLLMResponse& Response);
};
