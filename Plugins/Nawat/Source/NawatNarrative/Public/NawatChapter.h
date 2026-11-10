#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NawatAction.h"
#include "NawatCondition.h"
#include "NawatChapter.generated.h"

/** One line of the objective list shown to the player. */
USTRUCT(BlueprintType)
struct NAWATNARRATIVE_API FNawatObjective
{
	GENERATED_BODY()

	/** Name used by CompleteObjective and by saves. Unique within its stage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText Text;

	/** Optional objectives do not hold the stage back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bOptional = false;

	/** The objective latches complete once these pass. Leave empty to complete it only by hand. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Objective")
	TArray<TObjectPtr<UNawatCondition>> CompleteWhen;
};

/** A conditional jump taken when a stage completes. */
USTRUCT(BlueprintType)
struct NAWATNARRATIVE_API FNawatBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Branch")
	TArray<TObjectPtr<UNawatCondition>> Conditions;

	/** Stage to jump to. None ends the chapter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Branch")
	FName TargetStageId;
};

/** One step of a chapter. */
USTRUCT(BlueprintType)
struct NAWATNARRATIVE_API FNawatStage
{
	GENERATED_BODY()

	/** Name used by GoToStage, branches and saves. Unique within its chapter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	FName Id;

	/** Header shown above the objective list. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	TArray<FNawatObjective> Objectives;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Stage")
	TArray<TObjectPtr<UNawatAction>> EnterActions;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Stage")
	TArray<TObjectPtr<UNawatAction>> ExitActions;

	/**
	 * Extra conditions besides the objectives. The stage completes when every required objective is
	 * complete and these pass. A stage with neither only advances by hand (AdvanceStage).
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Stage")
	TArray<TObjectPtr<UNawatCondition>> CompleteWhen;

	/** Checked in order when the stage completes; the first branch whose conditions pass wins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	TArray<FNawatBranch> Branches;

	/** Used when no branch is taken. None = the next stage in array order; past the last one the chapter completes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	FName NextStageId;

	/** The objective's Id, or a positional name when the Id was left empty. */
	FName GetObjectiveKey(int32 ObjectiveIndex) const;

	/** Returns INDEX_NONE when no objective has that key. */
	int32 FindObjectiveIndex(FName ObjectiveKey) const;
};

/** A chapter of the story: an ordered list of stages run by UNawatStoryDirector. */
UCLASS(BlueprintType)
class NAWATNARRATIVE_API UNawatChapter : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapter")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapter")
	FText Subtitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapter")
	TArray<FNawatStage> Stages;

	/** The stage's Id, or a positional name when the Id was left empty. None for an invalid index. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Story")
	FName GetStageKey(int32 StageIndex) const;

	/** Returns -1 when no stage has that key. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Story")
	int32 FindStageIndex(FName StageKey) const;
};
