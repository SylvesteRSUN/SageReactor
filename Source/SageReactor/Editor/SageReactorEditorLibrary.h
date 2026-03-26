#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterProfile.h"
#include "NarrativeState.h"
#include "DialogueSession.h"
#include "SageReactorEditorLibrary.generated.h"

// Simple struct for character list display
USTRUCT(BlueprintType)
struct FCharacterListEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FString AssetPath;
};

UCLASS()
class SAGEREACTOR_API USageReactorEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// --- Character Profile Management ---

	// Create a new CharacterProfile in memory (not yet saved to disk)
	UFUNCTION(BlueprintCallable, Category = "SageReactor|Editor")
	static UCharacterProfile* CreateCharacterProfile(
		const FString& Name,
		const FString& Role,
		const FString& Personality,
		const FString& Background,
		const FString& CurrentGoal,
		const FString& SpeakingStyle
	);

	// Save a CharacterProfile as a DataAsset in Content Browser
	// AssetName: e.g. "DA_guard_marcus"
	// FolderPath: e.g. "/Game/NarrativeData/Characters"
	UFUNCTION(BlueprintCallable, Category = "SageReactor|Editor")
	static bool SaveCharacterAsset(UCharacterProfile* Profile, const FString& AssetName, const FString& FolderPath);

	// Load a CharacterProfile from an asset path
	UFUNCTION(BlueprintCallable, Category = "SageReactor|Editor")
	static UCharacterProfile* LoadCharacterProfile(const FString& AssetPath);

	// List all CharacterProfile assets in a folder (returns raw paths)
	UFUNCTION(BlueprintCallable, Category = "SageReactor|Editor")
	static TArray<FString> ListCharacterProfiles(const FString& FolderPath);

	// List all CharacterProfile assets with display names (for UI list)
	UFUNCTION(BlueprintCallable, Category = "SageReactor|Editor")
	static TArray<FCharacterListEntry> GetCharacterList(const FString& FolderPath);

	// Find and load a CharacterProfile by display name from a folder
	UFUNCTION(BlueprintCallable, Category = "SageReactor|Editor")
	static UCharacterProfile* FindCharacterByName(const FString& CharacterName, const FString& FolderPath);

	// --- Narrative State ---

	// Create a new NarrativeStateManager instance
	UFUNCTION(BlueprintCallable, Category = "SageReactor|Editor")
	static UNarrativeStateManager* CreateNarrativeStateManager();

	// --- Dialogue Session ---

	// Create a new DialogueSession
	UFUNCTION(BlueprintCallable, Category = "SageReactor|Editor")
	static UDialogueSession* CreateDialogueSession(UCharacterProfile* Profile, const FString& SceneContext);
};
