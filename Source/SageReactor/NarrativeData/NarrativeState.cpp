#include "NarrativeState.h"

void UNarrativeStateManager::SetState(const FString& Key, const FString& Value)
{
	State.StateMap.Add(Key, Value);
}

FString UNarrativeStateManager::GetState(const FString& Key) const
{
	const FString* Found = State.StateMap.Find(Key);
	return Found ? *Found : FString();
}

bool UNarrativeStateManager::HasState(const FString& Key) const
{
	return State.StateMap.Contains(Key);
}

void UNarrativeStateManager::RemoveState(const FString& Key)
{
	State.StateMap.Remove(Key);
}

void UNarrativeStateManager::ClearAllStates()
{
	State.StateMap.Empty();
}

FString UNarrativeStateManager::BuildStatePromptSection() const
{
	if (State.StateMap.Num() == 0)
	{
		return FString();
	}

	FString Result = TEXT("CURRENT WORLD STATE:\n");
	for (const auto& Pair : State.StateMap)
	{
		Result += FString::Printf(TEXT("- %s: %s\n"), *Pair.Key, *Pair.Value);
	}
	return Result;
}
