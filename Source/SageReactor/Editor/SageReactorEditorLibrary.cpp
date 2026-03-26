#include "SageReactorEditorLibrary.h"

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
