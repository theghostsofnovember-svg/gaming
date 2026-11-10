#include "NawatPresentationActions.h"

#include "LevelSequence.h"
#include "NawatCinematicSubsystem.h"
#include "NawatLog.h"
#include "NawatMusicSet.h"
#include "NawatMusicSubsystem.h"

void UNawatAction_SetMusicState::Run_Implementation(UObject* Context)
{
	UNawatMusicSubsystem* Music = UNawatMusicSubsystem::Get(Context);
	if (!Music)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a world context; skipped."), *GetName());
		return;
	}

	switch (Operation)
	{
	case ENawatMusicOperation::SetBase:
		Music->SetState(State);
		break;
	case ENawatMusicOperation::PushOverride:
		Music->PushOverride(OverrideSource, State);
		break;
	case ENawatMusicOperation::PopOverride:
		Music->PopOverride(OverrideSource);
		break;
	}
}

void UNawatAction_SetMusicSet::Run_Implementation(UObject* Context)
{
	UNawatMusicSubsystem* Music = UNawatMusicSubsystem::Get(Context);
	if (!Music)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a world context; skipped."), *GetName());
		return;
	}

	Music->SetMusicSet(MusicSet);
}

void UNawatAction_PlayCinematic::Run_Implementation(UObject* Context)
{
	UNawatCinematicSubsystem* Cinematics = UNawatCinematicSubsystem::Get(Context);
	if (!Cinematics)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a game world; skipped."), *GetName());
		return;
	}

	Cinematics->Play(Sequence, bSkippable, bHidePlayer);
}
