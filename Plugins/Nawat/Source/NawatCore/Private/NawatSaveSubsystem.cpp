#include "NawatSaveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NawatLog.h"
#include "NawatWorldState.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace
{
	const int32 NawatSaveUserIndex = 0;
}

void UNawatSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UNawatWorldState>();
}

FName UNawatSaveSubsystem::KeyOf(const UObject* Saveable)
{
	return INawatSaveable::Execute_GetSaveKey(Saveable);
}

void UNawatSaveSubsystem::WriteObject(UObject* Saveable, FNawatSavedObject& OutRecord)
{
	OutRecord.Bytes.Reset();
	FMemoryWriter Writer(OutRecord.Bytes, true);
	FObjectAndNameAsStringProxyArchive Archive(Writer, true);
	// Restricts serialization to properties flagged SaveGame.
	Archive.ArIsSaveGame = true;
	Saveable->Serialize(Archive);
}

void UNawatSaveSubsystem::ReadObject(UObject* Saveable, const FNawatSavedObject& Record)
{
	FMemoryReader Reader(Record.Bytes, true);
	FObjectAndNameAsStringProxyArchive Archive(Reader, true);
	Archive.ArIsSaveGame = true;
	Saveable->Serialize(Archive);
}

void UNawatSaveSubsystem::RegisterSaveable(UObject* Saveable)
{
	if (!Saveable || !Saveable->Implements<UNawatSaveable>())
	{
		UE_LOG(LogNawat, Warning, TEXT("RegisterSaveable: %s does not implement NawatSaveable; ignored."), *GetNameSafe(Saveable));
		return;
	}

	Saveables.AddUnique(TWeakObjectPtr<UObject>(Saveable));

	if (Current)
	{
		if (const FNawatSavedObject* Record = Current->Objects.Find(KeyOf(Saveable)))
		{
			ReadObject(Saveable, *Record);
			INawatSaveable::Execute_OnAfterNawatLoad(Saveable);
		}
	}
}

void UNawatSaveSubsystem::UnregisterSaveable(UObject* Saveable)
{
	Saveables.Remove(TWeakObjectPtr<UObject>(Saveable));
}

bool UNawatSaveSubsystem::SaveToSlot(const FString& SlotName)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNawatWorldState* WorldState = GameInstance ? GameInstance->GetSubsystem<UNawatWorldState>() : nullptr;
	UNawatSaveGame* Save = Cast<UNawatSaveGame>(UGameplayStatics::CreateSaveGameObject(UNawatSaveGame::StaticClass()));
	if (!WorldState || !Save)
	{
		OnSaved.Broadcast(SlotName, false);
		return false;
	}

	if (Current)
	{
		// Keep records of objects that are not alive right now, such as those in other levels.
		Save->Objects = Current->Objects;
		Save->bHasCheckpoint = Current->bHasCheckpoint;
		Save->CheckpointTransform = Current->CheckpointTransform;
	}

	Save->Facts = WorldState->GetAllFacts();
	Save->SavedAt = FDateTime::UtcNow();
	if (const UWorld* World = GameInstance->GetWorld())
	{
		Save->LevelName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	}

	Saveables.RemoveAll([](const TWeakObjectPtr<UObject>& Entry) { return !Entry.IsValid(); });
	for (const TWeakObjectPtr<UObject>& Entry : Saveables)
	{
		UObject* Saveable = Entry.Get();
		const FName Key = KeyOf(Saveable);
		if (Key.IsNone())
		{
			continue;
		}

		INawatSaveable::Execute_OnBeforeNawatSave(Saveable);
		WriteObject(Saveable, Save->Objects.FindOrAdd(Key));
	}

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(Save, SlotName, NawatSaveUserIndex);
	if (bSuccess)
	{
		Current = Save;
	}
	else
	{
		UE_LOG(LogNawat, Error, TEXT("Failed to write save slot '%s'."), *SlotName);
	}

	OnSaved.Broadcast(SlotName, bSuccess);
	return bSuccess;
}

bool UNawatSaveSubsystem::SaveCheckpoint(const FTransform& PlayerTransform, const FString& SlotName)
{
	if (!Current)
	{
		Current = Cast<UNawatSaveGame>(UGameplayStatics::CreateSaveGameObject(UNawatSaveGame::StaticClass()));
	}
	if (Current)
	{
		Current->bHasCheckpoint = true;
		Current->CheckpointTransform = PlayerTransform;
	}

	return SaveToSlot(SlotName);
}

bool UNawatSaveSubsystem::LoadFromSlot(const FString& SlotName)
{
	const UGameInstance* GameInstance = GetGameInstance();
	UNawatWorldState* WorldState = GameInstance ? GameInstance->GetSubsystem<UNawatWorldState>() : nullptr;
	UNawatSaveGame* Save = Cast<UNawatSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, NawatSaveUserIndex));
	if (!WorldState || !Save)
	{
		UE_LOG(LogNawat, Warning, TEXT("Save slot '%s' is missing or unreadable."), *SlotName);
		OnLoaded.Broadcast(SlotName, false);
		return false;
	}

	Current = Save;
	WorldState->RestoreFacts(Save->Facts);

	Saveables.RemoveAll([](const TWeakObjectPtr<UObject>& Entry) { return !Entry.IsValid(); });
	// Iterate a copy: restoring an object may register or unregister others.
	const TArray<TWeakObjectPtr<UObject>> Snapshot = Saveables;
	for (const TWeakObjectPtr<UObject>& Entry : Snapshot)
	{
		UObject* Saveable = Entry.Get();
		if (!Saveable)
		{
			continue;
		}

		if (const FNawatSavedObject* Record = Save->Objects.Find(KeyOf(Saveable)))
		{
			ReadObject(Saveable, *Record);
			INawatSaveable::Execute_OnAfterNawatLoad(Saveable);
		}
	}

	OnLoaded.Broadcast(SlotName, true);
	return true;
}

bool UNawatSaveSubsystem::DoesSlotExist(const FString& SlotName) const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, NawatSaveUserIndex);
}

bool UNawatSaveSubsystem::DeleteSlot(const FString& SlotName)
{
	return UGameplayStatics::DeleteGameInSlot(SlotName, NawatSaveUserIndex);
}

void UNawatSaveSubsystem::StartNewGame()
{
	Current = nullptr;

	const UGameInstance* GameInstance = GetGameInstance();
	if (UNawatWorldState* WorldState = GameInstance ? GameInstance->GetSubsystem<UNawatWorldState>() : nullptr)
	{
		WorldState->RestoreFacts(TMap<FGameplayTag, int32>());
	}
}

FName UNawatSaveSubsystem::GetSavedLevelName() const
{
	return Current ? Current->LevelName : NAME_None;
}

bool UNawatSaveSubsystem::GetCheckpointTransform(FTransform& OutTransform) const
{
	if (Current && Current->bHasCheckpoint)
	{
		OutTransform = Current->CheckpointTransform;
		return true;
	}
	return false;
}
