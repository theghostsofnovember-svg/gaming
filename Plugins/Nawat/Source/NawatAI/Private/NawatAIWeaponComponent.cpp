#include "NawatAIWeaponComponent.h"

#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NawatAgentComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UNawatAIWeaponComponent::UNawatAIWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNawatAIWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	Ammo = FMath::Max(MagazineSize, 1);
}

void UNawatAIWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}
	bReloading = false;

	Super::EndPlay(EndPlayReason);
}

void UNawatAIWeaponComponent::ApplyLoadout(const FNawatLoadout& Loadout)
{
	Damage = Loadout.Damage;
	FireInterval = Loadout.FireInterval;
	BurstSizeMin = FMath::Max(Loadout.BurstSizeMin, 1);
	BurstSizeMax = FMath::Max(Loadout.BurstSizeMax, BurstSizeMin);
	BurstPause = Loadout.BurstPause;
	MagazineSize = FMath::Max(Loadout.MagazineSize, 1);
	ReloadTime = Loadout.ReloadTime;
	Range = Loadout.Range;
	SpreadDegrees = Loadout.SpreadDegrees;
	MovingSpreadDegrees = Loadout.MovingSpreadDegrees;
	MuzzleSocket = Loadout.MuzzleSocket;
	FireSound = Loadout.FireSound;
	ReloadSound = Loadout.ReloadSound;
	FireMontage = Loadout.FireMontage;
	ReloadMontage = Loadout.ReloadMontage;

	Ammo = MagazineSize;
}

bool UNawatAIWeaponComponent::CanFire() const
{
	const UWorld* World = GetWorld();
	return World && Ammo > 0 && !bReloading && (World->GetTimeSeconds() - LastFireTime) >= FireInterval;
}

FVector UNawatAIWeaponComponent::GetMuzzleLocation() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	if (!MuzzleSocket.IsNone())
	{
		const ACharacter* Character = Cast<ACharacter>(Owner);
		const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		if (Mesh && Mesh->DoesSocketExist(MuzzleSocket))
		{
			return Mesh->GetSocketLocation(MuzzleSocket);
		}
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	Owner->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	return EyeLocation;
}

void UNawatAIWeaponComponent::FireAt(const FVector& TargetLocation)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !CanFire())
	{
		return;
	}

	--Ammo;
	LastFireTime = World->GetTimeSeconds();

	const FVector Muzzle = GetMuzzleLocation();
	FVector Direction = (TargetLocation - Muzzle).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Owner->GetActorForwardVector();
	}

	const bool bMoving = Owner->GetVelocity().SizeSquared() > FMath::Square(10.f);
	const float Spread = SpreadDegrees + (bMoving ? MovingSpreadDegrees : 0.f);
	if (Spread > 0.f)
	{
		Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(Spread));
	}

	const FVector TraceEnd = Muzzle + Direction * Range;

	FCollisionQueryParams Params(FName(TEXT("NawatAIFire")), false, Owner);
	TArray<AActor*> Attached;
	Owner->GetAttachedActors(Attached);
	Params.AddIgnoredActors(Attached);

	// Stock character collision ignores the Visibility channel, so pawns are traced by object type
	// as well and the nearer of the two hits wins.
	FHitResult WorldHit;
	const bool bWorldHit = World->LineTraceSingleByChannel(WorldHit, Muzzle, TraceEnd, TraceChannel.GetValue(), Params);

	FHitResult PawnHit;
	const bool bPawnHit = World->LineTraceSingleByObjectType(PawnHit, Muzzle, TraceEnd, FCollisionObjectQueryParams(ECC_Pawn), Params);

	const FHitResult* Hit = nullptr;
	if (bWorldHit && bPawnHit)
	{
		Hit = (PawnHit.Distance <= WorldHit.Distance) ? &PawnHit : &WorldHit;
	}
	else if (bPawnHit)
	{
		Hit = &PawnHit;
	}
	else if (bWorldHit)
	{
		Hit = &WorldHit;
	}

	const FVector ShotEnd = Hit ? Hit->ImpactPoint : TraceEnd;

	if (Hit)
	{
		AActor* HitActor = Hit->GetActor();
		if (HitActor)
		{
			// A shot that lands on the shooter's own side stops there without hurting anyone.
			const ENawatTeam OwnTeam = UNawatAgentComponent::GetTeam(Owner);
			const bool bFriendly = OwnTeam != ENawatTeam::Neutral && OwnTeam == UNawatAgentComponent::GetTeam(HitActor);
			if (!bFriendly && Damage > 0.f)
			{
				const APawn* OwnerPawn = Cast<APawn>(Owner);
				AController* InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
				UGameplayStatics::ApplyPointDamage(HitActor, Damage, Direction, *Hit, InstigatorController, Owner, UDamageType::StaticClass());
			}
		}
	}

	UAISense_Hearing::ReportNoiseEvent(this, Muzzle, 1.f, Owner, NoiseRange, NAME_None);

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Muzzle);
	}

	if (FireMontage)
	{
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			Character->PlayAnimMontage(FireMontage);
		}
	}

	if (bDrawDebugTracers)
	{
		DrawDebugLine(World, Muzzle, ShotEnd, FColor::Yellow, false, 0.2f, 0, 1.f);
	}

	OnFired.Broadcast(this, Muzzle, ShotEnd);
}

void UNawatAIWeaponComponent::StartReload()
{
	UWorld* World = GetWorld();
	if (!World || bReloading || Ammo >= MagazineSize)
	{
		return;
	}

	bReloading = true;
	OnReloadStarted.Broadcast(this);

	if (ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetMuzzleLocation());
	}

	if (ReloadMontage)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			Character->PlayAnimMontage(ReloadMontage);
		}
	}

	if (ReloadTime > 0.f)
	{
		World->GetTimerManager().SetTimer(ReloadTimer, this, &UNawatAIWeaponComponent::FinishReload, ReloadTime, false);
	}
	else
	{
		FinishReload();
	}
}

void UNawatAIWeaponComponent::FinishReload()
{
	bReloading = false;
	Ammo = FMath::Max(MagazineSize, 1);
	OnReloadFinished.Broadcast(this);
}
