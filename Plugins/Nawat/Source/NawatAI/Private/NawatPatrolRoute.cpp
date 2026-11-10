#include "NawatPatrolRoute.h"

#include "Components/SplineComponent.h"

ANawatPatrolRoute::ANawatPatrolRoute()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SetRootComponent(Spline);
}

int32 ANawatPatrolRoute::GetNumPoints() const
{
	return Spline ? Spline->GetNumberOfSplinePoints() : 0;
}

FVector ANawatPatrolRoute::GetPointLocation(int32 Index) const
{
	const int32 Num = GetNumPoints();
	if (!Spline || Num <= 0)
	{
		return GetActorLocation();
	}

	return Spline->GetLocationAtSplinePoint(FMath::Clamp(Index, 0, Num - 1), ESplineCoordinateSpace::World);
}

float ANawatPatrolRoute::GetWait(int32 Index) const
{
	return WaitSeconds.IsValidIndex(Index) ? FMath::Max(WaitSeconds[Index], 0.f) : DefaultWait;
}

int32 ANawatPatrolRoute::GetNextIndex(int32 Current, int32& Direction) const
{
	const int32 Num = GetNumPoints();
	if (Num <= 1)
	{
		return 0;
	}

	if (bLoop)
	{
		return (Current + 1) % Num;
	}

	if (Direction == 0)
	{
		Direction = 1;
	}

	int32 Next = Current + Direction;
	if (Next < 0 || Next >= Num)
	{
		Direction = -Direction;
		Next = Current + Direction;
	}
	return FMath::Clamp(Next, 0, Num - 1);
}
