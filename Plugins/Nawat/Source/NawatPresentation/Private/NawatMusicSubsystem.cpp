#include "NawatMusicSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NawatLog.h"
#include "NawatMusicSet.h"
#include "NawatSettings.h"
#include "NawatTags.h"
#include "Sound/SoundBase.h"

namespace
{
	const int32 MaxPlayRetries = 10;
	const float PlayRetrySeconds = 1.f;
	const float DuckSeconds = 0.3f;
}

UNawatMusicSubsystem* UNawatMusicSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UNawatMusicSubsystem>() : nullptr;
}

void UNawatMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UNawatWorldState* WorldState = Cast<UNawatWorldState>(Collection.InitializeDependency(UNawatWorldState::StaticClass()));
	UNawatPauseSubsystem* Pause = Cast<UNawatPauseSubsystem>(Collection.InitializeDependency(UNawatPauseSubsystem::StaticClass()));

	if (WorldState)
	{
		WorldState->OnEventNative(NawatTags::Event_AI_AlertChanged).AddUObject(this, &UNawatMusicSubsystem::HandleAlertChanged);
	}
	if (Pause)
	{
		Pause->OnPauseChanged.AddDynamic(this, &UNawatMusicSubsystem::HandlePauseChanged);
	}

	if (GetDefault<UNawatSettings>()->bAutoMusicFromAlert)
	{
		BaseState = NawatTags::Music_Explore;
	}
}

void UNawatMusicSubsystem::Deinitialize()
{
	ClearTimers();

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UNawatWorldState* WorldState = GameInstance->GetSubsystem<UNawatWorldState>())
		{
			WorldState->OnEventNative(NawatTags::Event_AI_AlertChanged).RemoveAll(this);
		}
		if (UNawatPauseSubsystem* Pause = GameInstance->GetSubsystem<UNawatPauseSubsystem>())
		{
			Pause->OnPauseChanged.RemoveDynamic(this, &UNawatMusicSubsystem::HandlePauseChanged);
		}
	}

	if (Current)
	{
		Current->Stop();
	}
	for (UAudioComponent* Component : FadingOut)
	{
		if (IsValid(Component))
		{
			Component->Stop();
		}
	}
	Current = nullptr;
	CurrentSound = nullptr;
	FadingOut.Reset();

	Super::Deinitialize();
}

void UNawatMusicSubsystem::SetMusicSet(UNawatMusicSet* NewSet)
{
	if (MusicSet == NewSet)
	{
		return;
	}

	MusicSet = NewSet;
	RetryCount = 0;
	Refresh();
}

UNawatMusicSet* UNawatMusicSubsystem::GetMusicSet() const
{
	return MusicSet;
}

void UNawatMusicSubsystem::SetState(FGameplayTag State)
{
	// A state chosen by hand cancels a pending alert-driven change.
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(CombatHoldTimer);
	}

	BaseState = State;
	RetryCount = 0;
	Refresh();
}

void UNawatMusicSubsystem::PushOverride(FName Source, FGameplayTag State)
{
	if (Source.IsNone())
	{
		UE_LOG(LogNawat, Warning, TEXT("Music PushOverride called without a source name; ignored."));
		return;
	}

	Overrides.RemoveAll([Source](const FOverride& Entry) { return Entry.Source == Source; });
	Overrides.Add({ Source, State });
	RetryCount = 0;
	Refresh();
}

void UNawatMusicSubsystem::PopOverride(FName Source)
{
	const int32 Removed = Overrides.RemoveAll([Source](const FOverride& Entry) { return Entry.Source == Source; });
	if (Removed > 0)
	{
		Refresh();
	}
}

FGameplayTag UNawatMusicSubsystem::GetCurrentState() const
{
	return Overrides.Num() > 0 ? Overrides.Last().State : BaseState;
}

bool UNawatMusicSubsystem::HasTrackFor(FGameplayTag State) const
{
	return MusicSet && MusicSet->FindTrack(State) != nullptr;
}

void UNawatMusicSubsystem::StopAll(float FadeOutSeconds)
{
	ClearTimers();
	BaseState = FGameplayTag();
	Overrides.Reset();

	FadeOutCurrent(FadeOutSeconds);
	CurrentSound = nullptr;
}

void UNawatMusicSubsystem::HandlePauseChanged(bool bAnyReasonActive, const FNawatPausePolicy& EffectivePolicy)
{
	if (EffectivePolicy.bDuckMusic == bDucked)
	{
		return;
	}

	bDucked = EffectivePolicy.bDuckMusic;
	if (Current && MusicSet)
	{
		Current->AdjustVolume(DuckSeconds, bDucked ? MusicSet->DuckedVolume : 1.f);
	}
}

void UNawatMusicSubsystem::HandleAlertChanged(FGameplayTag Event, const FNawatEventPayload& Payload)
{
	const UNawatSettings* Settings = GetDefault<UNawatSettings>();
	const UGameInstance* GameInstance = GetGameInstance();
	if (!Settings->bAutoMusicFromAlert || !GameInstance)
	{
		return;
	}

	const int32 Level = FMath::RoundToInt(Payload.Value);
	FGameplayTag NewState = NawatTags::Music_Explore;
	if (Level >= 2)
	{
		NewState = NawatTags::Music_Combat;
	}
	else if (Level == 1)
	{
		NewState = NawatTags::Music_Tension;
	}

	FTimerManager& Timers = GameInstance->GetTimerManager();
	const bool bLeavingCombat = Level < 2 && BaseState == NawatTags::Music_Combat;

	if (bLeavingCombat && Settings->CombatMusicHoldSeconds > 0.f)
	{
		// Keep the combat track for a while; the alert often comes straight back.
		PendingAlertState = NewState;
		if (!Timers.IsTimerActive(CombatHoldTimer))
		{
			Timers.SetTimer(CombatHoldTimer, this, &UNawatMusicSubsystem::HandleCombatHoldTimer, Settings->CombatMusicHoldSeconds, false);
		}
		return;
	}

	Timers.ClearTimer(CombatHoldTimer);
	if (BaseState != NewState)
	{
		BaseState = NewState;
		RetryCount = 0;
		Refresh();
	}
}

void UNawatMusicSubsystem::HandleCombatHoldTimer()
{
	if (BaseState != PendingAlertState)
	{
		BaseState = PendingAlertState;
		RetryCount = 0;
		Refresh();
	}
}

void UNawatMusicSubsystem::HandleRetryTimer()
{
	Refresh();
}

void UNawatMusicSubsystem::Refresh()
{
	USoundBase* Desired = MusicSet ? MusicSet->FindTrack(GetCurrentState()) : nullptr;

	const bool bCurrentAudible = Current && Current->IsPlaying();
	if (Desired == CurrentSound && (Desired == nullptr || bCurrentAudible))
	{
		return;
	}

	FadeOutCurrent(MusicSet ? MusicSet->FadeOutSeconds : 1.f);
	CurrentSound = nullptr;

	if (!Desired)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;

	UAudioComponent* Component = World
		? UGameplayStatics::CreateSound2D(World, Desired, MusicSet->Volume, 1.f, 0.f, nullptr, /*bPersistAcrossLevelTransition*/ true, /*bAutoDestroy*/ false)
		: nullptr;

	if (!Component)
	{
		// The world or its audio device is not ready yet (early start-up); try again shortly.
		if (GameInstance && RetryCount < MaxPlayRetries)
		{
			++RetryCount;
			GameInstance->GetTimerManager().SetTimer(RetryTimer, this, &UNawatMusicSubsystem::HandleRetryTimer, PlayRetrySeconds, false);
		}
		return;
	}

	RetryCount = 0;
	GameInstance->GetTimerManager().ClearTimer(RetryTimer);

	// UI sounds keep playing while the game is paused.
	Component->bIsUISound = true;
	Component->FadeIn(FMath::Max(MusicSet->FadeInSeconds, 0.f), bDucked ? MusicSet->DuckedVolume : 1.f);

	Current = Component;
	CurrentSound = Desired;
}

void UNawatMusicSubsystem::FadeOutCurrent(float Seconds)
{
	if (Current)
	{
		if (Current->IsPlaying())
		{
			// Let it clean itself up once the fade has finished.
			Current->bAutoDestroy = true;
			Current->FadeOut(FMath::Max(Seconds, 0.f), 0.f);
			FadingOut.Add(Current);
		}
		else
		{
			Current->Stop();
		}
		Current = nullptr;
	}

	FadingOut.RemoveAll([](const TObjectPtr<UAudioComponent>& Component)
	{
		return !IsValid(Component) || !Component->IsPlaying();
	});
}

void UNawatMusicSubsystem::ClearTimers()
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(CombatHoldTimer);
		GameInstance->GetTimerManager().ClearTimer(RetryTimer);
	}
}
