#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "NawatAITypes.h"
#include "NawatCondition.h"
#include "NawatEnemySpawner.generated.h"

class ACharacter;
class ANawatAIController;
class ANawatPatrolRoute;
class UBillboardComponent;
class USceneComponent;
struct FNawatEventPayload;

/**
 * Drop in the level, pick any character class, play. Spawns enemies and gives each a Nawat brain,
 * health and a weapon; the picked class needs no changes and no AI assets.
 */
UCLASS()
class NAWATAI_API ANawatEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	ANawatEnemySpawner();

	/** Spawns the group now if the conditions pass. Returns how many appeared. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|AI")
	int32 SpawnAgents();

	/** Spawned agents that are still alive. */
	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	int32 GetAliveCount() const;

	/** Any character, e.g. the mannequin character Blueprint of the First or Third Person template. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	TSubclassOf<ACharacter> CharacterClass;

	/** Leave empty for the default brain, or pick a Blueprint subclass with different tunables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	TSubclassOf<ANawatAIController> ControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** Agents appear at random navmesh points within this distance of the spawner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "0.0"))
	float SpawnRadius = 300.f;

	/** Enemies sharing a squad id coordinate. Leave None to make this spawner its own squad. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	FName SquadId = NAME_None;

	/** Optional. Without a route the agents wander around the spawner. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Nawat")
	TObjectPtr<ANawatPatrolRoute> PatrolRoute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	FNawatLoadout Loadout;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat|Trigger")
	bool bSpawnOnBeginPlay = true;

	/** Spawns when this event is broadcast on UNawatWorldState. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat|Trigger")
	FGameplayTag SpawnOnEvent;

	/** Ignore further triggers after the first successful spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat|Trigger")
	bool bSpawnOnlyOnce = true;

	/** All must pass at the moment of a trigger, otherwise nothing spawns. They are not polled. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Nawat|Trigger")
	TArray<TObjectPtr<UNawatCondition>> Conditions;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SpawnOnStart();
	void HandleSpawnEvent(FGameplayTag Event, const FNawatEventPayload& Payload);

	UPROPERTY(VisibleAnywhere, Category = "Nawat")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> SpriteComponent;
#endif

	TArray<TWeakObjectPtr<ACharacter>> SpawnedAgents;
	FGameplayTag BoundEvent;
	bool bHasSpawned = false;
};
