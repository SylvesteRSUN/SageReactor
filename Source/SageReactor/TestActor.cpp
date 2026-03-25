#include "TestActor.h"
#include "LLMServiceSubsystem.h"
#include "CharacterProfile.h"
#include "PromptBuilder.h"

void ATestActor::BeginPlay()
{
	Super::BeginPlay();

	ULLMServiceSubsystem* LLMService = GetGameInstance()->GetSubsystem<ULLMServiceSubsystem>();
	if (!LLMService)
	{
		UE_LOG(LogTemp, Error, TEXT("LLMServiceSubsystem not found!"));
		return;
	}

	// Create a character profile in code (normally you'd create this as a DataAsset in Content Browser)
	UCharacterProfile* Guard = NewObject<UCharacterProfile>(this);
	Guard->CharacterName = TEXT("Marcus");
	Guard->Role = TEXT("City gate guard");
	Guard->Personality = TEXT("Gruff but fair, takes his duty seriously, has a dry sense of humor");
	Guard->Background = FText::FromString(TEXT("A veteran soldier who served in the border wars. Now guards the main gate of Ironhaven."));
	Guard->CurrentGoal = TEXT("Keep the city safe and check all travelers entering the gate");
	Guard->SpeakingStyle = TEXT("Short, direct sentences. Occasional military jargon. Speaks with authority.");

	// Create dialogue session
	Session = NewObject<UDialogueSession>(this);
	Session->Character = Guard;
	Session->SceneContext = TEXT("The player approaches the main gate of Ironhaven at dusk. The guard stands blocking the entrance.");

	// Build the request using PromptBuilder
	FString SystemPrompt = UPromptBuilder::BuildSystemPrompt(Guard, Session->SceneContext);
	FString PlayerMessage = TEXT("Good evening! I'd like to enter the city, please.");

	Session->AddPlayerMessage(PlayerMessage);

	UE_LOG(LogTemp, Warning, TEXT("=== Testing CharacterProfile + PromptBuilder ==="));
	UE_LOG(LogTemp, Warning, TEXT("Character: %s (%s)"), *Guard->CharacterName, *Guard->Role);
	UE_LOG(LogTemp, Warning, TEXT("Player says: %s"), *PlayerMessage);

	FLLMRequest Request;
	Request.SystemPrompt = SystemPrompt;
	Request.UserMessage = PlayerMessage;
	Request.Temperature = 0.7f;

	FOnLLMResponseReceived Callback;
	Callback.BindDynamic(this, &ATestActor::OnLLMResponse);
	LLMService->SendChatRequest(Request, Callback);
}

void ATestActor::OnLLMResponse(const FLLMResponse& Response)
{
	if (Response.bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("=== %s (%.0fms): %s ==="),
			*Session->Character->CharacterName, Response.ResponseTimeMs, *Response.Content);

		Session->AddNPCResponse(Response.Content, TEXT(""), Response.ResponseTimeMs);

		// Print full dialogue so far
		UE_LOG(LogTemp, Warning, TEXT("--- Dialogue Export ---"));
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Session->ExportToJSON());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("=== LLM Error: %s ==="), *Response.ErrorMessage);
	}
}
