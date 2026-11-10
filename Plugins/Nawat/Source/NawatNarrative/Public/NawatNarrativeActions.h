#pragma once

#include "CoreMinimal.h"
#include "NawatAction.h"
#include "NawatNarrativeActions.generated.h"

class UNawatChapter;
class UNawatDialogue;

/** Starts a chapter on the story director, replacing the running one. */
UCLASS(meta = (DisplayName = "Start Chapter"))
class NAWATNARRATIVE_API UNawatAction_StartChapter : public UNawatAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TObjectPtr<UNawatChapter> Chapter;

	/** None = the first stage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FName StartStageId;

protected:
	virtual void Run_Implementation(UObject* Context) override;
};

/** Completes the current stage now and follows its branches. */
UCLASS(meta = (DisplayName = "Advance Stage"))
class NAWATNARRATIVE_API UNawatAction_AdvanceStage : public UNawatAction
{
	GENERATED_BODY()

protected:
	virtual void Run_Implementation(UObject* Context) override;
};

/** Jumps to a named stage of the running chapter. */
UCLASS(meta = (DisplayName = "Go To Stage"))
class NAWATNARRATIVE_API UNawatAction_GoToStage : public UNawatAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FName StageId;

protected:
	virtual void Run_Implementation(UObject* Context) override;
};

/** Completes an objective of the current stage by hand. */
UCLASS(meta = (DisplayName = "Complete Objective"))
class NAWATNARRATIVE_API UNawatAction_CompleteObjective : public UNawatAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FName ObjectiveId;

protected:
	virtual void Run_Implementation(UObject* Context) override;
};

/** Starts a dialogue, replacing the running one. */
UCLASS(meta = (DisplayName = "Start Dialogue"))
class NAWATNARRATIVE_API UNawatAction_StartDialogue : public UNawatAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TObjectPtr<UNawatDialogue> Dialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bBlockPlayerInput = true;

protected:
	virtual void Run_Implementation(UObject* Context) override;
};

/** Saves the game to a slot without moving the checkpoint. */
UCLASS(meta = (DisplayName = "Save Game"))
class NAWATNARRATIVE_API UNawatAction_SaveGame : public UNawatAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FString SlotName = TEXT("NawatAuto");

protected:
	virtual void Run_Implementation(UObject* Context) override;
};
