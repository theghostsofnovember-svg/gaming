#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NawatAction.h"
#include "NawatPresentationActions.generated.h"

class ULevelSequence;
class UNawatMusicSet;

UENUM(BlueprintType)
enum class ENawatMusicOperation : uint8
{
	/** Change the base state. */
	SetBase,
	/** Force State until the override named OverrideSource is popped. */
	PushOverride,
	/** Remove the override named OverrideSource. */
	PopOverride
};

/** Changes the music state from the story. */
UCLASS(meta = (DisplayName = "Set Music State"))
class NAWATPRESENTATION_API UNawatAction_SetMusicState : public UNawatAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	ENawatMusicOperation Operation = ENawatMusicOperation::SetBase;

	/** Unused by Pop Override. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FGameplayTag State;

	/** Name shared by the push and the pop of one override. Unused by Set Base. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FName OverrideSource = TEXT("Story");

protected:
	virtual void Run_Implementation(UObject* Context) override;
};

/** Switches to another set of tracks, e.g. per chapter. */
UCLASS(meta = (DisplayName = "Set Music Set"))
class NAWATPRESENTATION_API UNawatAction_SetMusicSet : public UNawatAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TObjectPtr<UNawatMusicSet> MusicSet;

protected:
	virtual void Run_Implementation(UObject* Context) override;
};

/** Plays a level sequence as a cinematic. The story continues on NawatTags::Event_Cinematic_Finished. */
UCLASS(meta = (DisplayName = "Play Cinematic"))
class NAWATPRESENTATION_API UNawatAction_PlayCinematic : public UNawatAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TObjectPtr<ULevelSequence> Sequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bSkippable = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool bHidePlayer = false;

protected:
	virtual void Run_Implementation(UObject* Context) override;
};
