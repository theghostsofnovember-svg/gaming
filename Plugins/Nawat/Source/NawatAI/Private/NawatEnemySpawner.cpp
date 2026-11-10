#include "NawatEnemySpawner.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "NawatAIController.h"
#include "NawatAIStatics.h"
#include "NawatHealthComponent.h"
#include "NawatLog.h"
#include "NawatPatrolRoute.h"
#include "NawatWorldState.h"
#include "TimerManager.h"

ANawatEnemySpawner::ANawatEnemySpawner()
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
}

void ANawatEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (SpawnOnEvent.IsValid())
	{
		if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
		{
			BoundEvent = SpawnOnEvent;
			WorldState->OnEventNative(BoundEvent).AddUObject(this, &ANawatEnemySpawner::HandleSpawnEvent);
		}
	}

	if (bSpawnOnBeginPlay)
	{
		// One tick later every actor has begun play, so the player pawn and the navmesh are ready.
		GetWorldTimerManager().SetTimerForNextTick(this, &ANawatEnemySpawner::SpawnOnStart);
	}
}

void ANawatEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundEvent.IsValid())
	{
		if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
		{
			WorldState->OnEventNative(BoundEvent).RemoveAll(this);
		}
		BoundEvent = FGameplayTag();
	}

	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void ANawatEnemySpawner::SpawnOnStart()
{
	SpawnAgents();
}

void ANawatEnemySpawner::HandleSpawnEvent(FGameplayTag Event, const FNawatEventPayload& Payload)
{
	SpawnAgents();
}

int32 ANawatEnemySpawner::SpawnAgents()
{
	UWorld* World = GetWorld();
	if (!World || (bSpawnOnlyOnce && bHasSpawned))
	{
		return 0;
	}

	if (!CharacterClass)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s has no Character Class; nothing spawned."), *GetName());
		return 0;
	}

	if (!UNawatCondition::AllPass(Conditions, this))
	{
		return 0;
	}

	if (!FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		UE_LOG(LogNawat, Warning, TEXT("%s: the level has no navigation system. Add a NavMeshBoundsVolume or the agents cannot move."), *GetName());
	}

	// Without an explicit id the group of this spawner is one squad of its own.
	const FName EffectiveSquadId = SquadId.IsNone() ? GetFName() : SquadId;
	const FRotator Facing(0.f, GetActorRotation().Yaw, 0.f);

	int32 Spawned = 0;
	for (int32 Index = 0; Index < FMath::Max(Count, 1); ++Index)
	{
		const FVector SpawnLocation = UNawatAIStatics::FindSpawnLocation(this, CharacterClass, GetActorLocation(), SpawnRadius);
		const FTransform SpawnTransform(Facing, SpawnLocation);

		ACharacter* Character = UNawatAIStatics::SpawnAgent(this, CharacterClass, SpawnTransform, ENawatTeam::Enemy, EffectiveSquadId, Loadout, PatrolRoute, ControllerClass);
		if (Character)
		{
			SpawnedAgents.Add(Character);
			++Spawned;
		}
	}

	if (Spawned > 0)
	{
		bHasSpawned = true;
	}
	return Spawned;
}

int32 ANawatEnemySpawner::GetAliveCount() const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<ACharacter>& Entry : SpawnedAgents)
	{
		const ACharacter* Character = Entry.Get();
		if (!Character)
		{
			continue;
		}

		const UNawatHealthComponent* AgentHealth = Character->FindComponentByClass<UNawatHealthComponent>();
		if (!AgentHealth || !AgentHealth->IsDead())
		{
			++Alive;
		}
	}
	return Alive;
}
