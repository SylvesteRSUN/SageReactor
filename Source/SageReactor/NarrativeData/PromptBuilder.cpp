#include "PromptBuilder.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FString UPromptBuilder::PromptTemplate = TEXT(
	"You are a narrative designer's assistant generating in-character dialogue.\n"
	"\n"
	"CHARACTER PROFILE:\n"
	"- Name: {Name}\n"
	"- Role: {Role}\n"
	"- Personality: {Personality}\n"
	"- Background: {Background}\n"
	"- Current Goal: {Goal}\n"
	"- Speaking Style: {SpeakingStyle}\n"
	"\n"
	"SCENE CONTEXT:\n"
	"{SceneContext}\n"
	"\n"
	"RULES:\n"
	"- Stay strictly in character\n"
	"- Keep responses to 1-3 sentences unless asked for more\n"
	"- Match the speaking style defined above\n"
	"- Do not break character or reference being an AI\n"
	"- If the player says something unrelated, deflect naturally in character\n"
	"\n"
	"Respond to the player's dialogue as {Name}."
);

FString UPromptBuilder::BuildSystemPrompt(const UCharacterProfile* Profile, const FString& SceneContext)
{
	if (!Profile)
	{
		return TEXT("You are a helpful NPC. Respond in character.");
	}

	FString Result = PromptTemplate;
	Result = Result.Replace(TEXT("{Name}"), *Profile->CharacterName);
	Result = Result.Replace(TEXT("{Role}"), *Profile->Role);
	Result = Result.Replace(TEXT("{Personality}"), *Profile->Personality);
	Result = Result.Replace(TEXT("{Background}"), *Profile->Background.ToString());
	Result = Result.Replace(TEXT("{Goal}"), *Profile->CurrentGoal);
	Result = Result.Replace(TEXT("{SpeakingStyle}"), *Profile->SpeakingStyle);
	Result = Result.Replace(TEXT("{SceneContext}"), *SceneContext);

	return Result;
}

FString UPromptBuilder::GetDefaultPromptTemplate()
{
	return PromptTemplate;
}

void UPromptBuilder::SetPromptTemplate(const FString& NewTemplate)
{
	PromptTemplate = NewTemplate;
}

FString UPromptBuilder::BuildChatMessages(const UDialogueSession* Session)
{
	if (!Session)
	{
		return TEXT("[]");
	}

	TArray<TSharedPtr<FJsonValue>> MessagesArray;

	// Add system prompt if character profile exists
	if (Session->Character)
	{
		TSharedPtr<FJsonObject> SystemMsg = MakeShared<FJsonObject>();
		SystemMsg->SetStringField(TEXT("role"), TEXT("system"));
		SystemMsg->SetStringField(TEXT("content"), BuildSystemPrompt(Session->Character, Session->SceneContext));
		MessagesArray.Add(MakeShared<FJsonValueObject>(SystemMsg));
	}

	// Add dialogue entries as user/assistant messages
	for (const FDialogueEntry& Entry : Session->Entries)
	{
		TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), Entry.Speaker == TEXT("Player") ? TEXT("user") : TEXT("assistant"));
		Msg->SetStringField(TEXT("content"), Entry.Content);
		MessagesArray.Add(MakeShared<FJsonValueObject>(Msg));
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(MessagesArray, Writer);
	return OutputString;
}
