#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NawatCoverPoint.generated.h"

class UArrowComponent;
class UBillboardComponent;
class USceneComponent;

/**
 * A hand-placed cover position. Put it on the floor where an agent should stand, behind the
 * obstacle. Placed points are preferred over generated cover.
 */
UCLASS()
class NAWATAI_API ANawatCoverPoint : public AActor
{
	GENERATED_BODY()

public:
	ANawatCoverPoint();

	/** Low cover: the agent crouches here and stands up to fire. Off means full-height cover it steps out of. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bCrouch = true;

	UFUNCTION(BlueprintPure, Category = "Nawat|Cover")
	bool IsClaimed() const { return ClaimedBy.IsValid(); }

	/** Maintained by UNawatCoverSubsystem. */
	TWeakObjectPtr<const AActor> ClaimedBy;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Nawat")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> SpriteComponent;

	UPROPERTY()
	TObjectPtr<UArrowComponent> ArrowComponent;
#endif
};
