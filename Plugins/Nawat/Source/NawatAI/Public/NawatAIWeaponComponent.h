#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "NawatAITypes.h"
#include "NawatAIWeaponComponent.generated.h"

class UAnimMontage;
class USoundBase;
class UNawatAIWeaponComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNawatWeaponFired, UNawatAIWeaponComponent*, Weapon, FVector, Start, FVector, End);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNawatWeaponEvent, UNawatAIWeaponComponent*, Weapon);

/**
 * Hitscan weapon fired by the AI brain. It needs no weapon mesh or animation: shots start at
 * MuzzleSocket when the character mesh has it, otherwise at the owner's eyes.
 */
UCLASS(ClassGroup = (Nawat), meta = (BlueprintSpawnableComponent))
class NAWATAI_API UNawatAIWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNawatAIWeaponComponent();

	/** True when there is ammo, no reload is running and the fire interval has passed. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Weapon")
	bool CanFire() const;

	/** Fires one shot toward TargetLocation. Does nothing when CanFire is false. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Weapon")
	void FireAt(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "Nawat|Weapon")
	void StartReload();

	UFUNCTION(BlueprintPure, Category = "Nawat|Weapon")
	bool IsReloading() const { return bReloading; }

	UFUNCTION(BlueprintPure, Category = "Nawat|Weapon")
	bool NeedsReload() const { return Ammo <= 0; }

	UFUNCTION(BlueprintPure, Category = "Nawat|Weapon")
	int32 GetAmmo() const { return Ammo; }

	UFUNCTION(BlueprintPure, Category = "Nawat|Weapon")
	FVector GetMuzzleLocation() const;

	/** Copies the weapon values of a loadout and refills the magazine. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Weapon")
	void ApplyLoadout(const FNawatLoadout& Loadout);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "0.0"))
	float Damage = 10.f;

	/** Seconds between shots inside a burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "0.02"))
	float FireInterval = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "1"))
	int32 BurstSizeMin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "1"))
	int32 BurstSizeMax = 5;

	/** Seconds between bursts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "0.0"))
	float BurstPause = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "1"))
	int32 MagazineSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "0.0"))
	float ReloadTime = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "100.0"))
	float Range = 6000.f;

	/** Half-angle of the shot cone, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "0.0"))
	float SpreadDegrees = 2.5f;

	/** Added to the spread while the owner is moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "0.0"))
	float MovingSpreadDegrees = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	FName MuzzleSocket = NAME_None;

	/** World geometry that stops a shot. Pawns are always tested as well, whatever this channel is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** How far a shot is heard by AI hearing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "0.0"))
	float NoiseRange = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat|Optional")
	TObjectPtr<USoundBase> FireSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat|Optional")
	TObjectPtr<USoundBase> ReloadSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat|Optional")
	TObjectPtr<UAnimMontage> FireMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat|Optional")
	TObjectPtr<UAnimMontage> ReloadMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat|Debug")
	bool bDrawDebugTracers = false;

	/** Start is the muzzle, End is the hit point or the end of the range. */
	UPROPERTY(BlueprintAssignable, Category = "Nawat|Weapon")
	FNawatWeaponFired OnFired;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Weapon")
	FNawatWeaponEvent OnReloadStarted;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Weapon")
	FNawatWeaponEvent OnReloadFinished;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void FinishReload();

	UPROPERTY(VisibleInstanceOnly, Category = "Nawat")
	int32 Ammo = 30;

	bool bReloading = false;
	float LastFireTime = -1000.f;
	FTimerHandle ReloadTimer;
};
