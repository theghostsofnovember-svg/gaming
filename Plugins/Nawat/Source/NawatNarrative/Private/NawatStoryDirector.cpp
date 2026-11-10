#include "NawatStoryDirector.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NawatAction.h"
#include "NawatCondition.h"
#include "NawatLog.h"
#include "NawatTags.h"

namespace
{
	const float NawatStoryPollInterval = 0.25f;
	const int32 NawatStoryMaxEvaluationPasses = 32;
	const int32 NawatStoryMaxChainedStageChanges = 64;

	bool HasAnyCondition(const TArray<TObjectPtr<UNawatCondition>>& Conditions)
	{
		for (const UNawatCondition* Condition : Conditions)
		{
			if (Condition)
			{
				return true;
			}
		}
		return false;
	}
}

UNawatStoryDirector* UNawatStoryDirector::Get(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UNawatStoryDirector>() : nullptr;
}

void UNawatStoryDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UNawatWorldState* WorldState = Collection.InitializeDependency<UNawatWorldState>();
	UNawatSaveSubsystem* SaveSubsystem = Collection.InitializeDependency<UNawatSaveSubsystem>();

	if (WorldState)
	{
		BoundWorldState = WorldState;
		FactChangedHandle = WorldState->OnFactChangedNative.AddUObject(this, &UNawatStoryDirector::HandleFactChanged);
		WorldState->OnEvent.AddDynamic(this, &UNawatStoryDirector::HandleBusEvent);
		// OnFactsRestoredNative is deliberately left to the poll below: it fires in the middle of a load,
		// before this object's own state is restored, and evaluating then would judge the old stage by the new facts.
	}

	if (SaveSubsystem)
	{
		SaveSubsystem->RegisterSaveable(this);
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		// Covers conditions that read something other than facts.
		GameInstance->GetTimerManager().SetTimer(PollTimer, this, &UNawatStoryDirector::HandlePollTimer, NawatStoryPollInterval, true);
	}
}

void UNawatStoryDirector::Deinitialize()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		GameInstance->GetTimerManager().ClearTimer(PollTimer);

		if (UNawatSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UNawatSaveSubsystem>())
		{
			SaveSubsystem->UnregisterSaveable(this);
		}
	}

	if (UNawatWorldState* WorldState = BoundWorldState.Get())
	{
		WorldState->OnFactChangedNative.Remove(FactChangedHandle);
		WorldState->OnEvent.RemoveDynamic(this, &UNawatStoryDirector::HandleBusEvent);
	}
	BoundWorldState.Reset();
	FactChangedHandle.Reset();

	Chapter = nullptr;
	StageIndex = INDEX_NONE;
	CompletedObjectives.Reset();
	++Serial;

	Super::Deinitialize();
}

UWorld* UNawatStoryDirector::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

bool UNawatStoryDirector::StartChapter(UNawatChapter* InChapter, FName StartStageId)
{
	if (!InChapter || InChapter->Stages.Num() == 0)
	{
		UE_LOG(LogNawat, Warning, TEXT("StartChapter: %s is null or has no stages; ignored."), *GetNameSafe(InChapter));
		return false;
	}

	int32 StartIndex = 0;
	if (!StartStageId.IsNone())
	{
		StartIndex = InChapter->FindStageIndex(StartStageId);
		if (StartIndex == INDEX_NONE)
		{
			UE_LOG(LogNawat, Warning, TEXT("StartChapter: %s has no stage '%s'; ignored."), *InChapter->GetName(), *StartStageId.ToString());
			return false;
		}
	}

	const int32 MySerial = ++Serial;
	Chapter = InChapter;
	StageIndex = INDEX_NONE;
	CompletedObjectives.Reset();
	bHasPendingStage = false;

	OnChapterStarted.Broadcast(InChapter);
	BroadcastStoryEvent(NawatTags::Event_Story_ChapterStarted, InChapter, 0.f);

	// A listener may have stopped or replaced the chapter.
	if (Serial == MySerial)
	{
		SetStage(StartIndex);
	}
	return true;
}

void UNawatStoryDirector::StopChapter()
{
	++Serial;
	Chapter = nullptr;
	StageIndex = INDEX_NONE;
	CompletedObjectives.Reset();
	bHasPendingStage = false;
}

void UNawatStoryDirector::AdvanceStage()
{
	if (IsChapterActive())
	{
		CompleteCurrentStage();
	}
}

bool UNawatStoryDirector::GoToStage(FName StageId)
{
	if (!Chapter)
	{
		UE_LOG(LogNawat, Warning, TEXT("GoToStage('%s') called with no active chapter; ignored."), *StageId.ToString());
		return false;
	}

	const int32 TargetIndex = Chapter->FindStageIndex(StageId);
	if (TargetIndex == INDEX_NONE)
	{
		UE_LOG(LogNawat, Warning, TEXT("GoToStage: %s has no stage '%s'; ignored."), *Chapter->GetName(), *StageId.ToString());
		return false;
	}

	SetStage(TargetIndex);
	return true;
}

bool UNawatStoryDirector::CompleteObjective(FName ObjectiveId)
{
	if (!IsChapterActive())
	{
		return false;
	}

	const FNawatStage& Stage = Chapter->Stages[StageIndex];
	const int32 ObjectiveIndex = Stage.FindObjectiveIndex(ObjectiveId);
	if (ObjectiveIndex == INDEX_NONE)
	{
		UE_LOG(LogNawat, Warning, TEXT("CompleteObjective: the current stage has no objective '%s'; ignored."), *ObjectiveId.ToString());
		return false;
	}

	if (CompletedObjectives.Contains(ObjectiveId))
	{
		return false;
	}

	MarkObjectiveComplete(ObjectiveIndex);
	RequestEvaluate();
	return true;
}

bool UNawatStoryDirector::IsChapterActive() const
{
	return Chapter && Chapter->Stages.IsValidIndex(StageIndex);
}

UNawatChapter* UNawatStoryDirector::GetCurrentChapter() const
{
	return Chapter;
}

int32 UNawatStoryDirector::GetCurrentStageIndex() const
{
	return IsChapterActive() ? StageIndex : INDEX_NONE;
}

FName UNawatStoryDirector::GetCurrentStageId() const
{
	return IsChapterActive() ? Chapter->GetStageKey(StageIndex) : FName();
}

bool UNawatStoryDirector::GetCurrentStage(FNawatStage& OutStage) const
{
	if (!IsChapterActive())
	{
		return false;
	}

	OutStage = Chapter->Stages[StageIndex];
	return true;
}

bool UNawatStoryDirector::IsObjectiveComplete(FName ObjectiveId) const
{
	return IsChapterActive() && CompletedObjectives.Contains(ObjectiveId);
}

TArray<FNawatObjectiveState> UNawatStoryDirector::GetObjectiveStates() const
{
	TArray<FNawatObjectiveState> Result;
	if (!IsChapterActive())
	{
		return Result;
	}

	const FNawatStage& Stage = Chapter->Stages[StageIndex];
	Result.Reserve(Stage.Objectives.Num());
	for (int32 Index = 0; Index < Stage.Objectives.Num(); ++Index)
	{
		const FNawatObjective& Objective = Stage.Objectives[Index];

		FNawatObjectiveState State;
		State.Id = Stage.GetObjectiveKey(Index);
		State.Text = Objective.Text;
		State.bOptional = Objective.bOptional;
		State.bComplete = CompletedObjectives.Contains(State.Id);
		Result.Add(State);
	}
	return Result;
}

FName UNawatStoryDirector::GetSaveKey_Implementation() const
{
	return FName(TEXT("NawatStoryDirector"));
}

void UNawatStoryDirector::OnBeforeNawatSave_Implementation()
{
	if (IsChapterActive())
	{
		SavedChapter = FSoftObjectPath(Chapter.Get());
		SavedStageId = Chapter->GetStageKey(StageIndex);
	}
	else
	{
		SavedChapter.Reset();
		SavedStageId = NAME_None;
		CompletedObjectives.Reset();
	}
}

void UNawatStoryDirector::OnAfterNawatLoad_Implementation()
{
	++Serial;
	bHasPendingStage = false;

	Chapter = SavedChapter.IsNull() ? nullptr : Cast<UNawatChapter>(SavedChapter.TryLoad());
	StageIndex = Chapter ? Chapter->FindStageIndex(SavedStageId) : INDEX_NONE;

	if (!SavedChapter.IsNull() && !Chapter)
	{
		UE_LOG(LogNawat, Warning, TEXT("Saved chapter '%s' could not be loaded; no chapter is active."), *SavedChapter.ToString());
	}
	else if (Chapter && StageIndex == INDEX_NONE)
	{
		UE_LOG(LogNawat, Warning, TEXT("Saved stage '%s' no longer exists in %s; no chapter is active."), *SavedStageId.ToString(), *Chapter->GetName());
		Chapter = nullptr;
	}

	if (!Chapter)
	{
		CompletedObjectives.Reset();
	}

	// Enter actions already ran before the save was written, so the stage is restored silently.
	UNawatChapter* Restored = Chapter;
	OnStageChanged.Broadcast(Restored, StageIndex);
	if (Restored)
	{
		BroadcastStoryEvent(NawatTags::Event_Story_StageChanged, Restored, static_cast<float>(StageIndex));
	}

	RequestEvaluate();
}

void UNawatStoryDirector::HandleBusEvent(FGameplayTag Event, const FNawatEventPayload& Payload)
{
	RequestEvaluate();
}

void UNawatStoryDirector::HandleFactChanged(FGameplayTag Fact, int32 OldValue, int32 NewValue)
{
	RequestEvaluate();
}

void UNawatStoryDirector::HandlePollTimer()
{
	RequestEvaluate();
}

void UNawatStoryDirector::RequestEvaluate()
{
	if (bEvaluating)
	{
		bEvaluateDirty = true;
		return;
	}

	// SetStage evaluates once the stage change has settled.
	if (bChangingStage)
	{
		return;
	}

	bEvaluating = true;
	int32 Passes = 0;
	do
	{
		bEvaluateDirty = false;
		EvaluateOnce();
		++Passes;
	}
	while (bEvaluateDirty && Passes < NawatStoryMaxEvaluationPasses);
	bEvaluating = false;

	if (bEvaluateDirty)
	{
		bEvaluateDirty = false;
		UE_LOG(LogNawat, Warning, TEXT("Story evaluation did not settle after %d passes; check for stages or actions that feed each other."), NawatStoryMaxEvaluationPasses);
	}
}

void UNawatStoryDirector::EvaluateOnce()
{
	if (!IsChapterActive())
	{
		return;
	}

	const UNawatChapter* Active = Chapter;
	const int32 ActiveStageIndex = StageIndex;
	const int32 MySerial = Serial;

	const int32 NumObjectives = Active->Stages[ActiveStageIndex].Objectives.Num();
	for (int32 Index = 0; Index < NumObjectives; ++Index)
	{
		const FNawatStage& Stage = Active->Stages[ActiveStageIndex];
		const FNawatObjective& Objective = Stage.Objectives[Index];
		if (CompletedObjectives.Contains(Stage.GetObjectiveKey(Index)) || !HasAnyCondition(Objective.CompleteWhen))
		{
			continue;
		}

		if (UNawatCondition::AllPass(Objective.CompleteWhen, this))
		{
			MarkObjectiveComplete(Index);

			// A listener changed the stage or chapter; the next pass starts from the new state.
			if (Serial != MySerial)
			{
				bEvaluateDirty = true;
				return;
			}
		}
	}

	if (IsStageSatisfied(Active->Stages[ActiveStageIndex]))
	{
		CompleteCurrentStage();
	}
}

bool UNawatStoryDirector::IsStageSatisfied(const FNawatStage& Stage) const
{
	int32 RequiredCount = 0;
	for (int32 Index = 0; Index < Stage.Objectives.Num(); ++Index)
	{
		if (Stage.Objectives[Index].bOptional)
		{
			continue;
		}

		++RequiredCount;
		if (!CompletedObjectives.Contains(Stage.GetObjectiveKey(Index)))
		{
			return false;
		}
	}

	// Nothing to wait for means the stage is advanced by hand.
	if (RequiredCount == 0 && !HasAnyCondition(Stage.CompleteWhen))
	{
		return false;
	}

	return UNawatCondition::AllPass(Stage.CompleteWhen, this);
}

void UNawatStoryDirector::MarkObjectiveComplete(int32 ObjectiveIndex)
{
	if (!IsChapterActive())
	{
		return;
	}

	const FName ObjectiveKey = Chapter->Stages[StageIndex].GetObjectiveKey(ObjectiveIndex);
	if (ObjectiveKey.IsNone() || CompletedObjectives.Contains(ObjectiveKey))
	{
		return;
	}

	CompletedObjectives.Add(ObjectiveKey);

	UNawatChapter* Active = Chapter;
	OnObjectiveCompleted.Broadcast(Active, StageIndex, ObjectiveKey);
	BroadcastStoryEvent(NawatTags::Event_Story_ObjectiveCompleted, Active, static_cast<float>(ObjectiveIndex));
}

void UNawatStoryDirector::CompleteCurrentStage()
{
	if (!IsChapterActive())
	{
		return;
	}

	const FNawatStage& Stage = Chapter->Stages[StageIndex];

	for (const FNawatBranch& Branch : Stage.Branches)
	{
		if (!UNawatCondition::AllPass(Branch.Conditions, this))
		{
			continue;
		}

		if (Branch.TargetStageId.IsNone())
		{
			SetStage(Chapter->Stages.Num());
			return;
		}

		const int32 TargetIndex = Chapter->FindStageIndex(Branch.TargetStageId);
		if (TargetIndex != INDEX_NONE)
		{
			SetStage(TargetIndex);
			return;
		}

		UE_LOG(LogNawat, Warning, TEXT("%s: a branch targets unknown stage '%s'; skipped."), *Chapter->GetName(), *Branch.TargetStageId.ToString());
	}

	int32 NextIndex = StageIndex + 1;
	if (!Stage.NextStageId.IsNone())
	{
		const int32 TargetIndex = Chapter->FindStageIndex(Stage.NextStageId);
		if (TargetIndex != INDEX_NONE)
		{
			NextIndex = TargetIndex;
		}
		else
		{
			UE_LOG(LogNawat, Warning, TEXT("%s: NextStageId '%s' is unknown; continuing in array order."), *Chapter->GetName(), *Stage.NextStageId.ToString());
		}
	}

	SetStage(NextIndex);
}

void UNawatStoryDirector::SetStage(int32 NewStageIndex)
{
	if (bChangingStage)
	{
		// Asked for by an action of the stage being left or entered; applied once that change is done.
		PendingStageIndex = NewStageIndex;
		bHasPendingStage = true;
		return;
	}

	bChangingStage = true;
	int32 TargetIndex = NewStageIndex;
	int32 Changes = 0;
	for (;;)
	{
		bHasPendingStage = false;
		ApplyStage(TargetIndex);
		++Changes;

		if (!bHasPendingStage || !Chapter)
		{
			break;
		}
		if (Changes >= NawatStoryMaxChainedStageChanges)
		{
			UE_LOG(LogNawat, Error, TEXT("%s: stage actions keep changing the stage; stopped after %d changes."), *Chapter->GetName(), Changes);
			break;
		}
		TargetIndex = PendingStageIndex;
	}
	bHasPendingStage = false;
	bChangingStage = false;

	RequestEvaluate();
}

void UNawatStoryDirector::ApplyStage(int32 NewStageIndex)
{
	UNawatChapter* Active = Chapter;
	if (!Active)
	{
		return;
	}

	const int32 MySerial = ++Serial;

	if (Active->Stages.IsValidIndex(StageIndex))
	{
		UNawatAction::ExecuteAll(Active->Stages[StageIndex].ExitActions, this);
		if (Serial != MySerial)
		{
			return;
		}
	}

	CompletedObjectives.Reset();

	if (!Active->Stages.IsValidIndex(NewStageIndex))
	{
		Chapter = nullptr;
		StageIndex = INDEX_NONE;

		OnChapterCompleted.Broadcast(Active);
		BroadcastStoryEvent(NawatTags::Event_Story_ChapterCompleted, Active, 0.f);
		return;
	}

	StageIndex = NewStageIndex;

	OnStageChanged.Broadcast(Active, NewStageIndex);
	BroadcastStoryEvent(NawatTags::Event_Story_StageChanged, Active, static_cast<float>(NewStageIndex));
	if (Serial != MySerial)
	{
		return;
	}

	UNawatAction::ExecuteAll(Active->Stages[NewStageIndex].EnterActions, this);
}

void UNawatStoryDirector::BroadcastStoryEvent(FGameplayTag Event, UNawatChapter* Subject, float Value) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	UNawatWorldState* WorldState = GameInstance ? GameInstance->GetSubsystem<UNawatWorldState>() : nullptr;
	if (!WorldState)
	{
		return;
	}

	FNawatEventPayload Payload;
	Payload.Subject = Subject;
	Payload.Value = Value;
	WorldState->BroadcastEvent(Event, Payload);
}
