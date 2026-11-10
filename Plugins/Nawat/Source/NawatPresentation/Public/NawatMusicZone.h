#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NawatMusicZone.generated.h"

class APawn;
class UBoxComponent;
class UPrimitiveComponent;

/** Drop-in volume that overrides the music state while the player is inside it. */
UCLASS()
class NAWATPRESENTATION_API ANawatMusicZone : public AActor
{
	GENERATED_BODY()

public:
	ANawatMusicZone();

	/** Music state heard inside the zone. The music set needs a track for it; without one the zone is silent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	FGameplayTag State;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nawat")
	TObjectPtr<UBoxComponent> Box;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void Enter(APawn* Pawn);
	void Leave();

	TWeakObjectPtr<APawn> PlayerInside;
	bool bPushed = false;
};
