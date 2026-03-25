// Fill out your copyright notice in the Description page of Project Settings.


#include "TestActor.h"

// Called when the game starts or when spawned
void ATestActor::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("=== SageReactor: Hello from C++! ==="));
}

