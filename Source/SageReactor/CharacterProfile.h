#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterProfile.generated.h"

UCLASS(BlueprintType)
class SAGEREACTOR_API UCharacterProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString Role;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
	FString Personality;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Background", meta = (MultiLine = true))
	FText Background;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	FString CurrentGoal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	FString SpeakingStyle;
};
