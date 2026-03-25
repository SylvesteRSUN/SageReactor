#include "DialogueSession.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void UDialogueSession::AddPlayerMessage(const FString& Message)
{
	FDialogueEntry Entry;
	Entry.Speaker = TEXT("Player");
	Entry.Content = Message;
	Entry.Timestamp = FDateTime::Now();
	Entries.Add(Entry);
}

void UDialogueSession::AddNPCResponse(const FString& Response, const FString& RawPrompt, float ResponseTime)
{
	FDialogueEntry Entry;
	Entry.Speaker = Character ? Character->CharacterName : TEXT("NPC");
	Entry.Content = Response;
	Entry.Timestamp = FDateTime::Now();
	Entry.RawPrompt = RawPrompt;
	Entry.ResponseTimeMs = ResponseTime;
	Entries.Add(Entry);
}

TArray<FString> UDialogueSession::GetMessageHistoryForLLM() const
{
	TArray<FString> History;
	for (const FDialogueEntry& Entry : Entries)
	{
		History.Add(Entry.Content);
	}
	return History;
}

void UDialogueSession::ClearHistory()
{
	Entries.Empty();
}

FString UDialogueSession::ExportToJSON() const
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	if (Character)
	{
		RootObject->SetStringField(TEXT("character"), Character->CharacterName);
	}
	RootObject->SetStringField(TEXT("sceneContext"), SceneContext);

	TArray<TSharedPtr<FJsonValue>> EntriesArray;
	for (const FDialogueEntry& Entry : Entries)
	{
		TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
		EntryObj->SetStringField(TEXT("speaker"), Entry.Speaker);
		EntryObj->SetStringField(TEXT("content"), Entry.Content);
		EntryObj->SetStringField(TEXT("timestamp"), Entry.Timestamp.ToString());
		EntryObj->SetNumberField(TEXT("responseTimeMs"), Entry.ResponseTimeMs);
		EntriesArray.Add(MakeShared<FJsonValueObject>(EntryObj));
	}
	RootObject->SetArrayField(TEXT("entries"), EntriesArray);

	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);
	return OutputString;
}
