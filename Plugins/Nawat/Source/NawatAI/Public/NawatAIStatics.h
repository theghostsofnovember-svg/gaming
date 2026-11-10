#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "NawatAITypes.h"
#include "NawatAIStatics.generated.h"

class ACharacter;
class ANawatAIController;
class ANawatPatrolRoute;

/** Shared spawning helpers used by the spawners and the squad subsystem. */
UCLASS()
class NAWATAI_API UNawatAIStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Spawns any character class as a Nawat agent: adds the agent, health and weapon components it
	 * lacks, then gives it a Nawat AI controller. The class needs no changes, and may be a player
	 * character Blueprint. ControllerClass and PatrolRoute may be null.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nawat|AI", meta = (WorldContext = "WorldContextObject"))
	static ACharacter* SpawnAgent(const UObject* WorldContextObject, TSubclassOf<ACharacter> CharacterClass, const FTransform& SpawnTransform,
		ENawatTeam Team, FName SquadId, const FNawatLoadout& Loadout, ANawatPatrolRoute* PatrolRoute, TSubclassOf<ANawatAIController> ControllerClass);

	/** Adds the missing Nawat components to an actor and applies the team, squad and loadout. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|AI")
	static void SetupAgentComponents(AActor* Actor, ENawatTeam Team, FName SquadId, const FNawatLoadout& Loadout);

	/** Adds the missing Nawat components with their default values and leaves existing ones untouched. */
	static void EnsureAgentComponents(AActor* Actor);

	static bool ProjectToNavigation(const UObject* WorldContextObject, const FVector& Point, const FVector& Extent, FVector& OutLocation);

	/**
	 * A standing location for CharacterClass near Origin: a random reachable navmesh point within
	 * Radius, raised by the capsule half-height. Falls back to Origin when there is no navmesh.
	 */
	static FVector FindSpawnLocation(const UObject* WorldContextObject, TSubclassOf<ACharacter> CharacterClass, const FVector& Origin, float Radius);
};
