#include "NawatChapter.h"

FName FNawatStage::GetObjectiveKey(int32 ObjectiveIndex) const
{
	if (!Objectives.IsValidIndex(ObjectiveIndex))
	{
		return NAME_None;
	}

	const FName ObjectiveId = Objectives[ObjectiveIndex].Id;
	return ObjectiveId.IsNone() ? FName(*FString::Printf(TEXT("Objective_%d"), ObjectiveIndex)) : ObjectiveId;
}

int32 FNawatStage::FindObjectiveIndex(FName ObjectiveKey) const
{
	if (ObjectiveKey.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		if (GetObjectiveKey(Index) == ObjectiveKey)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FName UNawatChapter::GetStageKey(int32 StageIndex) const
{
	if (!Stages.IsValidIndex(StageIndex))
	{
		return NAME_None;
	}

	const FName StageId = Stages[StageIndex].Id;
	return StageId.IsNone() ? FName(*FString::Printf(TEXT("Stage_%d"), StageIndex)) : StageId;
}

int32 UNawatChapter::FindStageIndex(FName StageKey) const
{
	if (StageKey.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Stages.Num(); ++Index)
	{
		if (GetStageKey(Index) == StageKey)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
