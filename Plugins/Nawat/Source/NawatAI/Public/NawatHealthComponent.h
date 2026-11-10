#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NawatAITypes.h"
#include "NawatHealthComponent.generated.h"

class AController;
class UDamageType;
class UNawatHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNawatDamaged, UNawatHealthComponent*, Component, float, Damage, AActor*, InstigatorActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNawatDeath, UNawatHealthComponent*, Component, AActor*, Killer);

/**
 * Receives any engine damage dealt to its owner (ApplyDamage, ApplyPointDamage, radial damage)
 * and handles death. The player's own weapons need to know nothing about this component.
 */
UCLASS(ClassGroup = (Nawat), meta = (BlueprintSpawnableComponent))
class NAWATAI_API UNawatHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNawatHealthComponent();

	UFUNCTION(BlueprintPure, Category = "Nawat|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Nawat|Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Health")
	bool IsDead() const { return bDead; }

	/** Kills the owner regardless of remaining health. Killer may be null. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Health")
	void Kill(AActor* Killer);

	/** Copies the health values of a loadout and refills health. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Health")
	void ApplyLoadout(const FNawatLoadout& Loadout);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** Damage from the owner's own team is ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bIgnoreFriendlyFire = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	bool bRagdollOnDeath = true;

	/** Seconds the body stays after death. Zero keeps it forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat", meta = (ClampMin = "0.0"))
	float CorpseLifeSpan = 20.f;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Health")
	FNawatDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Health")
	FNawatDeath OnDeath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	void Die(AActor* Killer);

	UPROPERTY(VisibleInstanceOnly, Category = "Nawat")
	float Health = 100.f;

	bool bDead = false;
};
