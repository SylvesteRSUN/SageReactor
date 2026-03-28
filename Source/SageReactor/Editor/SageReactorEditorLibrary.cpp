#include "SageReactorEditorLibrary.h"
#include "PromptBuilder.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#endif

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
	const FOnEditorLLMResponse& OnComplete)
{
	if (!Profile || !Session)
	{
		OnComplete.ExecuteIfBound(TEXT("ERROR: Profile or Session is null"));
		return;
	}

	// Add player message to session history
	Session->AddPlayerMessage(PlayerMessage);

	// Build system prompt from character profile + scene context
	FString SystemPrompt = UPromptBuilder::BuildSystemPrompt(Profile, Session->SceneContext);

	// Get conversation history for multi-turn dialogue
	TArray<FString> History = Session->GetMessageHistoryForLLM();

	UE_LOG(LogTemp, Warning, TEXT("SendDialogueRequest: Player said: %s"), *PlayerMessage);
	UE_LOG(LogTemp, Warning, TEXT("SendDialogueRequest: History has %d entries"), History.Num());

	// Send with full history
	SendEditorLLMRequestWithHistory(SystemPrompt, History, OnComplete);
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
	HttpRequest->SetURL(OllamaURL);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetTimeout(60.0f);

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("model"), ModelName);
	RootObject->SetBoolField(TEXT("stream"), false);
	RootObject->SetBoolField(TEXT("think"), false);

	// Build messages array with full history
	TArray<TSharedPtr<FJsonValue>> Messages;

	TSharedPtr<FJsonObject> SystemMsg = MakeShared<FJsonObject>();
	SystemMsg->SetStringField(TEXT("role"), TEXT("system"));
	SystemMsg->SetStringField(TEXT("content"), SystemPrompt);
	Messages.Add(MakeShared<FJsonValueObject>(SystemMsg));

	// MessageHistory alternates: [0]=user, [1]=assistant, [2]=user, ...
	for (int32 i = 0; i < MessageHistory.Num(); i++)
	{
		TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), (i % 2 == 0) ? TEXT("user") : TEXT("assistant"));
		Msg->SetStringField(TEXT("content"), MessageHistory[i]);
		Messages.Add(MakeShared<FJsonValueObject>(Msg));
	}

	RootObject->SetArrayField(TEXT("messages"), Messages);

	TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetNumberField(TEXT("temperature"), Temperature);
	Options->SetNumberField(TEXT("num_predict"), 256);
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
				OnComplete.ExecuteIfBound(TEXT("ERROR: Failed to connect to Ollama"));
				return;
			}

			FString ResponseBody = Response->GetContentAsString();
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

			if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
			{
				OnComplete.ExecuteIfBound(TEXT("ERROR: Failed to parse response"));
				return;
			}

			const TSharedPtr<FJsonObject>* MessageObj;
			if (JsonObject->TryGetObjectField(TEXT("message"), MessageObj))
			{
				FString Content = (*MessageObj)->GetStringField(TEXT("content"));
				if (Content.IsEmpty())
				{
					Content = (*MessageObj)->GetStringField(TEXT("thinking"));
				}
				UE_LOG(LogTemp, Warning, TEXT("LLM Response: %s"), *Content);
				OnComplete.ExecuteIfBound(Content);
			}
			else
			{
				OnComplete.ExecuteIfBound(TEXT("ERROR: No message in response"));
			}
		}
	);

	HttpRequest->ProcessRequest();
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
