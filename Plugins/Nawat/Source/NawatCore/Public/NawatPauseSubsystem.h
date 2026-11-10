#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NawatPauseSubsystem.generated.h"

class APlayerController;

/** What one reason for holding the game needs. Active reasons are OR-ed together. */
USTRUCT(BlueprintType)
struct NAWATCORE_API FNawatPausePolicy
{
	GENERATED_BODY()

	/** Stop world time. Off for cinematics and walk-and-talk dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bPauseTime = true;

	/** Ignore move and look input on the player controller. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bBlockGameInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bShowCursor = false;

	/** Read by the HUD layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bHideHUD = false;

	/** Read by the music layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bDuckMusic = false;

	/** While active, TogglePauseMenu refuses to open the menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bBlockPauseMenu = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNawatPauseChanged, bool, bAnyReasonActive, const FNawatPausePolicy&, EffectivePolicy);

/**
 * Single owner of "the game is held". Systems push and pop a named reason instead of pausing directly,
 * so a pause menu opened over a cinematic, or a dialogue ending while the menu is open, cannot leave
 * the game stuck paused or wrongly resumed.
 */
UCLASS()
class NAWATCORE_API UNawatPauseSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Adds a reason, or updates its policy if it is already active. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Pause", meta = (AutoCreateRefTerm = "Policy"))
	void PushReason(FGameplayTag Reason, const FNawatPausePolicy& Policy);

	UFUNCTION(BlueprintCallable, Category = "Nawat|Pause")
	void PopReason(FGameplayTag Reason);

	/** Drops every reason. For returning to the main menu or restarting a level. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Pause")
	void ClearAllReasons();

	/**
	 * Opens or closes the pause menu reason (Nawat.Pause.Menu). Returns whether the menu is open afterwards.
	 * The input action bound to this must be allowed to trigger while the game is paused.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Pause")
	bool TogglePauseMenu();

	UFUNCTION(BlueprintPure, Category = "Nawat|Pause")
	bool HasReason(FGameplayTag Reason) const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Pause")
	bool IsAnyReasonActive() const { return Entries.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "Nawat|Pause")
	FNawatPausePolicy GetEffectivePolicy() const;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Pause")
	FNawatPauseChanged OnPauseChanged;

private:
	struct FEntry
	{
		FGameplayTag Reason;
		FNawatPausePolicy Policy;
	};

	void Apply();

	TArray<FEntry> Entries;

	// What this subsystem itself changed, so it only ever undoes its own changes.
	TWeakObjectPtr<UWorld> AppliedWorld;
	TWeakObjectPtr<APlayerController> AppliedController;
	bool bTimeApplied = false;
	bool bInputApplied = false;
	bool bCursorApplied = false;
	bool bCursorBefore = false;
};
