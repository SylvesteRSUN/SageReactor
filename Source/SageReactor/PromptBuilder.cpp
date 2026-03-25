#include "PromptBuilder.h"

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
