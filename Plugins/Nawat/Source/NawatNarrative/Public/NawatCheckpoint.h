#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NawatCheckpoint.generated.h"

class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;

/**
 * Drop-in volume that saves the game the first time the player walks in, recording its arrow as
 * the respawn point, and broadcasts NawatTags::Event_Checkpoint_Reached.
 */
UCLASS()
class NAWATNARRATIVE_API ANawatCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	ANawatCheckpoint();

	/** Where the player respawns: the arrow's location and rotation. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Save")
	FTransform GetRespawnTransform() const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Save")
	bool HasBeenReached() const { return bReached; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	FString SlotName = TEXT("NawatAuto");

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nawat")
	TObjectPtr<UBoxComponent> Box;

	/** Move and rotate this to place the respawn point. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nawat")
	TObjectPtr<UArrowComponent> RespawnPoint;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool bReached = false;
};
