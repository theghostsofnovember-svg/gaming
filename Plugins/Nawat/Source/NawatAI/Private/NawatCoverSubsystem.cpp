#include "NawatCoverSubsystem.h"

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"
#include "NawatCoverPoint.h"

namespace NawatCover
{
	// Heights above the floor at the cover, and above the centre of the threat.
	const float CrouchHeight = 70.f;
	const float StandHeight = 160.f;
	const float ThreatEyeOffset = 50.f;

	const float PlacedPointBonus = 300.f;
	const float FullCoverPenalty = 250.f;
	const float FarPenaltyScale = 2.f;
}

bool UNawatCoverSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UNawatCoverSubsystem::RegisterPoint(ANawatCoverPoint* Point)
{
	if (Point)
	{
		Points.AddUnique(TWeakObjectPtr<ANawatCoverPoint>(Point));
	}
}

void UNawatCoverSubsystem::UnregisterPoint(ANawatCoverPoint* Point)
{
	Points.Remove(TWeakObjectPtr<ANawatCoverPoint>(Point));
	Claims.RemoveAll([Point](const FClaim& Claim) { return Claim.Point.Get() == Point; });
}

bool UNawatCoverSubsystem::IsBlocked(const AActor* Seeker, const FVector& ThreatLocation, const FVector& CoverLocation, float Height) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FCollisionQueryParams Params(FName(TEXT("NawatCover")), false, Seeker);
	const FVector Start = ThreatLocation + FVector(0.f, 0.f, NawatCover::ThreatEyeOffset);
	const FVector End = CoverLocation + FVector(0.f, 0.f, Height);
	return World->LineTraceTestByChannel(Start, End, ECC_Visibility, Params);
}

bool UNawatCoverSubsystem::IsHidden(const AActor* Seeker, const FVector& ThreatLocation, const FVector& CoverLocation) const
{
	return IsBlocked(Seeker, ThreatLocation, CoverLocation, NawatCover::CrouchHeight);
}

bool UNawatCoverSubsystem::FindCover(const AActor* Seeker, const FVector& ThreatLocation, float MaxDistance, FNawatCoverResult& Out)
{
	const FVector Origin = Seeker ? Seeker->GetActorLocation() : FVector::ZeroVector;
	return FindCoverNear(Seeker, Origin, ThreatLocation, MaxDistance, Out);
}

bool UNawatCoverSubsystem::FindCoverNear(const AActor* Seeker, const FVector& SearchOrigin, const FVector& ThreatLocation, float MaxDistance, FNawatCoverResult& Out)
{
	Out = FNawatCoverResult();

	UWorld* World = GetWorld();
	if (!World || !Seeker || MaxDistance <= 0.f)
	{
		return false;
	}

	PruneClaims();

	const FVector SeekerLocation = Seeker->GetActorLocation();
	float BestCost = TNumericLimits<float>::Max();
	bool bFound = false;

	// Lower cost wins: near where the seeker wants to be, a short run away, within firing range.
	auto Consider = [&](const FVector& Location, bool bCrouchCover, ANawatCoverPoint* Point, float Bias)
	{
		const float ThreatDistance = FVector::Dist2D(Location, ThreatLocation);
		if (ThreatDistance < MinThreatDistance || IsClaimedByOther(Location, Seeker))
		{
			return;
		}

		float Cost = FVector::Dist(Location, SearchOrigin) + 0.5f * FVector::Dist(Location, SeekerLocation) + Bias;
		if (ThreatDistance > PreferredMaxThreatDistance)
		{
			Cost += (ThreatDistance - PreferredMaxThreatDistance) * NawatCover::FarPenaltyScale;
		}

		if (Cost < BestCost)
		{
			BestCost = Cost;
			bFound = true;
			Out.Location = Location;
			Out.bCrouch = bCrouchCover;
			Out.Point = Point;
		}
	};

	for (const TWeakObjectPtr<ANawatCoverPoint>& WeakPoint : Points)
	{
		ANawatCoverPoint* Point = WeakPoint.Get();
		if (!Point)
		{
			continue;
		}

		const AActor* Holder = Point->ClaimedBy.Get();
		if (Holder && Holder != Seeker)
		{
			continue;
		}

		const FVector Location = Point->GetActorLocation();
		if (FVector::Dist(Location, SearchOrigin) > MaxDistance)
		{
			continue;
		}

		if (IsHidden(Seeker, ThreatLocation, Location))
		{
			Consider(Location, Point->bCrouch, Point, -NawatCover::PlacedPointBonus);
		}
	}

	if (bFound)
	{
		return true;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSystem)
	{
		return false;
	}

	static const float RingScales[] = { 0.15f, 0.35f, 0.6f, 1.f };
	const int32 Samples = FMath::Clamp(SamplesPerRing, 4, 32);
	const FVector ProjectExtent(100.f, 100.f, 250.f);

	int32 RingIndex = 0;
	for (const float RingScale : RingScales)
	{
		const float Radius = MaxDistance * RingScale;
		// Rings are rotated against each other so samples do not line up.
		const float AngleOffset = RingIndex * 0.5f;
		++RingIndex;

		for (int32 Index = 0; Index < Samples; ++Index)
		{
			const float Angle = AngleOffset + (2.f * PI * Index) / Samples;
			const FVector Candidate = SearchOrigin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;

			FNavLocation NavLocation;
			if (!NavSystem->ProjectPointToNavigation(Candidate, NavLocation, ProjectExtent))
			{
				continue;
			}

			if (!IsHidden(Seeker, ThreatLocation, NavLocation.Location))
			{
				continue;
			}

			// Hidden when crouched but exposed when standing means low cover the agent can fire over.
			const bool bFullCover = IsBlocked(Seeker, ThreatLocation, NavLocation.Location, NawatCover::StandHeight);
			Consider(NavLocation.Location, !bFullCover, nullptr, bFullCover ? NawatCover::FullCoverPenalty : 0.f);
		}
	}

	return bFound;
}

void UNawatCoverSubsystem::ClaimCover(const AActor* Claimer, const FNawatCoverResult& Cover)
{
	if (!Claimer)
	{
		return;
	}

	ReleaseCover(Claimer);

	FClaim Claim;
	Claim.Claimer = Claimer;
	Claim.Location = Cover.Location;
	Claim.Point = Cover.Point;
	Claims.Add(Claim);

	if (ANawatCoverPoint* Point = Cover.Point.Get())
	{
		Point->ClaimedBy = Claimer;
	}
}

void UNawatCoverSubsystem::ReleaseCover(const AActor* Claimer)
{
	for (int32 Index = Claims.Num() - 1; Index >= 0; --Index)
	{
		const FClaim& Claim = Claims[Index];
		if (Claim.Claimer.Get() != Claimer)
		{
			continue;
		}

		ANawatCoverPoint* Point = Claim.Point.Get();
		if (Point && Point->ClaimedBy.Get() == Claimer)
		{
			Point->ClaimedBy = nullptr;
		}
		Claims.RemoveAtSwap(Index);
	}
}

bool UNawatCoverSubsystem::IsClaimedByOther(const FVector& Location, const AActor* Claimer) const
{
	const float RadiusSquared = FMath::Square(ClaimRadius);
	for (const FClaim& Claim : Claims)
	{
		const AActor* Holder = Claim.Claimer.Get();
		if (Holder && Holder != Claimer && FVector::DistSquared(Claim.Location, Location) <= RadiusSquared)
		{
			return true;
		}
	}
	return false;
}

void UNawatCoverSubsystem::PruneClaims()
{
	// A claimer destroyed without releasing leaves a stale entry behind.
	for (int32 Index = Claims.Num() - 1; Index >= 0; --Index)
	{
		if (!Claims[Index].Claimer.IsValid())
		{
			if (ANawatCoverPoint* Point = Claims[Index].Point.Get())
			{
				if (!Point->ClaimedBy.IsValid())
				{
					Point->ClaimedBy = nullptr;
				}
			}
			Claims.RemoveAtSwap(Index);
		}
	}

	Points.RemoveAll([](const TWeakObjectPtr<ANawatCoverPoint>& Point) { return !Point.IsValid(); });
}
