#include "TestActor.h"
#include "LLMServiceSubsystem.h"
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

	// Use editor-assigned DataAsset, or create a fallback in code
	UCharacterProfile* Profile = CharacterProfile;
	if (!Profile)
	{
		UE_LOG(LogTemp, Warning, TEXT("No CharacterProfile assigned, using default Marcus"));
		Profile = NewObject<UCharacterProfile>(this);
		Profile->CharacterName = TEXT("Marcus");
		Profile->Role = TEXT("City gate guard");
		Profile->Personality = TEXT("Gruff but fair, takes his duty seriously, has a dry sense of humor");
		Profile->Background = FText::FromString(TEXT("A veteran soldier who served in the border wars. Now guards the main gate of Ironhaven."));
		Profile->CurrentGoal = TEXT("Keep the city safe and check all travelers entering the gate");
		Profile->SpeakingStyle = TEXT("Short, direct sentences. Occasional military jargon. Speaks with authority.");
	}

	// Create dialogue session
	Session = NewObject<UDialogueSession>(this);
	Session->Character = Profile;
	Session->SceneContext = SceneContext;
	Session->AddPlayerMessage(PlayerMessage);

	// Build request
	FString SystemPrompt = UPromptBuilder::BuildSystemPrompt(Profile, SceneContext);

	UE_LOG(LogTemp, Warning, TEXT("=== Character: %s (%s) ==="), *Profile->CharacterName, *Profile->Role);
	UE_LOG(LogTemp, Warning, TEXT("=== Player: %s ==="), *PlayerMessage);

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
		UE_LOG(LogTemp, Warning, TEXT("--- Dialogue Export ---"));
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Session->ExportToJSON());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("=== LLM Error: %s ==="), *Response.ErrorMessage);
	}
}
