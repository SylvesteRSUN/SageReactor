// Fill out your copyright notice in the Description page of Project Settings.

#include "TestActor.h"
#include "LLMServiceSubsystem.h"

void ATestActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("=== SageReactor: Sending LLM test request... ==="));

	ULLMServiceSubsystem* LLMService = GetGameInstance()->GetSubsystem<ULLMServiceSubsystem>();
	if (!LLMService)
	{
		UE_LOG(LogTemp, Error, TEXT("LLMServiceSubsystem not found!"));
		return;
	}

	// Build a test request
	FLLMRequest Request;
	Request.SystemPrompt = TEXT("You are a helpful assistant. Reply in one sentence.");
	Request.UserMessage = TEXT("Hello, who are you?");
	Request.Temperature = 0.7f;

	// Send request
	FOnLLMResponseReceived Callback;
	Callback.BindDynamic(this, &ATestActor::OnLLMResponse);
	LLMService->SendChatRequest(Request, Callback);
}

void ATestActor::OnLLMResponse(const FLLMResponse& Response)
{
	if (Response.bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("=== LLM Response (%.0fms): %s ==="), Response.ResponseTimeMs, *Response.Content);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("=== LLM Error: %s ==="), *Response.ErrorMessage);
	}

	// Also print raw JSON for debugging
	if (!Response.RawJSON.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("Raw JSON: %s"), *Response.RawJSON);
	}
}
