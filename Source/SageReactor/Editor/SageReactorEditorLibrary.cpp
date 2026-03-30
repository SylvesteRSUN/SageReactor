#include "SageReactorEditorLibrary.h"
#include "PromptBuilder.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/DateTime.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#endif

// Static member initialization
FLLMDebugInfo USageReactorEditorLibrary::LastDebugInfo;
ELLMProvider USageReactorEditorLibrary::EditorProvider = ELLMProvider::Ollama;
FString USageReactorEditorLibrary::EditorOllamaURL = TEXT("http://localhost:11434/api/chat");
FString USageReactorEditorLibrary::EditorModelName = TEXT("qwen3.5:9b");
FString USageReactorEditorLibrary::EditorApiKey;
FString USageReactorEditorLibrary::LastPromptCharacterSection;
FString USageReactorEditorLibrary::LastPromptStateSection;
FString USageReactorEditorLibrary::LastPromptHistorySection;

UCharacterProfile* USageReactorEditorLibrary::CreateCharacterProfile(
	const FString& Name,
	const FString& Role,
	const FString& Personality,
	const FString& Background,
	const FString& CurrentGoal,
	const FString& SpeakingStyle)
{
	UCharacterProfile* Profile = NewObject<UCharacterProfile>();
	Profile->CharacterName = Name;
	Profile->Role = Role;
	Profile->Personality = Personality;
	Profile->Background = FText::FromString(Background);
	Profile->CurrentGoal = CurrentGoal;
	Profile->SpeakingStyle = SpeakingStyle;
	return Profile;
}

bool USageReactorEditorLibrary::SaveCharacterAsset(UCharacterProfile* Profile, const FString& AssetName, const FString& FolderPath)
{
#if WITH_EDITOR
	if (!Profile)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveCharacterAsset: Profile is null"));
		return false;
	}

	// Build full package path: /Game/NarrativeData/Characters/DA_guard_marcus
	FString PackagePath = FolderPath / AssetName;
	UPackage* Package = CreatePackage(*PackagePath);
	Package->FullyLoad();

	// Duplicate the profile into the new package
	UCharacterProfile* SavedProfile = DuplicateObject<UCharacterProfile>(Profile, Package, *AssetName);
	SavedProfile->SetFlags(RF_Public | RF_Standalone);

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(SavedProfile);
	SavedProfile->MarkPackageDirty();

	// Save to disk
	FString FilePath = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	bool bSaved = UPackage::SavePackage(Package, SavedProfile, *FilePath, SaveArgs);

	if (bSaved)
	{
		UE_LOG(LogTemp, Log, TEXT("SaveCharacterAsset: Saved %s to %s"), *AssetName, *FilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SaveCharacterAsset: Failed to save %s"), *AssetName);
	}

	return bSaved;
#else
	UE_LOG(LogTemp, Error, TEXT("SaveCharacterAsset: Only available in editor"));
	return false;
#endif
}

UCharacterProfile* USageReactorEditorLibrary::LoadCharacterProfile(const FString& AssetPath)
{
	UCharacterProfile* Profile = Cast<UCharacterProfile>(
		StaticLoadObject(UCharacterProfile::StaticClass(), nullptr, *AssetPath)
	);

	if (!Profile)
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadCharacterProfile: Could not load asset at %s"), *AssetPath);
	}

	return Profile;
}

TArray<FString> USageReactorEditorLibrary::ListCharacterProfiles(const FString& FolderPath)
{
	TArray<FString> Result;

#if WITH_EDITOR
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*FolderPath), AssetDataList, /*bRecursive=*/true);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (Cast<UCharacterProfile>(AssetData.GetAsset()))
		{
			Result.Add(AssetData.GetObjectPathString());
		}
	}
#endif

	return Result;
}

TArray<FCharacterListEntry> USageReactorEditorLibrary::GetCharacterList(const FString& FolderPath)
{
	TArray<FCharacterListEntry> Result;

#if WITH_EDITOR
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByPath(FName(*FolderPath), AssetDataList, /*bRecursive=*/true);

	UE_LOG(LogTemp, Warning, TEXT("GetCharacterList: Searching in '%s', found %d assets"), *FolderPath, AssetDataList.Num());

	for (const FAssetData& AssetData : AssetDataList)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Asset: %s, Class: %s"), *AssetData.AssetName.ToString(), *AssetData.AssetClassPath.ToString());

		UCharacterProfile* Profile = Cast<UCharacterProfile>(AssetData.GetAsset());
		if (Profile)
		{
			FCharacterListEntry Entry;
			Entry.DisplayName = Profile->CharacterName.IsEmpty() ? AssetData.AssetName.ToString() : Profile->CharacterName;
			Entry.AssetPath = AssetData.GetObjectPathString();
			Result.Add(Entry);
			UE_LOG(LogTemp, Warning, TEXT("  -> Added: %s"), *Entry.DisplayName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  -> Skipped (not CharacterProfile)"));
		}
	}
#endif

	return Result;
}

UCharacterProfile* USageReactorEditorLibrary::FindCharacterByName(const FString& CharacterName, const FString& FolderPath)
{
	TArray<FCharacterListEntry> Entries = GetCharacterList(FolderPath);
	for (const FCharacterListEntry& Entry : Entries)
	{
		if (Entry.DisplayName == CharacterName)
		{
			return LoadCharacterProfile(Entry.AssetPath);
		}
	}
	return nullptr;
}

UNarrativeStateManager* USageReactorEditorLibrary::CreateNarrativeStateManager()
{
	return NewObject<UNarrativeStateManager>();
}

UDialogueSession* USageReactorEditorLibrary::CreateDialogueSession(UCharacterProfile* Profile, const FString& SceneContext)
{
	UDialogueSession* Session = NewObject<UDialogueSession>();
	Session->Character = Profile;
	Session->SceneContext = SceneContext;
	return Session;
}

void USageReactorEditorLibrary::SendDialogueRequest(
	UCharacterProfile* Profile,
	UDialogueSession* Session,
	const FString& PlayerMessage,
	const FOnEditorLLMResponse& OnComplete,
	UNarrativeStateManager* StateManager)
{
	if (!Profile || !Session)
	{
		OnComplete.ExecuteIfBound(TEXT("ERROR: Profile or Session is null"));
		return;
	}

	// Add player message to session history
	Session->AddPlayerMessage(PlayerMessage);

	// Build system prompt from character profile + scene context + narrative state
	FString SystemPrompt = UPromptBuilder::BuildSystemPrompt(Profile, Session->SceneContext, StateManager);

	// Get conversation history for multi-turn dialogue
	TArray<FString> History = Session->GetMessageHistoryForLLM();

	// Track debug info — prompt breakdown sections
	LastPromptCharacterSection = FString::Printf(
		TEXT("[Character: %s | Role: %s | Style: %s]"),
		*Profile->CharacterName, *Profile->Role, *Profile->SpeakingStyle
	);
	LastPromptStateSection = StateManager
		? StateManager->BuildStatePromptSection()
		: TEXT("(no state manager)");
	LastPromptHistorySection = FString::Printf(TEXT("[%d messages in history]"), History.Num());

	LastDebugInfo.LastSystemPrompt = SystemPrompt;
	LastDebugInfo.LastUserMessage = PlayerMessage;
	LastDebugInfo.ModelName = EditorModelName;

	UE_LOG(LogTemp, Warning, TEXT("SendDialogueRequest: Player said: %s"), *PlayerMessage);
	UE_LOG(LogTemp, Warning, TEXT("SendDialogueRequest: History has %d entries"), History.Num());

	// Send with full history using configured URL and model
	SendEditorLLMRequestWithHistory(SystemPrompt, History, OnComplete, EditorOllamaURL, EditorModelName);
}

FString USageReactorEditorLibrary::FormatDialogueHistory(const UDialogueSession* Session)
{
	if (!Session)
	{
		return FString();
	}

	FString Result;
	for (const FDialogueEntry& Entry : Session->Entries)
	{
		Result += FString::Printf(TEXT("%s: %s\n\n"), *Entry.Speaker, *Entry.Content);
	}
	return Result;
}

// --- Editor LLM ---

void USageReactorEditorLibrary::SendEditorLLMRequest(
	const FString& SystemPrompt,
	const FString& UserMessage,
	const FOnEditorLLMResponse& OnComplete,
	const FString& OllamaURL,
	const FString& ModelName,
	float Temperature)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(OllamaURL);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetTimeout(60.0f);

	// Build JSON body
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("model"), ModelName);
	RootObject->SetBoolField(TEXT("stream"), false);
	RootObject->SetBoolField(TEXT("think"), false);

	// Messages array
	TArray<TSharedPtr<FJsonValue>> Messages;

	TSharedPtr<FJsonObject> SystemMsg = MakeShared<FJsonObject>();
	SystemMsg->SetStringField(TEXT("role"), TEXT("system"));
	SystemMsg->SetStringField(TEXT("content"), SystemPrompt);
	Messages.Add(MakeShared<FJsonValueObject>(SystemMsg));

	TSharedPtr<FJsonObject> UserMsg = MakeShared<FJsonObject>();
	UserMsg->SetStringField(TEXT("role"), TEXT("user"));
	UserMsg->SetStringField(TEXT("content"), UserMessage);
	Messages.Add(MakeShared<FJsonValueObject>(UserMsg));

	RootObject->SetArrayField(TEXT("messages"), Messages);

	// Options
	TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetNumberField(TEXT("temperature"), Temperature);
	Options->SetNumberField(TEXT("num_predict"), 512);
	RootObject->SetObjectField(TEXT("options"), Options);

	FString JsonBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonBody);
	FJsonSerializer::Serialize(RootObject, Writer);
	HttpRequest->SetContentAsString(JsonBody);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			if (!bSuccess || !Response.IsValid())
			{
				OnComplete.ExecuteIfBound(TEXT("ERROR: Failed to connect to Ollama. Is it running?"));
				return;
			}

			FString ResponseBody = Response->GetContentAsString();
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

			if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
			{
				OnComplete.ExecuteIfBound(TEXT("ERROR: Failed to parse Ollama response"));
				return;
			}

			const TSharedPtr<FJsonObject>* MessageObj;
			if (JsonObject->TryGetObjectField(TEXT("message"), MessageObj))
			{
				FString Content = (*MessageObj)->GetStringField(TEXT("content"));
				if (Content.IsEmpty())
				{
					// Fallback for thinking models
					Content = (*MessageObj)->GetStringField(TEXT("thinking"));
				}
				OnComplete.ExecuteIfBound(Content);
			}
			else
			{
				OnComplete.ExecuteIfBound(TEXT("ERROR: No message in Ollama response"));
			}
		}
	);

	HttpRequest->ProcessRequest();
	UE_LOG(LogTemp, Log, TEXT("SendEditorLLMRequest: Sent request to %s"), *OllamaURL);
}

void USageReactorEditorLibrary::SendEditorLLMRequestWithHistory(
	const FString& SystemPrompt,
	const TArray<FString>& MessageHistory,
	const FOnEditorLLMResponse& OnComplete,
	const FString& OllamaURL,
	const FString& ModelName,
	float Temperature)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetTimeout(60.0f);

	FString JsonBody;
	ELLMProvider Provider = EditorProvider;

	// Build messages array (shared across Ollama/OpenAI/Anthropic)
	TArray<TSharedPtr<FJsonValue>> Messages;
	TSharedPtr<FJsonObject> SystemMsg = MakeShared<FJsonObject>();
	SystemMsg->SetStringField(TEXT("role"), TEXT("system"));
	SystemMsg->SetStringField(TEXT("content"), SystemPrompt);

	for (int32 i = 0; i < MessageHistory.Num(); i++)
	{
		TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), (i % 2 == 0) ? TEXT("user") : TEXT("assistant"));
		Msg->SetStringField(TEXT("content"), MessageHistory[i]);
		Messages.Add(MakeShared<FJsonValueObject>(Msg));
	}

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	if (Provider == ELLMProvider::Ollama)
	{
		HttpRequest->SetURL(OllamaURL);

		Messages.Insert(MakeShared<FJsonValueObject>(SystemMsg), 0);
		RootObject->SetStringField(TEXT("model"), ModelName);
		RootObject->SetBoolField(TEXT("stream"), false);
		RootObject->SetBoolField(TEXT("think"), false);
		RootObject->SetArrayField(TEXT("messages"), Messages);

		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetNumberField(TEXT("temperature"), Temperature);
		Options->SetNumberField(TEXT("num_predict"), 256);
		RootObject->SetObjectField(TEXT("options"), Options);
	}
	else if (Provider == ELLMProvider::OpenAI)
	{
		HttpRequest->SetURL(OllamaURL);
		HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *EditorApiKey));

		Messages.Insert(MakeShared<FJsonValueObject>(SystemMsg), 0);
		RootObject->SetStringField(TEXT("model"), ModelName);
		RootObject->SetArrayField(TEXT("messages"), Messages);
		RootObject->SetNumberField(TEXT("temperature"), Temperature);
		RootObject->SetNumberField(TEXT("max_tokens"), 512);
	}
	else if (Provider == ELLMProvider::Anthropic)
	{
		HttpRequest->SetURL(OllamaURL);
		HttpRequest->SetHeader(TEXT("x-api-key"), EditorApiKey);
		HttpRequest->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));

		// Anthropic: system is a top-level field, not in messages
		RootObject->SetStringField(TEXT("model"), ModelName);
		RootObject->SetStringField(TEXT("system"), SystemPrompt);
		RootObject->SetArrayField(TEXT("messages"), Messages);
		RootObject->SetNumberField(TEXT("max_tokens"), 512);
	}
	else if (Provider == ELLMProvider::Gemini)
	{
		// Gemini uses ?key= param in URL
		FString GeminiURL = FString::Printf(TEXT("%s?key=%s"), *OllamaURL, *EditorApiKey);
		HttpRequest->SetURL(GeminiURL);

		// Gemini format: contents array with parts
		TArray<TSharedPtr<FJsonValue>> Contents;
		for (int32 i = 0; i < MessageHistory.Num(); i++)
		{
			TSharedPtr<FJsonObject> Content = MakeShared<FJsonObject>();
			Content->SetStringField(TEXT("role"), (i % 2 == 0) ? TEXT("user") : TEXT("model"));

			TSharedPtr<FJsonObject> Part = MakeShared<FJsonObject>();
			Part->SetStringField(TEXT("text"), MessageHistory[i]);
			TArray<TSharedPtr<FJsonValue>> Parts;
			Parts.Add(MakeShared<FJsonValueObject>(Part));
			Content->SetArrayField(TEXT("parts"), Parts);

			Contents.Add(MakeShared<FJsonValueObject>(Content));
		}
		RootObject->SetArrayField(TEXT("contents"), Contents);

		// System instruction
		TSharedPtr<FJsonObject> SysInstruction = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> SysPart = MakeShared<FJsonObject>();
		SysPart->SetStringField(TEXT("text"), SystemPrompt);
		TArray<TSharedPtr<FJsonValue>> SysParts;
		SysParts.Add(MakeShared<FJsonValueObject>(SysPart));
		SysInstruction->SetArrayField(TEXT("parts"), SysParts);
		RootObject->SetObjectField(TEXT("systemInstruction"), SysInstruction);
	}

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonBody);
	FJsonSerializer::Serialize(RootObject, Writer);
	HttpRequest->SetContentAsString(JsonBody);

	double RequestStartTime = FPlatformTime::Seconds();

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[OnComplete, RequestStartTime, Provider](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			float ElapsedMs = (FPlatformTime::Seconds() - RequestStartTime) * 1000.0f;

			if (!bSuccess || !Response.IsValid())
			{
				LastDebugInfo.bLastRequestSuccess = false;
				LastDebugInfo.ErrorMessage = TEXT("Failed to connect to LLM provider.");
				LastDebugInfo.LastResponseTimeMs = ElapsedMs;
				OnComplete.ExecuteIfBound(TEXT("ERROR: Failed to connect to LLM provider"));
				return;
			}

			FString ResponseBody = Response->GetContentAsString();
			LastDebugInfo.LastRawJSON = ResponseBody;
			LastDebugInfo.LastResponseTimeMs = ElapsedMs;

			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

			if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
			{
				LastDebugInfo.bLastRequestSuccess = false;
				LastDebugInfo.ErrorMessage = TEXT("Failed to parse response JSON");
				OnComplete.ExecuteIfBound(TEXT("ERROR: Failed to parse response"));
				return;
			}

			FString Content;

			if (Provider == ELLMProvider::Ollama)
			{
				// Ollama: { "message": { "content": "..." } }
				const TSharedPtr<FJsonObject>* MessageObj;
				if (JsonObject->TryGetObjectField(TEXT("message"), MessageObj))
				{
					Content = (*MessageObj)->GetStringField(TEXT("content"));
					if (Content.IsEmpty())
					{
						Content = (*MessageObj)->GetStringField(TEXT("thinking"));
					}
				}
			}
			else if (Provider == ELLMProvider::OpenAI)
			{
				// OpenAI: { "choices": [{ "message": { "content": "..." } }] }
				const TArray<TSharedPtr<FJsonValue>>* Choices;
				if (JsonObject->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
				{
					const TSharedPtr<FJsonObject>* ChoiceObj;
					if ((*Choices)[0]->TryGetObject(ChoiceObj))
					{
						const TSharedPtr<FJsonObject>* MsgObj;
						if ((*ChoiceObj)->TryGetObjectField(TEXT("message"), MsgObj))
						{
							Content = (*MsgObj)->GetStringField(TEXT("content"));
						}
					}
				}
			}
			else if (Provider == ELLMProvider::Anthropic)
			{
				// Anthropic: { "content": [{ "type": "text", "text": "..." }] }
				const TArray<TSharedPtr<FJsonValue>>* ContentArray;
				if (JsonObject->TryGetArrayField(TEXT("content"), ContentArray) && ContentArray->Num() > 0)
				{
					const TSharedPtr<FJsonObject>* BlockObj;
					if ((*ContentArray)[0]->TryGetObject(BlockObj))
					{
						Content = (*BlockObj)->GetStringField(TEXT("text"));
					}
				}
			}
			else if (Provider == ELLMProvider::Gemini)
			{
				// Gemini: { "candidates": [{ "content": { "parts": [{ "text": "..." }] } }] }
				const TArray<TSharedPtr<FJsonValue>>* Candidates;
				if (JsonObject->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates->Num() > 0)
				{
					const TSharedPtr<FJsonObject>* CandObj;
					if ((*Candidates)[0]->TryGetObject(CandObj))
					{
						const TSharedPtr<FJsonObject>* ContentObj;
						if ((*CandObj)->TryGetObjectField(TEXT("content"), ContentObj))
						{
							const TArray<TSharedPtr<FJsonValue>>* PartsArr;
							if ((*ContentObj)->TryGetArrayField(TEXT("parts"), PartsArr) && PartsArr->Num() > 0)
							{
								const TSharedPtr<FJsonObject>* PartObj;
								if ((*PartsArr)[0]->TryGetObject(PartObj))
								{
									Content = (*PartObj)->GetStringField(TEXT("text"));
								}
							}
						}
					}
				}
			}

			if (!Content.IsEmpty())
			{
				LastDebugInfo.LastResponse = Content;
				LastDebugInfo.bLastRequestSuccess = true;
				LastDebugInfo.ErrorMessage = TEXT("");
				UE_LOG(LogTemp, Warning, TEXT("LLM Response (%.0fms): %s"), ElapsedMs, *Content);
				OnComplete.ExecuteIfBound(Content);
			}
			else
			{
				// Check for API error messages
				FString ErrorMsg = JsonObject->HasField(TEXT("error"))
					? JsonObject->GetObjectField(TEXT("error"))->GetStringField(TEXT("message"))
					: TEXT("No content in response");
				LastDebugInfo.bLastRequestSuccess = false;
				LastDebugInfo.ErrorMessage = ErrorMsg;
				OnComplete.ExecuteIfBound(FString::Printf(TEXT("ERROR: %s"), *ErrorMsg));
			}
		}
	);

	HttpRequest->ProcessRequest();
	UE_LOG(LogTemp, Log, TEXT("SendEditorLLMRequest: Sent to %s (Provider: %d)"), *OllamaURL, (int32)Provider);
}

// --- AI Character Generation ---

FString USageReactorEditorLibrary::BuildCharacterGenerationPrompt(const FString& Description)
{
	return FString::Printf(TEXT(
		"You are a game narrative designer assistant. Based on the user's description, "
		"generate a complete NPC character profile. Respond ONLY with a valid JSON object, "
		"no other text or explanation.\n\n"
		"The JSON must have exactly these fields:\n"
		"{\n"
		"  \"name\": \"character's full name\",\n"
		"  \"role\": \"their role in the game world (e.g. Village blacksmith, Royal advisor)\",\n"
		"  \"personality\": \"2-3 sentence personality description\",\n"
		"  \"background\": \"2-3 sentence backstory\",\n"
		"  \"goal\": \"their current motivation or objective\",\n"
		"  \"speaking_style\": \"how they talk (tone, vocabulary, speech patterns)\"\n"
		"}\n\n"
		"User description: %s"
	), *Description);
}

bool USageReactorEditorLibrary::ParseGeneratedCharacter(
	const FString& JSONResponse,
	FString& OutName,
	FString& OutRole,
	FString& OutPersonality,
	FString& OutBackground,
	FString& OutGoal,
	FString& OutSpeakingStyle)
{
	// Try to extract JSON from the response (LLM might wrap it in markdown code blocks)
	FString CleanJSON = JSONResponse;

	// Remove markdown code block if present
	int32 JsonStart = CleanJSON.Find(TEXT("{"));
	int32 JsonEnd = CleanJSON.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	if (JsonStart == INDEX_NONE || JsonEnd == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("ParseGeneratedCharacter: No JSON object found in response"));
		return false;
	}

	CleanJSON = CleanJSON.Mid(JsonStart, JsonEnd - JsonStart + 1);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CleanJSON);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("ParseGeneratedCharacter: Failed to parse JSON"));
		return false;
	}

	OutName = JsonObject->GetStringField(TEXT("name"));
	OutRole = JsonObject->GetStringField(TEXT("role"));
	OutPersonality = JsonObject->GetStringField(TEXT("personality"));
	OutBackground = JsonObject->GetStringField(TEXT("background"));
	OutGoal = JsonObject->GetStringField(TEXT("goal"));
	OutSpeakingStyle = JsonObject->GetStringField(TEXT("speaking_style"));

	UE_LOG(LogTemp, Log, TEXT("ParseGeneratedCharacter: Parsed character '%s'"), *OutName);
	return true;
}

// --- Connection Settings ---

void USageReactorEditorLibrary::SetEditorProvider(ELLMProvider Provider)
{
	EditorProvider = Provider;
	UE_LOG(LogTemp, Log, TEXT("SageReactor: Provider set to %d"), (int32)Provider);
}

ELLMProvider USageReactorEditorLibrary::GetEditorProvider()
{
	return EditorProvider;
}

void USageReactorEditorLibrary::SetEditorOllamaURL(const FString& NewURL)
{
	EditorOllamaURL = NewURL;
	UE_LOG(LogTemp, Log, TEXT("SageReactor: API URL set to %s"), *NewURL);
}

FString USageReactorEditorLibrary::GetEditorOllamaURL()
{
	return EditorOllamaURL;
}

void USageReactorEditorLibrary::SetEditorModelName(const FString& NewModelName)
{
	EditorModelName = NewModelName;
	UE_LOG(LogTemp, Log, TEXT("SageReactor: Model set to %s"), *NewModelName);
}

FString USageReactorEditorLibrary::GetEditorModelName()
{
	return EditorModelName;
}

void USageReactorEditorLibrary::SetEditorApiKey(const FString& NewApiKey)
{
	EditorApiKey = NewApiKey;
	UE_LOG(LogTemp, Log, TEXT("SageReactor: API key updated"));
}

FString USageReactorEditorLibrary::GetDefaultURLForProvider(ELLMProvider Provider)
{
	switch (Provider)
	{
	case ELLMProvider::Ollama:    return TEXT("http://localhost:11434/api/chat");
	case ELLMProvider::OpenAI:   return TEXT("https://api.openai.com/v1/chat/completions");
	case ELLMProvider::Anthropic: return TEXT("https://api.anthropic.com/v1/messages");
	case ELLMProvider::Gemini:   return TEXT("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent");
	default:                      return TEXT("http://localhost:11434/api/chat");
	}
}

FString USageReactorEditorLibrary::GetDefaultModelForProvider(ELLMProvider Provider)
{
	switch (Provider)
	{
	case ELLMProvider::Ollama:    return TEXT("qwen3.5:9b");
	case ELLMProvider::OpenAI:   return TEXT("gpt-4o-mini");
	case ELLMProvider::Anthropic: return TEXT("claude-sonnet-4-20250514");
	case ELLMProvider::Gemini:   return TEXT("gemini-2.0-flash");
	default:                      return TEXT("qwen3.5:9b");
	}
}

void USageReactorEditorLibrary::TestConnection(const FOnConnectionTestResult& OnResult)
{
	// Ping Ollama by requesting /api/tags (lightweight endpoint)
	FString BaseURL = EditorOllamaURL;
	// Extract base URL from chat endpoint
	BaseURL = BaseURL.Replace(TEXT("/api/chat"), TEXT("/api/tags"));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(BaseURL);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetTimeout(5.0f);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[OnResult](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			if (!bSuccess || !Response.IsValid())
			{
				OnResult.ExecuteIfBound(false, TEXT("Cannot connect to Ollama. Is it running?"));
				return;
			}

			if (Response->GetResponseCode() == 200)
			{
				OnResult.ExecuteIfBound(true, TEXT("Connected to Ollama successfully."));
			}
			else
			{
				OnResult.ExecuteIfBound(false, FString::Printf(TEXT("Ollama returned HTTP %d"), Response->GetResponseCode()));
			}
		}
	);

	HttpRequest->ProcessRequest();
}

void USageReactorEditorLibrary::GetAvailableModels(const FOnModelsReceived& OnResult)
{
	FString BaseURL = EditorOllamaURL;
	BaseURL = BaseURL.Replace(TEXT("/api/chat"), TEXT("/api/tags"));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(BaseURL);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetTimeout(10.0f);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[OnResult](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			TArray<FString> ModelNames;

			if (!bSuccess || !Response.IsValid())
			{
				OnResult.ExecuteIfBound(ModelNames);
				return;
			}

			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* ModelsArray;
				if (JsonObject->TryGetArrayField(TEXT("models"), ModelsArray))
				{
					for (const TSharedPtr<FJsonValue>& ModelValue : *ModelsArray)
					{
						const TSharedPtr<FJsonObject>* ModelObj;
						if (ModelValue->TryGetObject(ModelObj))
						{
							FString Name = (*ModelObj)->GetStringField(TEXT("name"));
							if (!Name.IsEmpty())
							{
								ModelNames.Add(Name);
							}
						}
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("SageReactor: Found %d available models"), ModelNames.Num());
			OnResult.ExecuteIfBound(ModelNames);
		}
	);

	HttpRequest->ProcessRequest();
}

// --- Debug / Diagnostics ---

FLLMDebugInfo USageReactorEditorLibrary::GetLastDebugInfo()
{
	return LastDebugInfo;
}

FString USageReactorEditorLibrary::GetPromptBreakdown()
{
	FString Result;
	Result += TEXT("=== PROMPT BREAKDOWN ===\n\n");
	Result += TEXT("--- Character Profile ---\n");
	Result += LastPromptCharacterSection + TEXT("\n\n");
	Result += TEXT("--- Narrative State ---\n");
	Result += LastPromptStateSection + TEXT("\n\n");
	Result += TEXT("--- Dialogue History ---\n");
	Result += LastPromptHistorySection + TEXT("\n\n");
	Result += TEXT("--- Full System Prompt ---\n");
	Result += LastDebugInfo.LastSystemPrompt + TEXT("\n");
	return Result;
}

FString USageReactorEditorLibrary::GetNarrativeStateDebugText(const UNarrativeStateManager* StateManager)
{
	if (!StateManager)
	{
		return TEXT("No NarrativeStateManager available.");
	}

	const FNarrativeState& State = StateManager->GetNarrativeState();
	if (State.StateMap.Num() == 0)
	{
		return TEXT("(empty - no state keys set)");
	}

	FString Result = TEXT("Current Narrative State:\n");
	for (const auto& Pair : State.StateMap)
	{
		Result += FString::Printf(TEXT("  %s = %s\n"), *Pair.Key, *Pair.Value);
	}
	return Result;
}
