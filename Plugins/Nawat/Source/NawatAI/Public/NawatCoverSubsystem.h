#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NawatCoverSubsystem.generated.h"

class ANawatCoverPoint;

USTRUCT(BlueprintType)
struct NAWATAI_API FNawatCoverResult
{
	GENERATED_BODY()

	/** Where to stand, at floor level. */
	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	FVector Location = FVector::ZeroVector;

	/** Low cover: crouch here and stand up to fire. */
	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	bool bCrouch = false;

	/** Set when the cover is a placed point; empty for generated cover. */
	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	TWeakObjectPtr<ANawatCoverPoint> Point;
};

/**
 * Finds positions hidden from a threat. Placed ANawatCoverPoint actors are used first; when none
 * qualifies, candidates are sampled on the navmesh and tested with line traces, so a level works
 * without any placed cover.
 */
UCLASS()
class NAWATAI_API UNawatCoverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterPoint(ANawatCoverPoint* Point);
	void UnregisterPoint(ANawatCoverPoint* Point);

	/** Searches around the seeker. ThreatLocation is the centre of the threat, not its feet. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Cover")
	bool FindCover(const AActor* Seeker, const FVector& ThreatLocation, float MaxDistance, FNawatCoverResult& Out);

	/** Same as FindCover but searches around SearchOrigin, e.g. a flanking position or the player. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Cover")
	bool FindCoverNear(const AActor* Seeker, const FVector& SearchOrigin, const FVector& ThreatLocation, float MaxDistance, FNawatCoverResult& Out);

	/** True when a trace from the threat's eyes to crouch height above CoverLocation is blocked. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Cover")
	bool IsHidden(const AActor* Seeker, const FVector& ThreatLocation, const FVector& CoverLocation) const;

	/** Reserves a cover for Claimer and drops whatever it held before. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Cover")
	void ClaimCover(const AActor* Claimer, const FNawatCoverResult& Cover);

	UFUNCTION(BlueprintCallable, Category = "Nawat|Cover")
	void ReleaseCover(const AActor* Claimer);

	UFUNCTION(BlueprintPure, Category = "Nawat|Cover")
	bool IsClaimedByOther(const FVector& Location, const AActor* Claimer) const;

	/** Cover closer to the threat than this is rejected. */
	UPROPERTY(BlueprintReadWrite, Category = "Nawat|Cover")
	float MinThreatDistance = 400.f;

	/** Cover farther from the threat than this is penalised, so agents do not hide out of weapon range. */
	UPROPERTY(BlueprintReadWrite, Category = "Nawat|Cover")
	float PreferredMaxThreatDistance = 2500.f;

	/** Two agents never use covers closer together than this. */
	UPROPERTY(BlueprintReadWrite, Category = "Nawat|Cover")
	float ClaimRadius = 130.f;

	/** Navmesh samples per ring when generating cover. Four rings are sampled. */
	UPROPERTY(BlueprintReadWrite, Category = "Nawat|Cover")
	int32 SamplesPerRing = 12;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FClaim
	{
		TWeakObjectPtr<const AActor> Claimer;
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<ANawatCoverPoint> Point;
	};

	bool IsBlocked(const AActor* Seeker, const FVector& ThreatLocation, const FVector& CoverLocation, float Height) const;
	void PruneClaims();

	TArray<TWeakObjectPtr<ANawatCoverPoint>> Points;
	TArray<FClaim> Claims;
};
