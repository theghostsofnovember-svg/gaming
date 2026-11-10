#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NawatPatrolRoute.generated.h"

class USplineComponent;

/** A patrol path. Each spline point is a stop; the curve between points is ignored, the AI paths on the navmesh. */
UCLASS()
class NAWATAI_API ANawatPatrolRoute : public AActor
{
	GENERATED_BODY()

public:
	ANawatPatrolRoute();

	UFUNCTION(BlueprintPure, Category = "Nawat|Patrol")
	int32 GetNumPoints() const;

	/** World location of a stop. The index is clamped to the valid range. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Patrol")
	FVector GetPointLocation(int32 Index) const;

	/** Seconds to wait at a stop. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Patrol")
	float GetWait(int32 Index) const;

	/** The stop after Current. Direction is +1 or -1 and flips at the ends of a ping-pong route. */
	int32 GetNextIndex(int32 Current, int32& Direction) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nawat")
	TObjectPtr<USplineComponent> Spline;

	/** Wait per stop, by point index. Stops beyond the end of this list use DefaultWait. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	TArray<float> WaitSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "0.0"))
	float DefaultWait = 2.f;

	/** After the last stop go back to the first. When off the route is walked back and forth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bLoop = true;
};
