#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NawatChapter.h"
#include "NawatSaveSubsystem.h"
#include "NawatWorldState.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"
#include "NawatStoryDirector.generated.h"

/** One objective of the current stage with its completion state, ready for UI. */
USTRUCT(BlueprintType)
struct NAWATNARRATIVE_API FNawatObjectiveState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	bool bOptional = false;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	bool bComplete = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNawatChapterEvent, UNawatChapter*, Chapter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNawatStageChanged, UNawatChapter*, Chapter, int32, StageIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNawatObjectiveCompleted, UNawatChapter*, Chapter, int32, StageIndex, FName, ObjectiveId);

/**
 * Runs one chapter at a time: tracks the current stage, latches objectives, takes branches and
 * mirrors every change on the event bus (NawatTags::Event_Story_*).
 *
 * Conditions and actions receive the director as their context.
 */
UCLASS()
class NAWATNARRATIVE_API UNawatStoryDirector : public UGameInstanceSubsystem, public INawatSaveable
{
	GENERATED_BODY()

public:
	/** Resolves the subsystem from any object that lives in a world. Returns null outside of gameplay. */
	static UNawatStoryDirector* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual UWorld* GetWorld() const override;

	// ---- Control ----

	/** Replaces the running chapter, if any. StartStageId None = the first stage. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Story")
	bool StartChapter(UNawatChapter* InChapter, FName StartStageId = NAME_None);

	/** Abandons the chapter without running exit actions or reporting completion. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Story")
	void StopChapter();

	/** Completes the current stage now, whatever its objectives say, then follows its branches. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Story")
	void AdvanceStage();

	/** Jumps straight to a stage, ignoring branches. Returns false if the chapter has no such stage. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Story")
	bool GoToStage(FName StageId);

	/** Completes an objective of the current stage by hand. Returns false if it is unknown or already complete. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Story")
	bool CompleteObjective(FName ObjectiveId);

	// ---- State ----

	UFUNCTION(BlueprintPure, Category = "Nawat|Story")
	bool IsChapterActive() const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Story")
	UNawatChapter* GetCurrentChapter() const;

	/** -1 when no chapter is active. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Story")
	int32 GetCurrentStageIndex() const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Story")
	FName GetCurrentStageId() const;

	/** Returns false when no chapter is active. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Story")
	bool GetCurrentStage(FNawatStage& OutStage) const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Story")
	bool IsObjectiveComplete(FName ObjectiveId) const;

	/** Objectives of the current stage in authoring order. Empty when no chapter is active. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Story")
	TArray<FNawatObjectiveState> GetObjectiveStates() const;

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Story")
	FNawatChapterEvent OnChapterStarted;

	/** Also fires after a save was loaded, so UI refreshes. Chapter may be null then. */
	UPROPERTY(BlueprintAssignable, Category = "Nawat|Story")
	FNawatStageChanged OnStageChanged;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Story")
	FNawatObjectiveCompleted OnObjectiveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Story")
	FNawatChapterEvent OnChapterCompleted;

	// ---- INawatSaveable ----

	virtual FName GetSaveKey_Implementation() const override;
	virtual void OnBeforeNawatSave_Implementation() override;
	virtual void OnAfterNawatLoad_Implementation() override;

private:
	UFUNCTION()
	void HandleBusEvent(FGameplayTag Event, const FNawatEventPayload& Payload);

	void HandleFactChanged(FGameplayTag Fact, int32 OldValue, int32 NewValue);
	void HandlePollTimer();

	/** Evaluates until nothing changes. Calls made while already evaluating only mark it dirty. */
	void RequestEvaluate();
	void EvaluateOnce();

	bool IsStageSatisfied(const FNawatStage& Stage) const;
	void MarkObjectiveComplete(int32 ObjectiveIndex);
	void CompleteCurrentStage();

	/** Changes stage; an index past the last stage completes the chapter. Requests made by stage actions are queued. */
	void SetStage(int32 NewStageIndex);
	void ApplyStage(int32 NewStageIndex);

	void BroadcastStoryEvent(FGameplayTag Event, UNawatChapter* Subject, float Value) const;

	UPROPERTY()
	TObjectPtr<UNawatChapter> Chapter;

	int32 StageIndex = INDEX_NONE;

	UPROPERTY(SaveGame)
	FSoftObjectPath SavedChapter;

	UPROPERTY(SaveGame)
	FName SavedStageId;

	/** Objective keys latched in the current stage. */
	UPROPERTY(SaveGame)
	TArray<FName> CompletedObjectives;

	// Bumped whenever the chapter or stage is replaced, so code that ran actions or broadcasts
	// can tell that the ground moved under it.
	int32 Serial = 0;

	bool bEvaluating = false;
	bool bEvaluateDirty = false;
	bool bChangingStage = false;
	bool bHasPendingStage = false;
	int32 PendingStageIndex = INDEX_NONE;

	TWeakObjectPtr<UNawatWorldState> BoundWorldState;
	FDelegateHandle FactChangedHandle;
	FTimerHandle PollTimer;
};
