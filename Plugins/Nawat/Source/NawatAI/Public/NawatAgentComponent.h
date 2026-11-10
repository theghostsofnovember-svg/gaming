#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NawatAITypes.h"
#include "NawatAgentComponent.generated.h"

/**
 * Says which side an actor is on. Spawners add it at runtime, so the picked character class needs
 * no changes; the player needs no component at all.
 */
UCLASS(ClassGroup = (Nawat), meta = (BlueprintSpawnableComponent))
class NAWATAI_API UNawatAgentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNawatAgentComponent();

	/**
	 * The component's team when the actor has one; otherwise Player for a player-controlled pawn
	 * and Neutral for everything else. A controller resolves to its pawn.
	 */
	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	static ENawatTeam GetTeam(const AActor* Actor);

	/** Only Player and Enemy are hostile to each other. */
	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	static bool IsHostile(const AActor* A, const AActor* B);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	ENawatTeam Team = ENawatTeam::Enemy;

	/** Agents of one team sharing this id coordinate as a squad. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	FName SquadId = NAME_None;
};
