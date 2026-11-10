#include "NawatAgentComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UNawatAgentComponent::UNawatAgentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ENawatTeam UNawatAgentComponent::GetTeam(const AActor* Actor)
{
	if (const AController* Controller = Cast<AController>(Actor))
	{
		Actor = Controller->GetPawn();
	}

	if (!Actor)
	{
		return ENawatTeam::Neutral;
	}

	if (const UNawatAgentComponent* Agent = Actor->FindComponentByClass<UNawatAgentComponent>())
	{
		return Agent->Team;
	}

	const APawn* Pawn = Cast<APawn>(Actor);
	return (Pawn && Pawn->IsPlayerControlled()) ? ENawatTeam::Player : ENawatTeam::Neutral;
}

bool UNawatAgentComponent::IsHostile(const AActor* A, const AActor* B)
{
	if (!A || !B || A == B)
	{
		return false;
	}

	const ENawatTeam TeamA = GetTeam(A);
	const ENawatTeam TeamB = GetTeam(B);
	return (TeamA == ENawatTeam::Player && TeamB == ENawatTeam::Enemy)
		|| (TeamA == ENawatTeam::Enemy && TeamB == ENawatTeam::Player);
}
