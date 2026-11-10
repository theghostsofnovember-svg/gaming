#include "NawatCheckpoint.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NawatSaveSubsystem.h"
#include "NawatTags.h"
#include "NawatWorldState.h"

ANawatCheckpoint::ANawatCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(150.f, 150.f, 100.f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);

	RespawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(Box);
}

FTransform ANawatCheckpoint::GetRespawnTransform() const
{
	// Scale is dropped so a stretched volume does not stretch the respawned pawn.
	return FTransform(RespawnPoint->GetComponentRotation(), RespawnPoint->GetComponentLocation());
}

void ANawatCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &ANawatCheckpoint::HandleBeginOverlap);
}

void ANawatCheckpoint::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (bReached || !Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		return;
	}

	bReached = true;
	const FTransform RespawnTransform = GetRespawnTransform();

	if (UNawatSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UNawatSaveSubsystem>())
	{
		SaveSubsystem->SaveCheckpoint(RespawnTransform, SlotName);
	}

	if (UNawatWorldState* WorldState = GameInstance->GetSubsystem<UNawatWorldState>())
	{
		FNawatEventPayload Payload;
		Payload.Instigator = this;
		Payload.Location = RespawnTransform.GetLocation();
		WorldState->BroadcastEvent(NawatTags::Event_Checkpoint_Reached, Payload);
	}
}
