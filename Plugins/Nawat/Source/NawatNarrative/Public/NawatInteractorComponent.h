#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "NawatInteractorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNawatFocusChanged, AActor*, NewFocus, FText, Prompt);

/**
 * Add to the player pawn. Looks along the player's view a few times per second for an
 * INawatInteractable and remembers it as the focus; the game calls TryInteract from its input.
 */
UCLASS(ClassGroup = (Nawat), meta = (BlueprintSpawnableComponent))
class NAWATNARRATIVE_API UNawatInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNawatInteractorComponent();

	/** Null when nothing usable is in view. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Interaction")
	AActor* GetFocusedActor() const;

	/** Empty when nothing is focused. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Interaction")
	FText GetFocusedPrompt() const;

	/** Uses whatever is in view right now. Returns whether something was used. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Interaction")
	bool TryInteract();

	/** NewFocus is null and Prompt empty when the focus is lost. Also fires when only the prompt changes. */
	UPROPERTY(BlueprintAssignable, Category = "Nawat|Interaction")
	FNawatFocusChanged OnFocusChanged;

	/** Measured from the view point, so third-person cameras need the boom length added. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float Range = 300.f;

	/** 0 traces a line; larger values sweep a sphere, which is more forgiving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float TraceRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Seconds between focus updates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.02"))
	float UpdateInterval = 0.1f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateFocus();
	AActor* FindInteractable() const;
	void SetFocus(AActor* NewFocus, const FText& NewPrompt);

	TWeakObjectPtr<AActor> FocusedActor;
	FText FocusedPrompt;
	FTimerHandle UpdateTimer;
};
