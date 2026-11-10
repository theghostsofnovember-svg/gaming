#include "NawatCinematicTrigger.h"

#include "Components/BoxComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/Pawn.h"
#include "LevelSequence.h"
#include "NawatCinematicSubsystem.h"

ANawatCinematicTrigger::ANawatCinematicTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);
}

void ANawatCinematicTrigger::ResetTrigger()
{
	bTriggered = false;
}

void ANawatCinematicTrigger::BeginPlay()
{
	Super::BeginPlay();

	Box->OnComponentBeginOverlap.AddDynamic(this, &ANawatCinematicTrigger::HandleBeginOverlap);

	// The player may already be standing inside when the level starts.
	TArray<AActor*> Overlapping;
	Box->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		if (Pawn && Pawn->IsPlayerControlled())
		{
			TryFire();
			break;
		}
	}
}

void ANawatCinematicTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bWaitingForFinish)
	{
		bWaitingForFinish = false;
		if (UNawatCinematicSubsystem* Cinematics = UNawatCinematicSubsystem::Get(this))
		{
			Cinematics->OnCinematicFinished.RemoveDynamic(this, &ANawatCinematicTrigger::HandleCinematicFinished);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ANawatCinematicTrigger::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		TryFire();
	}
}

void ANawatCinematicTrigger::HandleCinematicFinished(ULevelSequence* FinishedSequence, bool bSkipped)
{
	if (!bWaitingForFinish || FinishedSequence != Sequence)
	{
		return;
	}

	bWaitingForFinish = false;
	if (UNawatCinematicSubsystem* Cinematics = UNawatCinematicSubsystem::Get(this))
	{
		Cinematics->OnCinematicFinished.RemoveDynamic(this, &ANawatCinematicTrigger::HandleCinematicFinished);
	}

	UNawatAction::ExecuteAll(FinishedActions, this);
}

void ANawatCinematicTrigger::TryFire()
{
	if (!Sequence || bWaitingForFinish || (bTriggerOnce && bTriggered))
	{
		return;
	}

	UNawatCinematicSubsystem* Cinematics = UNawatCinematicSubsystem::Get(this);
	if (!Cinematics || !UNawatCondition::AllPass(Conditions, this))
	{
		return;
	}

	if (!Cinematics->Play(Sequence, bSkippable, bHidePlayer))
	{
		return;
	}
	bTriggered = true;

	// Bound after Play so the finish of a cinematic that this one replaced is not mistaken for ours.
	if (Cinematics->GetCurrentSequence() == Sequence)
	{
		bWaitingForFinish = true;
		Cinematics->OnCinematicFinished.AddDynamic(this, &ANawatCinematicTrigger::HandleCinematicFinished);
	}
	else
	{
		UNawatAction::ExecuteAll(FinishedActions, this);
	}
}
