#include "NawatHealthComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "NawatAgentComponent.h"
#include "NawatTags.h"
#include "NawatWorldState.h"

UNawatHealthComponent::UNawatHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UNawatHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;
}

void UNawatHealthComponent::Kill(AActor* Killer)
{
	if (!bDead)
	{
		Die(Killer);
	}
}

void UNawatHealthComponent::ApplyLoadout(const FNawatLoadout& Loadout)
{
	MaxHealth = FMath::Max(Loadout.MaxHealth, 1.f);
	bRagdollOnDeath = Loadout.bRagdollOnDeath;
	CorpseLifeSpan = Loadout.CorpseLifeSpan;

	if (!bDead)
	{
		Health = MaxHealth;
	}
}

void UNawatHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bDead)
	{
		Health = MaxHealth;
	}

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UNawatHealthComponent::HandleAnyDamage);
	}
}

void UNawatHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UNawatHealthComponent::HandleAnyDamage);
	}

	Super::EndPlay(EndPlayReason);
}

void UNawatHealthComponent::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (bDead || Damage <= 0.f)
	{
		return;
	}

	AActor* Owner = GetOwner();
	AActor* InstigatorActor = (InstigatedBy && InstigatedBy->GetPawn()) ? InstigatedBy->GetPawn() : DamageCauser;

	if (bIgnoreFriendlyFire && Owner && InstigatorActor && InstigatorActor != Owner)
	{
		const ENawatTeam OwnTeam = UNawatAgentComponent::GetTeam(Owner);
		if (OwnTeam != ENawatTeam::Neutral && OwnTeam == UNawatAgentComponent::GetTeam(InstigatorActor))
		{
			return;
		}
	}

	Health = FMath::Max(Health - Damage, 0.f);
	OnDamaged.Broadcast(this, Damage, InstigatorActor);

	if (Health <= 0.f && !bDead)
	{
		Die(InstigatorActor);
	}
}

void UNawatHealthComponent::Die(AActor* Killer)
{
	bDead = true;
	Health = 0.f;

	AActor* Owner = GetOwner();

	// Listeners first: the brain releases its cover and attack token while the pawn is still intact.
	OnDeath.Broadcast(this, Killer);

	if (!Owner)
	{
		return;
	}

	// A player who carries this component keeps its pawn; the game decides what its death means.
	APawn* Pawn = Cast<APawn>(Owner);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		return;
	}

	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		FNawatEventPayload Payload;
		Payload.Instigator = Killer;
		Payload.Subject = Owner;
		Payload.Location = Owner->GetActorLocation();
		WorldState->BroadcastEvent(NawatTags::Event_AI_Death, Payload);
	}

	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		Character->StopAnimMontage(nullptr);

		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}

		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (bRagdollOnDeath && Mesh)
		{
			Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
			Mesh->SetSimulatePhysics(true);
		}
	}

	if (Pawn)
	{
		if (AController* Controller = Pawn->GetController())
		{
			Controller->UnPossess();
		}
	}

	if (CorpseLifeSpan > 0.f)
	{
		Owner->SetLifeSpan(CorpseLifeSpan);
	}
}
