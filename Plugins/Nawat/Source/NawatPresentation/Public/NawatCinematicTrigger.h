#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NawatAction.h"
#include "NawatCondition.h"
#include "NawatCinematicTrigger.generated.h"

class UBoxComponent;
class ULevelSequence;
class UPrimitiveComponent;

/**
 * Drop-in volume that plays a level sequence when the player walks in and its conditions pass.
 * Conditions and actions receive this actor as context.
 */
UCLASS()
class NAWATPRESENTATION_API ANawatCinematicTrigger : public AActor
{
	GENERATED_BODY()

public:
	ANawatCinematicTrigger();

	/** Lets a trigger that already fired fire again. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Cinematic")
	void ResetTrigger();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic")
	TObjectPtr<ULevelSequence> Sequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	bool bSkippable = true;

	/** Hides the player pawn while the sequence plays. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	bool bHidePlayer = false;

	/** Not saved; gate with a fact condition when it must survive a reload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	bool bTriggerOnce = true;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Cinematic")
	TArray<TObjectPtr<UNawatCondition>> Conditions;

	/** Run when the cinematic ends, whether it played through or was skipped. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Cinematic")
	TArray<TObjectPtr<UNawatAction>> FinishedActions;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nawat")
	TObjectPtr<UBoxComponent> Box;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleCinematicFinished(ULevelSequence* FinishedSequence, bool bSkipped);

	void TryFire();

	bool bTriggered = false;
	bool bWaitingForFinish = false;
};
