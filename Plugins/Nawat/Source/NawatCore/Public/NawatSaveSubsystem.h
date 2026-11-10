#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Interface.h"
#include "NawatSaveSubsystem.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UNawatSaveable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement on any object whose state must survive a save. Mark the properties to keep with
 * UPROPERTY(SaveGame) (or the "SaveGame" checkbox in Blueprint), then register with UNawatSaveSubsystem.
 */
class NAWATCORE_API INawatSaveable
{
	GENERATED_BODY()

public:
	/** Stable, unique name for this object across sessions. Returning None opts out of saving. */
	UFUNCTION(BlueprintNativeEvent, Category = "Nawat|Save")
	FName GetSaveKey() const;

	/** Called just before properties are written; copy runtime state into SaveGame properties here. */
	UFUNCTION(BlueprintNativeEvent, Category = "Nawat|Save")
	void OnBeforeNawatSave();

	/** Called after SaveGame properties were restored; rebuild runtime state from them here. */
	UFUNCTION(BlueprintNativeEvent, Category = "Nawat|Save")
	void OnAfterNawatLoad();
};

USTRUCT()
struct FNawatSavedObject
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<uint8> Bytes;
};

/** On-disk layout of one save slot. */
UCLASS()
class NAWATCORE_API UNawatSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	FDateTime SavedAt;

	UPROPERTY()
	FName LevelName;

	UPROPERTY()
	bool bHasCheckpoint = false;

	UPROPERTY()
	FTransform CheckpointTransform;

	UPROPERTY()
	TMap<FGameplayTag, int32> Facts;

	UPROPERTY()
	TMap<FName, FNawatSavedObject> Objects;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNawatSaveEvent, const FString&, SlotName, bool, bSuccess);

/**
 * Saves and loads the story facts, the last checkpoint and every registered INawatSaveable.
 *
 * Loading restores state only; it does not travel. The game opens GetSavedLevelName() itself, and
 * objects in that level receive their state when they register.
 */
UCLASS()
class NAWATCORE_API UNawatSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Call from BeginPlay. If a loaded save holds state for this object it is restored immediately. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Save")
	void RegisterSaveable(UObject* Saveable);

	UFUNCTION(BlueprintCallable, Category = "Nawat|Save")
	void UnregisterSaveable(UObject* Saveable);

	UFUNCTION(BlueprintCallable, Category = "Nawat|Save")
	bool SaveToSlot(const FString& SlotName = FString(TEXT("NawatAuto")));

	/** Records where the player should respawn, then saves. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Save")
	bool SaveCheckpoint(const FTransform& PlayerTransform, const FString& SlotName = FString(TEXT("NawatAuto")));

	UFUNCTION(BlueprintCallable, Category = "Nawat|Save")
	bool LoadFromSlot(const FString& SlotName = FString(TEXT("NawatAuto")));

	UFUNCTION(BlueprintPure, Category = "Nawat|Save")
	bool DoesSlotExist(const FString& SlotName = FString(TEXT("NawatAuto"))) const;

	UFUNCTION(BlueprintCallable, Category = "Nawat|Save")
	bool DeleteSlot(const FString& SlotName = FString(TEXT("NawatAuto")));

	/** Forgets the loaded save and clears all facts. Does not touch slots on disk. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Save")
	void StartNewGame();

	/** Level the current save was written in, or None if nothing is loaded. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Save")
	FName GetSavedLevelName() const;

	/** Returns false if no checkpoint was recorded. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Save")
	bool GetCheckpointTransform(FTransform& OutTransform) const;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Save")
	FNawatSaveEvent OnSaved;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Save")
	FNawatSaveEvent OnLoaded;

private:
	static FName KeyOf(const UObject* Saveable);
	static void WriteObject(UObject* Saveable, FNawatSavedObject& OutRecord);
	static void ReadObject(UObject* Saveable, const FNawatSavedObject& Record);

	/** The save last written or loaded. Also carries state of objects in levels that are not loaded now. */
	UPROPERTY()
	TObjectPtr<UNawatSaveGame> Current;

	TArray<TWeakObjectPtr<UObject>> Saveables;
};
