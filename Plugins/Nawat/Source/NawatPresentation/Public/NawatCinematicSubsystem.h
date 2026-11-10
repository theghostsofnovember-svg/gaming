#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NawatCinematicSubsystem.generated.h"

class ALevelSequenceActor;
class APawn;
class ULevelSequence;
class ULevelSequencePlayer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNawatCinematicStarted, ULevelSequence*, Sequence);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNawatCinematicFinished, ULevelSequence*, Sequence, bool, bSkipped);

/**
 * Plays one level sequence at a time as a cinematic: blocks move/look input, hides the HUD, switches
 * the music if the set has a cinematic track, and reports start and finish on the event bus
 * (NawatTags::Event_Cinematic_*) so the story can continue.
 */
UCLASS()
class NAWATPRESENTATION_API UNawatCinematicSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Resolves the subsystem from any object that lives in a game world. Returns null otherwise. */
	static UNawatCinematicSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	/** Finishes the running cinematic first, if any. Returns false if the sequence could not be started. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Cinematic")
	bool Play(ULevelSequence* Sequence, bool bSkippable = true, bool bHidePlayer = false);

	/** Jumps to the end and finishes. Does nothing when the cinematic is not skippable. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Cinematic")
	void Skip();

	UFUNCTION(BlueprintPure, Category = "Nawat|Cinematic")
	bool IsPlaying() const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Cinematic")
	bool CanSkip() const;

	/** Null when nothing is playing. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Cinematic")
	ULevelSequence* GetCurrentSequence() const;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Cinematic")
	FNawatCinematicStarted OnCinematicStarted;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Cinematic")
	FNawatCinematicFinished OnCinematicFinished;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UFUNCTION()
	void HandleSequenceFinished();

	/** Stops the sequence at its end frame, then finishes as skipped. */
	void StopCurrent();

	/** Undoes everything Play applied and reports the result. Safe to call twice. */
	void Finish(bool bSkipped);

	/** Pops the pause reason and music override, shows the player again and lets go of the sequence actor. */
	void ReleaseHolds(bool bDestroyActorNow);

	UPROPERTY()
	TObjectPtr<ULevelSequence> CurrentSequence;

	UPROPERTY()
	TObjectPtr<ULevelSequencePlayer> SequencePlayer;

	UPROPERTY()
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	TWeakObjectPtr<APawn> HiddenPawn;

	bool bPlaying = false;
	bool bCurrentSkippable = false;
	bool bMusicPushed = false;
};
