#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterProfile.h"
#include "DialogueSession.h"
#include "NarrativeState.h"
#include "PromptBuilder.generated.h"

UCLASS()
class SAGEREACTOR_API UPromptBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Narrative|Prompt")
	static FString BuildSystemPrompt(const UCharacterProfile* Profile, const FString& SceneContext, const UNarrativeStateManager* StateManager = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Narrative|Prompt")
	static FString GetDefaultPromptTemplate();

	UFUNCTION(BlueprintCallable, Category = "Narrative|Prompt")
	static void SetPromptTemplate(const FString& NewTemplate);

	UFUNCTION(BlueprintCallable, Category = "Narrative|Prompt")
	static FString BuildChatMessages(const UDialogueSession* Session);

private:
	static FString PromptTemplate;
};
