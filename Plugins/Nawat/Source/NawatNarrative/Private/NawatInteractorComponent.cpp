#include "NawatInteractorComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/GameInstance.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "NawatInteractable.h"
#include "NawatPauseSubsystem.h"

UNawatInteractorComponent::UNawatInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AActor* UNawatInteractorComponent::GetFocusedActor() const
{
	return FocusedActor.Get();
}

FText UNawatInteractorComponent::GetFocusedPrompt() const
{
	return FocusedActor.IsValid() ? FocusedPrompt : FText::GetEmpty();
}

bool UNawatInteractorComponent::TryInteract()
{
	// The timer may be up to one interval behind the view.
	UpdateFocus();

	AActor* Target = FocusedActor.Get();
	if (!Target)
	{
		return false;
	}

	INawatInteractable::Execute_Interact(Target, GetOwner());

	// Using it usually changes whether it can be used again.
	UpdateFocus();
	return true;
}

void UNawatInteractorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(UpdateTimer, this, &UNawatInteractorComponent::UpdateFocus, FMath::Max(UpdateInterval, 0.02f), true);
	}
}

void UNawatInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	SetFocus(nullptr, FText::GetEmpty());

	Super::EndPlay(EndPlayReason);
}

void UNawatInteractorComponent::UpdateFocus()
{
	AActor* NewFocus = FindInteractable();
	const FText NewPrompt = NewFocus ? INawatInteractable::Execute_GetInteractionPrompt(NewFocus) : FText::GetEmpty();
	SetFocus(NewFocus, NewPrompt);
}

AActor* UNawatInteractorComponent::FindInteractable() const
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	// Nothing is usable while a dialogue, cinematic or menu holds the player's input.
	const UGameInstance* GameInstance = World->GetGameInstance();
	const UNawatPauseSubsystem* Pause = GameInstance ? GameInstance->GetSubsystem<UNawatPauseSubsystem>() : nullptr;
	if (Pause && Pause->GetEffectivePolicy().bBlockGameInput)
	{
		return nullptr;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	const APawn* Pawn = Cast<APawn>(Owner);
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (Controller)
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		Owner->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * Range;
	const ECollisionChannel Channel = TraceChannel;

	FCollisionQueryParams Params(FName(TEXT("NawatInteract")), false, Owner);
	FHitResult Hit;
	bool bHit = false;
	if (TraceRadius > 0.f)
	{
		bHit = World->SweepSingleByChannel(Hit, ViewLocation, TraceEnd, FQuat::Identity, Channel, FCollisionShape::MakeSphere(TraceRadius), Params);
	}
	else
	{
		bHit = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, Channel, Params);
	}

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	if (!HitActor || !HitActor->Implements<UNawatInteractable>())
	{
		return nullptr;
	}

	return INawatInteractable::Execute_CanInteract(HitActor, Owner) ? HitActor : nullptr;
}

void UNawatInteractorComponent::SetFocus(AActor* NewFocus, const FText& NewPrompt)
{
	if (FocusedActor.Get() == NewFocus && FocusedPrompt.EqualTo(NewPrompt))
	{
		return;
	}

	FocusedActor = NewFocus;
	FocusedPrompt = NewPrompt;
	OnFocusChanged.Broadcast(NewFocus, NewPrompt);
}
