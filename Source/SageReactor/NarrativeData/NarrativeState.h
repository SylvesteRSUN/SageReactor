#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeState.generated.h"

USTRUCT(BlueprintType)
struct SAGEREACTOR_API FNarrativeState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NarrativeState")
	TMap<FString, FString> StateMap;
};

UCLASS(BlueprintType)
class SAGEREACTOR_API UNarrativeStateManager : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Narrative|State")
	void SetState(const FString& Key, const FString& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|State")
	FString GetState(const FString& Key) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|State")
	bool HasState(const FString& Key) const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|State")
	void RemoveState(const FString& Key);

	UFUNCTION(BlueprintCallable, Category = "Narrative|State")
	void ClearAllStates();

	// Build a formatted string section for injection into the system prompt
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|State")
	FString BuildStatePromptSection() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|State")
	const FNarrativeState& GetNarrativeState() const { return State; }

private:
	UPROPERTY()
	FNarrativeState State;
};
