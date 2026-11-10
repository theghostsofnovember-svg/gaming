#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NawatAction.h"
#include "NawatCondition.h"
#include "NawatInteractable.h"
#include "NawatInteractableActor.generated.h"

class UBoxComponent;
class UNawatDialogue;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Drop-in usable object: a lever, a note, a character to talk to. Runs its actions and optionally
 * starts a dialogue when the player uses it. Conditions and actions receive this actor as context.
 */
UCLASS()
class NAWATNARRATIVE_API ANawatInteractableActor : public AActor, public INawatInteractable
{
	GENERATED_BODY()

public:
	ANawatInteractableActor();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	/** Makes a single-use interactable usable again. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Interaction")
	void ResetUse();

	UFUNCTION(BlueprintPure, Category = "Nawat|Interaction")
	bool HasBeenUsed() const { return bUsed; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Prompt;

	/** The actor can be used only while these pass. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Interaction")
	TArray<TObjectPtr<UNawatCondition>> Conditions;

	/** Run each time the actor is used. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Interaction")
	TArray<TObjectPtr<UNawatAction>> Actions;

	/** Started after the actions, if set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UNawatDialogue> Dialogue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bDialogueBlocksPlayerInput = true;

	/** Not saved; gate with a fact condition when the use must survive a reload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bSingleUse = false;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nawat")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Optional visual. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nawat")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** What the interactor's trace hits, so the actor is usable even without a mesh. Blocks only Visibility. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nawat")
	TObjectPtr<UBoxComponent> InteractionVolume;

private:
	bool bUsed = false;
};
