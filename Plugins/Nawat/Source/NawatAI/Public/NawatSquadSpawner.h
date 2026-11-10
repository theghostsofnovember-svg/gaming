#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "NawatAITypes.h"
#include "NawatSquadSpawner.generated.h"

class ACharacter;
class ANawatAIController;
class UBillboardComponent;
class USceneComponent;

/**
 * Drop in the level to give the player companions. They are on the player's team, follow by
 * default and obey the Request_Squad_* events. The spawner also tells the squad subsystem what the
 * companions are, so a Summon request can bring them back after a Dismiss.
 */
UCLASS()
class NAWATAI_API ANawatSquadSpawner : public AActor
{
	GENERATED_BODY()

public:
	ANawatSquadSpawner();

	/** Spawns the companions around this actor. Returns how many appeared. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Squad")
	int32 SpawnSquad();

	/** Any character, e.g. the mannequin character Blueprint of the First or Third Person template. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	TSubclassOf<ACharacter> CharacterClass;

	/** Leave empty for the default brain, or pick a Blueprint subclass with different tunables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	TSubclassOf<ANawatAIController> ControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "1"))
	int32 Count = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	FNawatLoadout Loadout;

	/** Off for a level that starts solo: the companions then appear on the first Summon request. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bSpawnOnBeginPlay = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SpawnOnStart();

	UPROPERTY(VisibleAnywhere, Category = "Nawat")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> SpriteComponent;
#endif
};
