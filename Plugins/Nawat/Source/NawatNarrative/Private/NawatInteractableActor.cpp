#include "NawatInteractableActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "NawatDialogue.h"
#include "NawatDialogueSubsystem.h"

ANawatInteractableActor::ANawatInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(SceneRoot);
	InteractionVolume->SetBoxExtent(FVector(40.f, 40.f, 40.f));
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionVolume->SetGenerateOverlapEvents(false);
}

bool ANawatInteractableActor::CanInteract_Implementation(AActor* Interactor) const
{
	if (bSingleUse && bUsed)
	{
		return false;
	}
	return UNawatCondition::AllPass(Conditions, this);
}

FText ANawatInteractableActor::GetInteractionPrompt_Implementation() const
{
	return Prompt;
}

void ANawatInteractableActor::Interact_Implementation(AActor* Interactor)
{
	if (!INawatInteractable::Execute_CanInteract(this, Interactor))
	{
		return;
	}

	bUsed = true;
	UNawatAction::ExecuteAll(Actions, this);

	if (Dialogue)
	{
		if (UNawatDialogueSubsystem* DialogueSubsystem = UNawatDialogueSubsystem::Get(this))
		{
			DialogueSubsystem->StartDialogue(Dialogue, bDialogueBlocksPlayerInput);
		}
	}
}

void ANawatInteractableActor::ResetUse()
{
	bUsed = false;
}
