#pragma once

#include "CoreMinimal.h"
#include "NawatAITypes.generated.h"

class UAnimMontage;
class USoundBase;

UENUM(BlueprintType)
enum class ENawatTeam : uint8
{
	Neutral,
	/** The player and the companions. */
	Player,
	Enemy
};

UENUM(BlueprintType)
enum class ENawatAIState : uint8
{
	Idle,
	Patrol,
	/** Walking to a stimulus to check it. */
	Suspicious,
	Combat,
	/** Lost the target; looking around its last known location. */
	Search,
	Dead
};

/** Orders the player gives to companions. Enemies always have None. */
UENUM(BlueprintType)
enum class ENawatSquadOrder : uint8
{
	None,
	Follow,
	Hold,
	Attack,
	Regroup
};

/** What a squad member does during a fight. */
UENUM(BlueprintType)
enum class ENawatSquadRole : uint8
{
	/** Stay in cover and fire. */
	Suppress,
	/** Move to cover at an angle to the target. */
	Flank,
	/** Move to closer cover. */
	Advance
};

/** Health and weapon values a spawner applies to the agents it creates. Every asset slot is optional. */
USTRUCT(BlueprintType)
struct NAWATAI_API FNawatLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bRagdollOnDeath = true;

	/** Seconds the body stays after death. Zero keeps it forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "0.0"))
	float CorpseLifeSpan = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 10.f;

	/** Seconds between shots inside a burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.02"))
	float FireInterval = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "1"))
	int32 BurstSizeMin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "1"))
	int32 BurstSizeMax = 5;

	/** Seconds between bursts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float BurstPause = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "1"))
	int32 MagazineSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float ReloadTime = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "100.0"))
	float Range = 6000.f;

	/** Half-angle of the shot cone, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float SpreadDegrees = 2.5f;

	/** Added to the spread while the shooter is moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float MovingSpreadDegrees = 3.f;

	/** Socket on the character mesh the shots start from. Falls back to the eyes when missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName MuzzleSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Optional")
	TObjectPtr<USoundBase> FireSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Optional")
	TObjectPtr<USoundBase> ReloadSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Optional")
	TObjectPtr<UAnimMontage> FireMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Optional")
	TObjectPtr<UAnimMontage> ReloadMontage = nullptr;
};
