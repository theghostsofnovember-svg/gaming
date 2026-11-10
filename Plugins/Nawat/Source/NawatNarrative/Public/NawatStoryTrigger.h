#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NawatAction.h"
#include "NawatCondition.h"
#include "NawatStoryTrigger.generated.h"

class APawn;
class UBoxComponent;
class UPrimitiveComponent;

/**
 * Drop-in volume that runs actions when the player walks in and its conditions pass. If they fail
 * on entry, it tries again whenever a fact changes while the player is still inside.
 * Conditions and actions receive this actor as context.
 */
UCLASS()
class NAWATNARRATIVE_API ANawatStoryTrigger : public AActor
{
	GENERATED_BODY()

public:
	ANawatStoryTrigger();

	/** Lets a trigger that already fired fire again. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Story")
	void ResetTrigger();

	UFUNCTION(BlueprintPure, Category = "Nawat|Story")
	bool HasTriggered() const { return bTriggered; }

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trigger")
	TArray<TObjectPtr<UNawatCondition>> Conditions;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Trigger")
	TArray<TObjectPtr<UNawatAction>> Actions;

	/** Off = fires once per visit. Not saved; gate with a fact condition when it must survive a reload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	bool bTriggerOnce = true;

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

	void HandleFactChanged(FGameplayTag Fact, int32 OldValue, int32 NewValue);
	void HandleFactsRestored();
	void TryFire();

	TWeakObjectPtr<APawn> PlayerInside;
	bool bTriggered = false;
	bool bFiredThisVisit = false;
};
