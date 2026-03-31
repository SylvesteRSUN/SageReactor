#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CharacterProfile.h"
#include "DialogueSession.generated.h"

/** A single line of dialogue with metadata for debugging. */
USTRUCT(BlueprintType)
struct SAGEREACTOR_API FDialogueEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString Speaker;

	UPROPERTY(BlueprintReadWrite)
	FString Content;

	UPROPERTY(BlueprintReadWrite)
	FDateTime Timestamp;

	UPROPERTY(BlueprintReadWrite)
	FString RawPrompt;

	UPROPERTY(BlueprintReadWrite)
	float ResponseTimeMs = 0.0f;
};

/**
 * Manages a multi-turn dialogue conversation between a player and an NPC.
 * Stores dialogue history and provides formatted output for LLM context and UI display.
 */
UCLASS(BlueprintType)
class SAGEREACTOR_API UDialogueSession : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UCharacterProfile> Character;

	UPROPERTY(BlueprintReadWrite)
	FString SceneContext;

	UPROPERTY(BlueprintReadWrite)
	TArray<FDialogueEntry> Entries;

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void AddPlayerMessage(const FString& Message);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void AddNPCResponse(const FString& Response, const FString& RawPrompt, float ResponseTime);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	TArray<FString> GetMessageHistoryForLLM() const;

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void ClearHistory();

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	FString ExportToJSON() const;
};
