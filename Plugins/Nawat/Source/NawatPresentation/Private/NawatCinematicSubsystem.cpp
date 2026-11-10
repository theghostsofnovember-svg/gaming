#include "NawatCinematicSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "NawatLog.h"
#include "NawatMusicSubsystem.h"
#include "NawatPauseSubsystem.h"
#include "NawatTags.h"
#include "NawatWorldState.h"

namespace
{
	const FName CinematicMusicSource(TEXT("Cinematic"));
}

UNawatCinematicSubsystem* UNawatCinematicSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UNawatCinematicSubsystem>() : nullptr;
}

bool UNawatCinematicSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UNawatCinematicSubsystem::Deinitialize()
{
	// The world is going away: release what is held without reporting a finish.
	if (bPlaying)
	{
		bPlaying = false;
		if (SequencePlayer)
		{
			SequencePlayer->OnFinished.RemoveDynamic(this, &UNawatCinematicSubsystem::HandleSequenceFinished);
		}
		ReleaseHolds(false);
	}

	Super::Deinitialize();
}

bool UNawatCinematicSubsystem::Play(ULevelSequence* Sequence, bool bSkippable, bool bHidePlayer)
{
	UWorld* World = GetWorld();
	if (!Sequence || !World)
	{
		UE_LOG(LogNawat, Warning, TEXT("Cinematic Play called without a sequence or a world; ignored."));
		return false;
	}

	// Listeners of the finish may start another cinematic, so this can take more than one pass.
	for (int32 Pass = 0; bPlaying && Pass < 4; ++Pass)
	{
		StopCurrent();
	}
	if (bPlaying)
	{
		return false;
	}

	ALevelSequenceActor* NewActor = nullptr;
	ULevelSequencePlayer* NewPlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, FMovieSceneSequencePlaybackSettings(), NewActor);
	if (!NewPlayer)
	{
		UE_LOG(LogNawat, Warning, TEXT("Could not create a player for sequence %s."), *Sequence->GetName());
		if (NewActor)
		{
			NewActor->Destroy();
		}
		return false;
	}

	CurrentSequence = Sequence;
	SequencePlayer = NewPlayer;
	SequenceActor = NewActor;
	bCurrentSkippable = bSkippable;
	bPlaying = true;

	const UGameInstance* GameInstance = World->GetGameInstance();

	if (UNawatPauseSubsystem* Pause = GameInstance ? GameInstance->GetSubsystem<UNawatPauseSubsystem>() : nullptr)
	{
		FNawatPausePolicy Policy;
		Policy.bPauseTime = false;
		Policy.bBlockGameInput = true;
		Policy.bHideHUD = true;
		Policy.bDuckMusic = false;
		Policy.bBlockPauseMenu = false;
		Pause->PushReason(NawatTags::Pause_Cinematic, Policy);
	}

	UNawatMusicSubsystem* Music = GameInstance ? GameInstance->GetSubsystem<UNawatMusicSubsystem>() : nullptr;
	if (Music && Music->HasTrackFor(NawatTags::Music_Cinematic))
	{
		Music->PushOverride(CinematicMusicSource, NawatTags::Music_Cinematic);
		bMusicPushed = true;
	}

	if (bHidePlayer)
	{
		const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (Pawn && !Pawn->IsHidden())
		{
			Pawn->SetActorHiddenInGame(true);
			HiddenPawn = Pawn;
		}
	}

	// Announced before playback starts so Started always precedes Finished.
	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		FNawatEventPayload Payload;
		Payload.Subject = Sequence;
		WorldState->BroadcastEvent(NawatTags::Event_Cinematic_Started, Payload);
	}
	OnCinematicStarted.Broadcast(Sequence);

	// A listener may already have replaced or ended this cinematic.
	if (bPlaying && SequencePlayer == NewPlayer)
	{
		NewPlayer->OnFinished.AddDynamic(this, &UNawatCinematicSubsystem::HandleSequenceFinished);
		NewPlayer->Play();
	}
	return true;
}

void UNawatCinematicSubsystem::Skip()
{
	if (CanSkip())
	{
		StopCurrent();
	}
}

bool UNawatCinematicSubsystem::IsPlaying() const
{
	return bPlaying;
}

bool UNawatCinematicSubsystem::CanSkip() const
{
	return bPlaying && bCurrentSkippable;
}

ULevelSequence* UNawatCinematicSubsystem::GetCurrentSequence() const
{
	return bPlaying ? CurrentSequence.Get() : nullptr;
}

void UNawatCinematicSubsystem::HandleSequenceFinished()
{
	Finish(false);
}

void UNawatCinematicSubsystem::StopCurrent()
{
	if (!bPlaying)
	{
		return;
	}

	if (SequencePlayer)
	{
		// Unbound first so stopping cannot report a natural finish.
		SequencePlayer->OnFinished.RemoveDynamic(this, &UNawatCinematicSubsystem::HandleSequenceFinished);
		SequencePlayer->GoToEndAndStop();
	}
	Finish(true);
}

void UNawatCinematicSubsystem::Finish(bool bSkipped)
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;

	ULevelSequence* FinishedSequence = CurrentSequence;
	if (SequencePlayer)
	{
		SequencePlayer->OnFinished.RemoveDynamic(this, &UNawatCinematicSubsystem::HandleSequenceFinished);
	}

	// State is fully cleared before anyone is told, because listeners often start the next cinematic.
	ReleaseHolds(bSkipped);

	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		FNawatEventPayload Payload;
		Payload.Subject = FinishedSequence;
		Payload.Value = bSkipped ? 1.f : 0.f;
		WorldState->BroadcastEvent(NawatTags::Event_Cinematic_Finished, Payload);
	}
	OnCinematicFinished.Broadcast(FinishedSequence, bSkipped);
}

void UNawatCinematicSubsystem::ReleaseHolds(bool bDestroyActorNow)
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	if (GameInstance)
	{
		if (UNawatPauseSubsystem* Pause = GameInstance->GetSubsystem<UNawatPauseSubsystem>())
		{
			Pause->PopReason(NawatTags::Pause_Cinematic);
		}
		if (bMusicPushed)
		{
			if (UNawatMusicSubsystem* Music = GameInstance->GetSubsystem<UNawatMusicSubsystem>())
			{
				Music->PopOverride(CinematicMusicSource);
			}
		}
	}
	bMusicPushed = false;

	if (APawn* Pawn = HiddenPawn.Get())
	{
		Pawn->SetActorHiddenInGame(false);
	}
	HiddenPawn.Reset();

	if (IsValid(SequenceActor))
	{
		if (bDestroyActorNow)
		{
			SequenceActor->Destroy();
		}
		else
		{
			// A natural finish arrives from inside the sequence player; destroy its actor a moment later.
			SequenceActor->SetLifeSpan(0.1f);
		}
	}

	SequenceActor = nullptr;
	SequencePlayer = nullptr;
	CurrentSequence = nullptr;
	bCurrentSkippable = false;
}
