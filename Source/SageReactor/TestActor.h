// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestActor.generated.h"

UCLASS()
class SAGEREACTOR_API ATestActor : public AActor
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

};
