#include "NawatSquadSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NawatAIController.h"
#include "NawatAIStatics.h"
#include "NawatLog.h"
#include "NawatTags.h"
#include "NawatWorldState.h"

const FName UNawatSquadSubsystem::PlayerSquadId(TEXT("PlayerSquad"));

namespace NawatSquadPrivate
{
	TArray<FGameplayTag> GetRequestTags()
	{
		TArray<FGameplayTag> Tags;
		Tags.Add(NawatTags::Request_AI_RaiseAlarm.GetTag());
		Tags.Add(NawatTags::Request_Squad_Follow.GetTag());
		Tags.Add(NawatTags::Request_Squad_Hold.GetTag());
		Tags.Add(NawatTags::Request_Squad_Attack.GetTag());
		Tags.Add(NawatTags::Request_Squad_Regroup.GetTag());
		Tags.Add(NawatTags::Request_Squad_Dismiss.GetTag());
		Tags.Add(NawatTags::Request_Squad_Summon.GetTag());
		return Tags;
	}

	/** Offset of member Index on a ring of Count members; a single member stays at the centre. */
	FVector RingOffset(int32 Index, int32 Count, float Radius)
	{
		if (Count <= 1)
		{
			return FVector::ZeroVector;
		}

		const float Angle = (2.f * PI * Index) / Count;
		return FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
	}
}

UNawatSquadSubsystem* UNawatSquadSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UNawatSquadSubsystem>() : nullptr;
}

bool UNawatSquadSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UNawatSquadSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	BindToBus();
}

void UNawatSquadSubsystem::Deinitialize()
{
	if (bBoundToBus)
	{
		if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
		{
			for (const FGameplayTag& Tag : NawatSquadPrivate::GetRequestTags())
			{
				WorldState->OnEventNative(Tag).RemoveAll(this);
			}
		}
		bBoundToBus = false;
	}

	Squads.Reset();

	Super::Deinitialize();
}

void UNawatSquadSubsystem::BindToBus()
{
	if (bBoundToBus)
	{
		return;
	}

	UNawatWorldState* WorldState = UNawatWorldState::Get(this);
	if (!WorldState)
	{
		return;
	}

	for (const FGameplayTag& Tag : NawatSquadPrivate::GetRequestTags())
	{
		WorldState->OnEventNative(Tag).AddUObject(this, &UNawatSquadSubsystem::HandleBusEvent);
	}
	bBoundToBus = true;
}

void UNawatSquadSubsystem::HandleBusEvent(FGameplayTag Event, const FNawatEventPayload& Payload)
{
	if (Event.MatchesTagExact(NawatTags::Request_AI_RaiseAlarm))
	{
		RaiseAlarm(Payload.Location);
	}
	else if (Event.MatchesTagExact(NawatTags::Request_Squad_Follow))
	{
		IssueOrder(ENawatSquadOrder::Follow, FVector::ZeroVector, nullptr);
	}
	else if (Event.MatchesTagExact(NawatTags::Request_Squad_Hold))
	{
		IssueOrder(ENawatSquadOrder::Hold, Payload.Location, nullptr);
	}
	else if (Event.MatchesTagExact(NawatTags::Request_Squad_Attack))
	{
		IssueOrder(ENawatSquadOrder::Attack, Payload.Location, Cast<AActor>(Payload.Subject.Get()));
	}
	else if (Event.MatchesTagExact(NawatTags::Request_Squad_Regroup))
	{
		IssueOrder(ENawatSquadOrder::Regroup, FVector::ZeroVector, nullptr);
	}
	else if (Event.MatchesTagExact(NawatTags::Request_Squad_Dismiss))
	{
		DismissCompanions(Payload.Location);
	}
	else if (Event.MatchesTagExact(NawatTags::Request_Squad_Summon))
	{
		SummonCompanions(Payload.Location);
	}
}

// ---- Membership ----

UNawatSquadSubsystem::FSquad* UNawatSquadSubsystem::FindSquad(const ANawatAIController* Member)
{
	if (!Member)
	{
		return nullptr;
	}

	const ENawatTeam Team = Member->GetAgentTeam();
	const FName Id = Member->GetSquadId();
	return Squads.FindByPredicate([Team, Id](const FSquad& Squad) { return Squad.Team == Team && Squad.Id == Id; });
}

const UNawatSquadSubsystem::FSquad* UNawatSquadSubsystem::FindSquad(const ANawatAIController* Member) const
{
	if (!Member)
	{
		return nullptr;
	}

	const ENawatTeam Team = Member->GetAgentTeam();
	const FName Id = Member->GetSquadId();
	return Squads.FindByPredicate([Team, Id](const FSquad& Squad) { return Squad.Team == Team && Squad.Id == Id; });
}

void UNawatSquadSubsystem::RegisterMember(ANawatAIController* Member)
{
	if (!Member)
	{
		return;
	}

	// Controllers can be possessed before the world begins play.
	BindToBus();

	FSquad* Squad = FindSquad(Member);
	if (!Squad)
	{
		FSquad NewSquad;
		NewSquad.Team = Member->GetAgentTeam();
		NewSquad.Id = Member->GetSquadId();
		Squad = &Squads.Add_GetRef(NewSquad);
	}

	Squad->Members.AddUnique(TWeakObjectPtr<ANawatAIController>(Member));
}

void UNawatSquadSubsystem::UnregisterMember(ANawatAIController* Member)
{
	if (!Member)
	{
		return;
	}

	// The member's team or squad id may have changed since it registered, so every squad is checked.
	for (FSquad& Squad : Squads)
	{
		Squad.Members.RemoveAll([Member](const TWeakObjectPtr<ANawatAIController>& Entry) { return !Entry.IsValid() || Entry.Get() == Member; });
		Squad.Tokens.RemoveAll([Member](const FToken& Token) { return !Token.Holder.IsValid() || Token.Holder.Get() == Member; });
	}

	AssignRoles(Member);
	RefreshAlert();
}

void UNawatSquadSubsystem::GatherTeam(ENawatTeam Team, bool bIncludeLeaving, TArray<ANawatAIController*>& OutMembers) const
{
	OutMembers.Reset();

	for (const FSquad& Squad : Squads)
	{
		if (Squad.Team != Team)
		{
			continue;
		}

		for (const TWeakObjectPtr<ANawatAIController>& Entry : Squad.Members)
		{
			ANawatAIController* Member = Entry.Get();
			if (!Member || !Member->GetPawn() || Member->GetState() == ENawatAIState::Dead)
			{
				continue;
			}

			if (bIncludeLeaving || !Member->IsLeaving())
			{
				OutMembers.Add(Member);
			}
		}
	}
}

// ---- Shared knowledge ----

void UNawatSquadSubsystem::ReportTarget(ANawatAIController* Member, AActor* Target, const FVector& Location)
{
	const UWorld* World = GetWorld();
	FSquad* Squad = FindSquad(Member);
	if (!World || !Squad)
	{
		return;
	}

	Squad->TargetActor = Target;
	Squad->TargetLocation = Location;
	Squad->TargetTime = World->GetTimeSeconds();
}

bool UNawatSquadSubsystem::GetSharedTarget(const ANawatAIController* Member, const AActor* Target, FVector& OutLocation, float& OutTime) const
{
	const FSquad* Squad = FindSquad(Member);
	if (!Squad || !Target || Squad->TargetActor.Get() != Target)
	{
		return false;
	}

	OutLocation = Squad->TargetLocation;
	OutTime = Squad->TargetTime;
	return true;
}

void UNawatSquadSubsystem::NotifyCombatStarted(ANawatAIController* Member, AActor* Target, const FVector& Location)
{
	const FSquad* Squad = FindSquad(Member);
	const APawn* MemberPawn = Member ? Member->GetPawn() : nullptr;
	if (!Squad || !MemberPawn)
	{
		return;
	}

	if (Target)
	{
		ReportTarget(Member, Target, Location);
	}

	// Alerting a mate can re-enter this function, so iterate over a copy.
	const TArray<TWeakObjectPtr<ANawatAIController>> Mates = Squad->Members;
	const FVector Origin = MemberPawn->GetActorLocation();
	const float RadiusSquared = FMath::Square(AlertRadius);

	for (const TWeakObjectPtr<ANawatAIController>& Entry : Mates)
	{
		ANawatAIController* Mate = Entry.Get();
		const APawn* MatePawn = Mate ? Mate->GetPawn() : nullptr;
		if (!MatePawn || Mate == Member)
		{
			continue;
		}

		if (FVector::DistSquared(MatePawn->GetActorLocation(), Origin) <= RadiusSquared)
		{
			Mate->ReceiveSquadAlert(Target, Location);
		}
	}

	AssignRoles(Member);
}

void UNawatSquadSubsystem::AssignRoles(const ANawatAIController* Member)
{
	const FSquad* Squad = FindSquad(Member);
	if (!Squad)
	{
		return;
	}

	TArray<ANawatAIController*> Fighters;
	for (const TWeakObjectPtr<ANawatAIController>& Entry : Squad->Members)
	{
		ANawatAIController* Fighter = Entry.Get();
		if (Fighter && Fighter->GetPawn() && Fighter->GetState() == ENawatAIState::Combat)
		{
			Fighters.Add(Fighter);
		}
	}

	// One fighter keeps the target pinned; with more, the others work around and toward it.
	static const ENawatSquadRole Pattern[] = { ENawatSquadRole::Suppress, ENawatSquadRole::Flank, ENawatSquadRole::Advance };
	int32 Flankers = 0;
	for (int32 Index = 0; Index < Fighters.Num(); ++Index)
	{
		const ENawatSquadRole AssignedRole = Fighters.Num() == 1 ? ENawatSquadRole::Suppress : Pattern[Index % 3];
		float Side = 1.f;
		if (AssignedRole == ENawatSquadRole::Flank)
		{
			Side = (Flankers % 2 == 0) ? 1.f : -1.f;
			++Flankers;
		}
		Fighters[Index]->SetSquadRole(AssignedRole, Side);
	}
}

// ---- Attack tokens ----

bool UNawatSquadSubsystem::RequestAttackToken(ANawatAIController* Member)
{
	const UWorld* World = GetWorld();
	FSquad* Squad = FindSquad(Member);
	if (!World || !Squad)
	{
		return true;
	}

	// Companions are few and fight beside the player; only enemy fire is rationed.
	if (Squad->Team != ENawatTeam::Enemy)
	{
		return true;
	}

	const float Now = World->GetTimeSeconds();
	const float Timeout = AttackTokenTimeout;
	Squad->Tokens.RemoveAll([Now, Timeout](const FToken& Token) { return !Token.Holder.IsValid() || (Now - Token.Time) > Timeout; });

	if (Squad->Tokens.ContainsByPredicate([Member](const FToken& Token) { return Token.Holder.Get() == Member; }))
	{
		return true;
	}

	if (Squad->Tokens.Num() >= FMath::Max(MaxSimultaneousAttackers, 1))
	{
		return false;
	}

	FToken Token;
	Token.Holder = Member;
	Token.Time = Now;
	Squad->Tokens.Add(Token);
	return true;
}

void UNawatSquadSubsystem::ReleaseAttackToken(ANawatAIController* Member)
{
	if (FSquad* Squad = FindSquad(Member))
	{
		Squad->Tokens.RemoveAll([Member](const FToken& Token) { return Token.Holder.Get() == Member; });
	}
}

// ---- Alert ----

void UNawatSquadSubsystem::RefreshAlert()
{
	const UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	int32 Highest = 0;
	for (const FSquad& Squad : Squads)
	{
		if (Squad.Team != ENawatTeam::Enemy)
		{
			continue;
		}

		for (const TWeakObjectPtr<ANawatAIController>& Entry : Squad.Members)
		{
			if (const ANawatAIController* Member = Entry.Get())
			{
				Highest = FMath::Max(Highest, Member->GetAlertLevel());
			}
		}
	}

	if (Highest == CurrentAlert)
	{
		return;
	}

	CurrentAlert = Highest;

	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		FNawatEventPayload Payload;
		Payload.Value = static_cast<float>(CurrentAlert);
		WorldState->BroadcastEvent(NawatTags::Event_AI_AlertChanged, Payload);
	}
}

void UNawatSquadSubsystem::RaiseAlarm(FVector Location)
{
	if (Location.IsNearlyZero())
	{
		if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			Location = PlayerPawn->GetActorLocation();
		}
	}

	TArray<ANawatAIController*> Enemies;
	GatherTeam(ENawatTeam::Enemy, false, Enemies);
	for (ANawatAIController* Enemy : Enemies)
	{
		Enemy->ReceiveAlarm(Location);
	}
}

// ---- Companions ----

void UNawatSquadSubsystem::IssueOrder(ENawatSquadOrder Order, FVector Location, AActor* Target)
{
	TArray<ANawatAIController*> Companions;
	GatherTeam(ENawatTeam::Player, false, Companions);

	for (int32 Index = 0; Index < Companions.Num(); ++Index)
	{
		// Spread the group around a shared destination so they do not fight over one spot.
		FVector MemberLocation = Location;
		if (!Location.IsNearlyZero())
		{
			MemberLocation += NawatSquadPrivate::RingOffset(Index, Companions.Num(), FormationSpacing * 0.7f);
		}
		Companions[Index]->SetOrder(Order, MemberLocation, Target);
	}
}

void UNawatSquadSubsystem::DismissCompanions(FVector Location)
{
	TArray<ANawatAIController*> Companions;
	GatherTeam(ENawatTeam::Player, false, Companions);

	for (ANawatAIController* Companion : Companions)
	{
		Companion->BeginLeaving(Location);
	}

	SetSquadPresent(false);

	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		FNawatEventPayload Payload;
		Payload.Location = Location;
		WorldState->BroadcastEvent(NawatTags::Event_Squad_Dismissed, Payload);
	}
}

void UNawatSquadSubsystem::SummonCompanions(FVector Location)
{
	TArray<ANawatAIController*> Companions;
	GatherTeam(ENawatTeam::Player, false, Companions);
	if (Companions.Num() > 0)
	{
		// Already here: a summon is then just a call to come back.
		SetSquadPresent(true);
		IssueOrder(ENawatSquadOrder::Regroup, FVector::ZeroVector, nullptr);
		return;
	}

	// Companions still walking away from an earlier dismissal are replaced, not doubled.
	GatherTeam(ENawatTeam::Player, true, Companions);
	for (ANawatAIController* Leaving : Companions)
	{
		Leaving->DestroyAgent();
	}

	if (SpawnCompanions(Location) <= 0)
	{
		return;
	}

	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		FNawatEventPayload Payload;
		Payload.Location = Location;
		WorldState->BroadcastEvent(NawatTags::Event_Squad_Summoned, Payload);
	}
}

void UNawatSquadSubsystem::SetCompanionSpawnParams(TSubclassOf<ACharacter> CharacterClass, TSubclassOf<ANawatAIController> ControllerClass, int32 Count, const FNawatLoadout& Loadout)
{
	CompanionClass = CharacterClass;
	CompanionControllerClass = ControllerClass;
	CompanionCount = FMath::Max(Count, 0);
	CompanionLoadout = Loadout;
}

int32 UNawatSquadSubsystem::SpawnCompanions(const FVector& Location)
{
	if (!CompanionClass || CompanionCount <= 0)
	{
		UE_LOG(LogNawat, Warning, TEXT("No companions to spawn: place a NawatSquadSpawner and pick its character class."));
		return 0;
	}

	FVector Origin = Location;
	FRotator Facing = FRotator::ZeroRotator;
	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Facing = FRotator(0.f, PlayerPawn->GetActorRotation().Yaw, 0.f);
		if (Location.IsNearlyZero())
		{
			Origin = PlayerPawn->GetActorLocation() - Facing.Vector() * (FormationSpacing * 1.5f);
		}
	}
	else if (Location.IsNearlyZero())
	{
		UE_LOG(LogNawat, Warning, TEXT("Companions need a location or a player pawn to appear next to; nothing spawned."));
		return 0;
	}

	int32 Spawned = 0;
	for (int32 Index = 0; Index < CompanionCount; ++Index)
	{
		const FVector Desired = Origin + NawatSquadPrivate::RingOffset(Index, CompanionCount, FormationSpacing * 0.7f);
		const FVector SpawnLocation = UNawatAIStatics::FindSpawnLocation(this, CompanionClass, Desired, 0.f);
		const FTransform SpawnTransform(Facing, SpawnLocation);

		if (UNawatAIStatics::SpawnAgent(this, CompanionClass, SpawnTransform, ENawatTeam::Player, PlayerSquadId, CompanionLoadout, nullptr, CompanionControllerClass))
		{
			++Spawned;
		}
	}

	if (Spawned > 0)
	{
		SetSquadPresent(true);
	}
	return Spawned;
}

int32 UNawatSquadSubsystem::GetNumCompanions() const
{
	TArray<ANawatAIController*> Companions;
	GatherTeam(ENawatTeam::Player, false, Companions);
	return Companions.Num();
}

FVector UNawatSquadSubsystem::GetFormationSlot(const ANawatAIController* Member) const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		const APawn* MemberPawn = Member ? Member->GetPawn() : nullptr;
		return MemberPawn ? MemberPawn->GetActorLocation() : FVector::ZeroVector;
	}

	TArray<ANawatAIController*> Companions;
	GatherTeam(ENawatTeam::Player, false, Companions);
	const int32 Index = FMath::Max(Companions.IndexOfByKey(Member), 0);

	// Two columns behind the player, each row a little wider than the one in front of it.
	const int32 Row = Index / 2;
	const float Side = (Index % 2 == 0) ? -1.f : 1.f;
	const FVector LocalOffset(-FormationSpacing * (Row + 1), Side * FormationSpacing * (0.7f + 0.3f * Row), 0.f);

	const FRotator Facing(0.f, PlayerPawn->GetActorRotation().Yaw, 0.f);
	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const FVector Desired = PlayerLocation + Facing.RotateVector(LocalOffset);

	FVector Projected;
	if (UNawatAIStatics::ProjectToNavigation(this, Desired, FVector(150.f, 150.f, 300.f), Projected))
	{
		return Projected;
	}
	return PlayerLocation;
}

void UNawatSquadSubsystem::SetSquadPresent(bool bPresent)
{
	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		WorldState->SetFact(NawatTags::Fact_Squad_Present, bPresent ? 1 : 0);
	}
}
