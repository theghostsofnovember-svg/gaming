#include "NawatMusicZone.h"

#include "Components/BoxComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/Pawn.h"
#include "NawatMusicSubsystem.h"

ANawatMusicZone::ANawatMusicZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(500.f, 500.f, 200.f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);
}

void ANawatMusicZone::BeginPlay()
{
	Super::BeginPlay();

	Box->OnComponentBeginOverlap.AddDynamic(this, &ANawatMusicZone::HandleBeginOverlap);
	Box->OnComponentEndOverlap.AddDynamic(this, &ANawatMusicZone::HandleEndOverlap);

	// The player may already be standing inside when the level starts.
	TArray<AActor*> Overlapping;
	Box->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (Pawn && Pawn->IsPlayerControlled())
		{
			Enter(Pawn);
			break;
		}
	}
}

void ANawatMusicZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Leave();

	Super::EndPlay(EndPlayReason);
}

void ANawatMusicZone::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		Enter(Pawn);
	}
}

void ANawatMusicZone::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor != PlayerInside.Get())
	{
		return;
	}

	// Another component of the same pawn may still be inside.
	if (!Box->IsOverlappingActor(OtherActor))
	{
		Leave();
	}
}

void ANawatMusicZone::Enter(APawn* Pawn)
{
	PlayerInside = Pawn;
	if (bPushed)
	{
		return;
	}

	if (UNawatMusicSubsystem* Music = UNawatMusicSubsystem::Get(this))
	{
		Music->PushOverride(GetFName(), State);
		bPushed = true;
	}
}

void ANawatMusicZone::Leave()
{
	PlayerInside.Reset();
	if (!bPushed)
	{
		return;
	}

	bPushed = false;
	if (UNawatMusicSubsystem* Music = UNawatMusicSubsystem::Get(this))
	{
		Music->PopOverride(GetFName());
	}
}
