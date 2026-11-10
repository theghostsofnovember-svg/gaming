#include "NawatAIStatics.h"

#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "NawatAIController.h"
#include "NawatAIWeaponComponent.h"
#include "NawatAgentComponent.h"
#include "NawatHealthComponent.h"
#include "NawatLog.h"
#include "NawatPatrolRoute.h"

namespace NawatAIStaticsPrivate
{
	template <typename ComponentType>
	ComponentType* FindOrAddComponent(AActor* Actor)
	{
		ComponentType* Component = Actor->FindComponentByClass<ComponentType>();
		if (!Component)
		{
			Component = NewObject<ComponentType>(Actor);
			Actor->AddInstanceComponent(Component);
			Component->RegisterComponent();
		}
		return Component;
	}

	UWorld* GetWorldFrom(const UObject* WorldContextObject)
	{
		return (GEngine && WorldContextObject) ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	}
}

ACharacter* UNawatAIStatics::SpawnAgent(const UObject* WorldContextObject, TSubclassOf<ACharacter> CharacterClass, const FTransform& SpawnTransform,
	ENawatTeam Team, FName SquadId, const FNawatLoadout& Loadout, ANawatPatrolRoute* PatrolRoute, TSubclassOf<ANawatAIController> ControllerClass)
{
	UWorld* World = NawatAIStaticsPrivate::GetWorldFrom(WorldContextObject);
	if (!World || !CharacterClass)
	{
		UE_LOG(LogNawat, Warning, TEXT("SpawnAgent called without a world or a character class; nothing spawned."));
		return nullptr;
	}

	UClass* BrainClass = ControllerClass ? ControllerClass.Get() : ANawatAIController::StaticClass();

	ACharacter* Character = World->SpawnActorDeferred<ACharacter>(CharacterClass.Get(), SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Character)
	{
		return nullptr;
	}

	// The picked class is often a player character: it must neither grab a player nor spawn a controller
	// of its own. The brain is created below, once the Nawat components exist.
	Character->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	Character->AutoPossessAI = EAutoPossessAI::Disabled;
	Character->AIControllerClass = BrainClass;
	Character->FinishSpawning(SpawnTransform);

	if (!IsValid(Character))
	{
		return nullptr;
	}

	SetupAgentComponents(Character, Team, SquadId, Loadout);

	FActorSpawnParameters ControllerParams;
	ControllerParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANawatAIController* Controller = World->SpawnActor<ANawatAIController>(BrainClass, Character->GetActorLocation(), Character->GetActorRotation(), ControllerParams);
	if (!Controller)
	{
		UE_LOG(LogNawat, Warning, TEXT("SpawnAgent could not create a controller for %s."), *Character->GetName());
		return Character;
	}

	Controller->InitAgent(SquadId, PatrolRoute);
	Controller->Possess(Character);
	return Character;
}

void UNawatAIStatics::SetupAgentComponents(AActor* Actor, ENawatTeam Team, FName SquadId, const FNawatLoadout& Loadout)
{
	if (!Actor)
	{
		return;
	}

	UNawatAgentComponent* Agent = NawatAIStaticsPrivate::FindOrAddComponent<UNawatAgentComponent>(Actor);
	Agent->Team = Team;
	Agent->SquadId = SquadId;

	NawatAIStaticsPrivate::FindOrAddComponent<UNawatHealthComponent>(Actor)->ApplyLoadout(Loadout);
	NawatAIStaticsPrivate::FindOrAddComponent<UNawatAIWeaponComponent>(Actor)->ApplyLoadout(Loadout);
}

void UNawatAIStatics::EnsureAgentComponents(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	NawatAIStaticsPrivate::FindOrAddComponent<UNawatAgentComponent>(Actor);
	NawatAIStaticsPrivate::FindOrAddComponent<UNawatHealthComponent>(Actor);
	NawatAIStaticsPrivate::FindOrAddComponent<UNawatAIWeaponComponent>(Actor);
}

bool UNawatAIStatics::ProjectToNavigation(const UObject* WorldContextObject, const FVector& Point, const FVector& Extent, FVector& OutLocation)
{
	UWorld* World = NawatAIStaticsPrivate::GetWorldFrom(WorldContextObject);
	UNavigationSystemV1* NavSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavSystem)
	{
		return false;
	}

	FNavLocation NavLocation;
	if (!NavSystem->ProjectPointToNavigation(Point, NavLocation, Extent))
	{
		return false;
	}

	OutLocation = NavLocation.Location;
	return true;
}

FVector UNawatAIStatics::FindSpawnLocation(const UObject* WorldContextObject, TSubclassOf<ACharacter> CharacterClass, const FVector& Origin, float Radius)
{
	float HalfHeight = 90.f;
	if (CharacterClass)
	{
		const ACharacter* DefaultCharacter = CharacterClass->GetDefaultObject<ACharacter>();
		const UCapsuleComponent* Capsule = DefaultCharacter ? DefaultCharacter->GetCapsuleComponent() : nullptr;
		if (Capsule)
		{
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	UWorld* World = NawatAIStaticsPrivate::GetWorldFrom(WorldContextObject);
	UNavigationSystemV1* NavSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (NavSystem)
	{
		FNavLocation NavLocation;
		if (Radius > 0.f && NavSystem->GetRandomReachablePointInRadius(Origin, Radius, NavLocation))
		{
			return NavLocation.Location + FVector(0.f, 0.f, HalfHeight + 2.f);
		}

		if (NavSystem->ProjectPointToNavigation(Origin, NavLocation, FVector(200.f, 200.f, 400.f)))
		{
			return NavLocation.Location + FVector(0.f, 0.f, HalfHeight + 2.f);
		}
	}

	// No navmesh here: the agent will stand where it is put, so at least spread the group out.
	const FVector2D Offset = FMath::RandPointInCircle(FMath::Max(Radius, 0.f));
	return Origin + FVector(Offset.X, Offset.Y, 0.f);
}
