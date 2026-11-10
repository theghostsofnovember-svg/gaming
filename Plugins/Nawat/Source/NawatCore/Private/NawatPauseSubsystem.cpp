#include "NawatPauseSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NawatLog.h"
#include "NawatTags.h"

void UNawatPauseSubsystem::PushReason(FGameplayTag Reason, const FNawatPausePolicy& Policy)
{
	if (!Reason.IsValid())
	{
		UE_LOG(LogNawat, Warning, TEXT("PushReason called with an invalid tag; ignored."));
		return;
	}

	FEntry* Existing = Entries.FindByPredicate([&Reason](const FEntry& Entry) { return Entry.Reason == Reason; });
	if (Existing)
	{
		Existing->Policy = Policy;
	}
	else
	{
		Entries.Add({ Reason, Policy });
	}

	Apply();
}

void UNawatPauseSubsystem::PopReason(FGameplayTag Reason)
{
	const int32 Removed = Entries.RemoveAll([&Reason](const FEntry& Entry) { return Entry.Reason == Reason; });
	if (Removed > 0)
	{
		Apply();
	}
}

void UNawatPauseSubsystem::ClearAllReasons()
{
	if (Entries.Num() > 0)
	{
		Entries.Reset();
		Apply();
	}
}

bool UNawatPauseSubsystem::TogglePauseMenu()
{
	if (HasReason(NawatTags::Pause_Menu))
	{
		PopReason(NawatTags::Pause_Menu);
		return false;
	}

	if (GetEffectivePolicy().bBlockPauseMenu)
	{
		return false;
	}

	FNawatPausePolicy Policy;
	Policy.bShowCursor = true;
	Policy.bDuckMusic = true;
	PushReason(NawatTags::Pause_Menu, Policy);
	return true;
}

bool UNawatPauseSubsystem::HasReason(FGameplayTag Reason) const
{
	return Entries.ContainsByPredicate([&Reason](const FEntry& Entry) { return Entry.Reason == Reason; });
}

FNawatPausePolicy UNawatPauseSubsystem::GetEffectivePolicy() const
{
	FNawatPausePolicy Result;
	Result.bPauseTime = false;
	Result.bBlockGameInput = false;

	for (const FEntry& Entry : Entries)
	{
		Result.bPauseTime |= Entry.Policy.bPauseTime;
		Result.bBlockGameInput |= Entry.Policy.bBlockGameInput;
		Result.bShowCursor |= Entry.Policy.bShowCursor;
		Result.bHideHUD |= Entry.Policy.bHideHUD;
		Result.bDuckMusic |= Entry.Policy.bDuckMusic;
		Result.bBlockPauseMenu |= Entry.Policy.bBlockPauseMenu;
	}
	return Result;
}

void UNawatPauseSubsystem::Apply()
{
	const FNawatPausePolicy Policy = GetEffectivePolicy();

	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;

	// A new world or controller starts unpaused and unblocked, so nothing of ours is applied to it yet.
	if (AppliedWorld.Get() != World)
	{
		AppliedWorld = World;
		bTimeApplied = false;
	}
	if (AppliedController.Get() != Controller)
	{
		AppliedController = Controller;
		bInputApplied = false;
		bCursorApplied = false;
	}

	if (World && Policy.bPauseTime != bTimeApplied)
	{
		if (UGameplayStatics::SetGamePaused(World, Policy.bPauseTime))
		{
			bTimeApplied = Policy.bPauseTime;
		}
	}

	if (Controller)
	{
		if (Policy.bBlockGameInput != bInputApplied)
		{
			// These are counters on the controller, so each must be paired exactly once.
			Controller->SetIgnoreMoveInput(Policy.bBlockGameInput);
			Controller->SetIgnoreLookInput(Policy.bBlockGameInput);
			bInputApplied = Policy.bBlockGameInput;
		}

		if (Policy.bShowCursor != bCursorApplied)
		{
			if (Policy.bShowCursor)
			{
				bCursorBefore = Controller->ShouldShowMouseCursor();
				Controller->SetShowMouseCursor(true);
			}
			else
			{
				Controller->SetShowMouseCursor(bCursorBefore);
			}
			bCursorApplied = Policy.bShowCursor;
		}
	}

	OnPauseChanged.Broadcast(Entries.Num() > 0, Policy);
}
