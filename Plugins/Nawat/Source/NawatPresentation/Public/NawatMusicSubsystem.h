#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NawatPauseSubsystem.h"
#include "NawatWorldState.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "NawatMusicSubsystem.generated.h"

class UAudioComponent;
class UNawatMusicSet;
class USoundBase;

/**
 * Background music as a state machine: one base state, plus named overrides pushed by zones,
 * cinematics and the story. The most recently pushed override wins; tracks cross-fade.
 * Lives on the game instance so music carries across level travel.
 */
UCLASS()
class NAWATPRESENTATION_API UNawatMusicSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Resolves the subsystem from any object that lives in a world. Returns null outside of gameplay. */
	static UNawatMusicSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Nawat|Music")
	void SetMusicSet(UNawatMusicSet* NewSet);

	UFUNCTION(BlueprintPure, Category = "Nawat|Music")
	UNawatMusicSet* GetMusicSet() const;

	/** Sets the base state, heard whenever no override is active. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Music")
	void SetState(FGameplayTag State);

	/** Pushing again with the same Source replaces that override and moves it to the top. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Music")
	void PushOverride(FName Source, FGameplayTag State);

	UFUNCTION(BlueprintCallable, Category = "Nawat|Music")
	void PopOverride(FName Source);

	/** The newest override, or the base state when there is none. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Music")
	FGameplayTag GetCurrentState() const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Music")
	FGameplayTag GetBaseState() const { return BaseState; }

	/** Whether the current set has a track for State. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Music")
	bool HasTrackFor(FGameplayTag State) const;

	/** Clears the base state and every override, then fades to silence. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Music")
	void StopAll(float FadeOutSeconds = 1.f);

private:
	struct FOverride
	{
		FName Source;
		FGameplayTag State;
	};

	UFUNCTION()
	void HandlePauseChanged(bool bAnyReasonActive, const FNawatPausePolicy& EffectivePolicy);

	void HandleAlertChanged(FGameplayTag Event, const FNawatEventPayload& Payload);
	void HandleCombatHoldTimer();
	void HandleRetryTimer();

	/** Makes the audible track match the current state. */
	void Refresh();
	void FadeOutCurrent(float Seconds);
	void ClearTimers();

	UPROPERTY()
	TObjectPtr<UNawatMusicSet> MusicSet;

	UPROPERTY()
	TObjectPtr<UAudioComponent> Current;

	UPROPERTY()
	TObjectPtr<USoundBase> CurrentSound;

	// Kept referenced until their fade ends.
	UPROPERTY()
	TArray<TObjectPtr<UAudioComponent>> FadingOut;

	FGameplayTag BaseState;
	TArray<FOverride> Overrides;

	/** Where the alert-driven state goes once the combat hold runs out. */
	FGameplayTag PendingAlertState;

	bool bDucked = false;
	int32 RetryCount = 0;

	FTimerHandle CombatHoldTimer;
	FTimerHandle RetryTimer;
};
