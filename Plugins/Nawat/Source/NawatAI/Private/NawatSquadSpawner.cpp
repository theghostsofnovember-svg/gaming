#include "NawatSquadSpawner.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NawatAIController.h"
#include "NawatLog.h"
#include "NawatSquadSubsystem.h"
#include "TimerManager.h"

ANawatSquadSpawner::ANawatSquadSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->SetupAttachment(SceneRoot);
	}
#endif

	// Companions fight beside the player all level long, so they are tougher than a default enemy.
	Loadout.MaxHealth = 250.f;
}

void ANawatSquadSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!CharacterClass)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s has no Character Class; the player will have no companions."), *GetName());
		return;
	}

	// Registered even when nothing spawns now, so a later Summon request knows what to create.
	if (UNawatSquadSubsystem* Squad = UNawatSquadSubsystem::Get(this))
	{
		Squad->SetCompanionSpawnParams(CharacterClass, ControllerClass, Count, Loadout);
	}

	if (bSpawnOnBeginPlay)
	{
		// One tick later the player pawn exists and every actor has begun play.
		GetWorldTimerManager().SetTimerForNextTick(this, &ANawatSquadSpawner::SpawnOnStart);
	}
}

void ANawatSquadSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void ANawatSquadSpawner::SpawnOnStart()
{
	SpawnSquad();
}

int32 ANawatSquadSpawner::SpawnSquad()
{
	UNawatSquadSubsystem* Squad = UNawatSquadSubsystem::Get(this);
	if (!Squad || !CharacterClass)
	{
		return 0;
	}

	Squad->SetCompanionSpawnParams(CharacterClass, ControllerClass, Count, Loadout);
	return Squad->SpawnCompanions(GetActorLocation());
}
