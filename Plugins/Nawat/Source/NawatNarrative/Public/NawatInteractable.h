#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NawatInteractable.generated.h"

class AActor;

UINTERFACE(MinimalAPI, Blueprintable)
class UNawatInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement on any actor the player can use. UNawatInteractorComponent finds it by looking at it.
 * Call through INawatInteractable::Execute_X(Actor, ...) so Blueprint implementations are reached too.
 */
class NAWATNARRATIVE_API INawatInteractable
{
	GENERATED_BODY()

public:
	/** Whether Interactor may use this right now. Actors that fail this are not focused. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Nawat|Interaction")
	bool CanInteract(AActor* Interactor) const;

	/** Short text for the HUD, such as "Open" or "Talk". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Nawat|Interaction")
	FText GetInteractionPrompt() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Nawat|Interaction")
	void Interact(AActor* Interactor);
};
