#pragma once

#include "CoreMinimal.h"
#include "NawatCondition.h"
#include "NawatNarrativeConditions.generated.h"

class UNawatChapter;
class UNawatDialogue;

/** Passes while the story director is on the named stage. */
UCLASS(meta = (DisplayName = "Stage Is"))
class NAWATNARRATIVE_API UNawatCondition_StageIs : public UNawatCondition
{
	GENERATED_BODY()

public:
	/** Optional. Leave empty to match the stage name in whichever chapter is running. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TObjectPtr<UNawatChapter> Chapter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FName StageId;

protected:
	virtual bool Test_Implementation(const UObject* Context) const override;
};

/** Passes once an objective of the current stage is complete. */
UCLASS(meta = (DisplayName = "Objective Complete"))
class NAWATNARRATIVE_API UNawatCondition_ObjectiveComplete : public UNawatCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FName ObjectiveId;

protected:
	virtual bool Test_Implementation(const UObject* Context) const override;
};

/** Passes while a dialogue is playing. */
UCLASS(meta = (DisplayName = "Dialogue Active"))
class NAWATNARRATIVE_API UNawatCondition_DialogueActive : public UNawatCondition
{
	GENERATED_BODY()

public:
	/** Optional. Leave empty to match any dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TObjectPtr<UNawatDialogue> Dialogue;

protected:
	virtual bool Test_Implementation(const UObject* Context) const override;
};
