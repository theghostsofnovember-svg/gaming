#include "NawatStoryTrigger.h"

#include "Components/BoxComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/Pawn.h"
#include "NawatWorldState.h"

ANawatStoryTrigger::ANawatStoryTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);
}

void ANawatStoryTrigger::ResetTrigger()
{
	bTriggered = false;
	bFiredThisVisit = false;
	TryFire();
}

void ANawatStoryTrigger::BeginPlay()
{
	Super::BeginPlay();

	Box->OnComponentBeginOverlap.AddDynamic(this, &ANawatStoryTrigger::HandleBeginOverlap);
	Box->OnComponentEndOverlap.AddDynamic(this, &ANawatStoryTrigger::HandleEndOverlap);

	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		WorldState->OnFactChangedNative.AddUObject(this, &ANawatStoryTrigger::HandleFactChanged);
		WorldState->OnFactsRestoredNative.AddUObject(this, &ANawatStoryTrigger::HandleFactsRestored);
	}

	// The player may already be standing inside when the level starts.
	TArray<AActor*> Overlapping;
	Box->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (Pawn && Pawn->IsPlayerControlled())
		{
			PlayerInside = Pawn;
			TryFire();
			break;
		}
	}
}

void ANawatStoryTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		WorldState->OnFactChangedNative.RemoveAll(this);
		WorldState->OnFactsRestoredNative.RemoveAll(this);
	}
	PlayerInside.Reset();

	Super::EndPlay(EndPlayReason);
}

void ANawatStoryTrigger::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	// A pawn overlaps once per component; only its first overlap starts a visit.
	if (PlayerInside.Get() != Pawn)
	{
		PlayerInside = Pawn;
		bFiredThisVisit = false;
	}
	TryFire();
}

void ANawatStoryTrigger::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor != PlayerInside.Get())
	{
		return;
	}

	// Another component of the same pawn may still be inside.
	if (!Box->IsOverlappingActor(OtherActor))
	{
		PlayerInside.Reset();
		bFiredThisVisit = false;
	}
}

void ANawatStoryTrigger::HandleFactChanged(FGameplayTag Fact, int32 OldValue, int32 NewValue)
{
	TryFire();
}

void ANawatStoryTrigger::HandleFactsRestored()
{
	TryFire();
}

void ANawatStoryTrigger::TryFire()
{
	if (!PlayerInside.IsValid() || bFiredThisVisit || (bTriggerOnce && bTriggered))
	{
		return;
	}

	if (!UNawatCondition::AllPass(Conditions, this))
	{
		return;
	}

	// Set before running: the actions usually change facts, which calls back into TryFire.
	bTriggered = true;
	bFiredThisVisit = true;
	UNawatAction::ExecuteAll(Actions, this);
}
