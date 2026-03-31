#include "NarrativeNPCComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "PromptBuilder.h"

UNarrativeNPCComponent::UNarrativeNPCComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UNarrativeNPCComponent::BeginPlay()
{
	Super::BeginPlay();

	// Create interaction sphere
	AActor* Owner = GetOwner();
	if (Owner)
	{
		InteractionSphere = NewObject<USphereComponent>(Owner, TEXT("InteractionSphere"));
		if (Owner->GetRootComponent())
		{
			InteractionSphere->SetupAttachment(Owner->GetRootComponent());
		}
		InteractionSphere->SetSphereRadius(InteractionRadius);
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		InteractionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
		InteractionSphere->SetGenerateOverlapEvents(true);
		InteractionSphere->RegisterComponent();

		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &UNarrativeNPCComponent::OnSphereBeginOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &UNarrativeNPCComponent::OnSphereEndOverlap);

		UE_LOG(LogTemp, Log, TEXT("NarrativeNPC: Sphere created for %s, radius=%.0f"), *GetCharacterName(), InteractionRadius);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NarrativeNPC: No owner actor!"));
	}

	// Set up NarrativeState
	StateManager = NewObject<UNarrativeStateManager>(this);
	StateManager->SetState(TEXT("npc_attitude"), TEXT("neutral"));
}

void UNarrativeNPCComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner) return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->GetPawn()) return;

	float Distance = FVector::Dist(Owner->GetActorLocation(), PC->GetPawn()->GetActorLocation());
	bool bNowInRange = Distance <= InteractionRadius;

	if (bNowInRange && !bPlayerInRange)
	{
		bPlayerInRange = true;
		OnPlayerEnteredRange.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("NarrativeNPC: Player entered range of %s (dist=%.0f)"), *GetCharacterName(), Distance);
	}
	else if (!bNowInRange && bPlayerInRange)
	{
		bPlayerInRange = false;
		OnPlayerExitedRange.Broadcast();
		if (bInDialogue)
		{
			EndDialogue();
		}
		UE_LOG(LogTemp, Log, TEXT("NarrativeNPC: Player left range of %s"), *GetCharacterName());
	}
}

void UNarrativeNPCComponent::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == GetOwner()) return;

	UE_LOG(LogTemp, Verbose, TEXT("NarrativeNPC: Overlap begin with %s"), *OtherActor->GetName());

	// Check if the overlapping actor is a player-controlled pawn
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		bPlayerInRange = true;
		OnPlayerEnteredRange.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("NarrativeNPC: Player entered range of %s"), *GetCharacterName());
	}
}

void UNarrativeNPCComponent::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == GetOwner()) return;

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		bPlayerInRange = false;
		OnPlayerExitedRange.Broadcast();

		if (bInDialogue)
		{
			EndDialogue();
		}
		UE_LOG(LogTemp, Log, TEXT("NarrativeNPC: Player left range of %s"), *GetCharacterName());
	}
}

void UNarrativeNPCComponent::StartDialogue()
{
	if (!CharacterProfile)
	{
		UE_LOG(LogTemp, Error, TEXT("NarrativeNPC: No CharacterProfile assigned!"));
		return;
	}

	bInDialogue = true;

	// Create a new dialogue session
	DialogueSession = NewObject<UDialogueSession>(this);
	DialogueSession->Character = CharacterProfile;
	DialogueSession->SceneContext = SceneContext;

	UE_LOG(LogTemp, Log, TEXT("NarrativeNPC: Started dialogue with %s"), *GetCharacterName());
}

void UNarrativeNPCComponent::EndDialogue()
{
	bInDialogue = false;
	UE_LOG(LogTemp, Log, TEXT("NarrativeNPC: Ended dialogue with %s"), *GetCharacterName());
}

void UNarrativeNPCComponent::SendPlayerMessage(const FString& Message)
{
	if (!bInDialogue || !DialogueSession || !CharacterProfile)
	{
		UE_LOG(LogTemp, Warning, TEXT("NarrativeNPC: Cannot send message, dialogue not active"));
		return;
	}

	// Add player message to session
	DialogueSession->AddPlayerMessage(Message);

	// Build prompt
	FString SystemPrompt = UPromptBuilder::BuildSystemPrompt(CharacterProfile, SceneContext, StateManager);

	// Build LLM request
	FLLMRequest Request;
	Request.SystemPrompt = SystemPrompt;
	Request.UserMessage = Message;
	Request.MessageHistory = DialogueSession->GetMessageHistoryForLLM();
	Request.Temperature = 0.7f;

	// Send via LLMServiceSubsystem
	UGameInstance* GI = GetOwner()->GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("NarrativeNPC: No GameInstance"));
		return;
	}

	ULLMServiceSubsystem* LLMService = GI->GetSubsystem<ULLMServiceSubsystem>();
	if (!LLMService)
	{
		UE_LOG(LogTemp, Error, TEXT("NarrativeNPC: No LLMServiceSubsystem"));
		return;
	}

	FOnLLMResponseReceived Callback;
	Callback.BindDynamic(this, &UNarrativeNPCComponent::OnLLMResponse);
	LLMService->SendChatRequest(Request, Callback);

	UE_LOG(LogTemp, Log, TEXT("NarrativeNPC: Player says: %s"), *Message);
}

void UNarrativeNPCComponent::OnLLMResponse(const FLLMResponse& Response)
{
	if (Response.bSuccess)
	{
		// Add NPC response to session
		if (DialogueSession)
		{
			DialogueSession->AddNPCResponse(Response.Content, TEXT(""), Response.ResponseTimeMs);
		}

		UE_LOG(LogTemp, Log, TEXT("NarrativeNPC: %s says: %s (%.0fms)"),
			*GetCharacterName(), *Response.Content, Response.ResponseTimeMs);

		// Broadcast to UI
		OnNPCResponseReady.Broadcast(Response.Content);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NarrativeNPC: LLM Error: %s"), *Response.ErrorMessage);
		OnNPCResponseReady.Broadcast(FString::Printf(TEXT("[Error: %s]"), *Response.ErrorMessage));
	}
}

FString UNarrativeNPCComponent::GetCharacterName() const
{
	if (CharacterProfile)
	{
		return CharacterProfile->CharacterName;
	}
	return TEXT("Unknown NPC");
}

FString UNarrativeNPCComponent::GetDialogueHistoryText() const
{
	if (!DialogueSession)
	{
		return FString();
	}

	FString Result;
	for (const FDialogueEntry& Entry : DialogueSession->Entries)
	{
		Result += FString::Printf(TEXT("%s: %s\n\n"), *Entry.Speaker, *Entry.Content);
	}
	return Result;
}
