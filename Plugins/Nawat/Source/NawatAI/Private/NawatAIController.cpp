#include "NawatAIController.h"

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NawatAIStatics.h"
#include "NawatAIWeaponComponent.h"
#include "NawatAgentComponent.h"
#include "NawatHealthComponent.h"
#include "NawatPatrolRoute.h"
#include "NawatSquadSubsystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"

namespace NawatBrain
{
	const float ArriveTolerance = 100.f;
	const float PeekTimeout = 2.f;
	const float PeekStep = 130.f;
	const float PeekEyeHeight = 150.f;
	const float CoverRetryInterval = 4.f;
	const float MoveToCoverTimeout = 12.f;
	const float InvestigateTimeout = 25.f;
	const float LeaveTimeout = 30.f;
	const float FollowRepathDistance = 150.f;
	const float AwarenessAfterLosingTarget = 0.8f;
	const float AwarenessWhenAlerted = 0.6f;
	const int32 MaxFailedPeeks = 2;
}

ANawatAIController::ANawatAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	UAIPerceptionComponent* Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("NawatPerception"));

	// Affiliation is left wide open: hostility is decided by UNawatAgentComponent::IsHostile, so the
	// player's classes do not have to implement the engine team interface.
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("NawatSightConfig"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->SetMaxAge(5.f);

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("NawatHearingConfig"));
	HearingConfig->HearingRange = HearingRange;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->SetMaxAge(5.f);

	Perception->ConfigureSense(*SightConfig);
	Perception->ConfigureSense(*HearingConfig);
	Perception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*Perception);
}

void ANawatAIController::BeginPlay()
{
	Super::BeginPlay();

	if (UAIPerceptionComponent* Perception = GetAIPerceptionComponent())
	{
		Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ANawatAIController::HandlePerceptionUpdated);
	}
}

void ANawatAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseAgent();

	if (UAIPerceptionComponent* Perception = GetAIPerceptionComponent())
	{
		Perception->OnTargetPerceptionUpdated.RemoveDynamic(this, &ANawatAIController::HandlePerceptionUpdated);
	}

	Super::EndPlay(EndPlayReason);
}

void ANawatAIController::InitAgent(FName InSquadId, ANawatPatrolRoute* InPatrolRoute)
{
	PatrolRoute = InPatrolRoute;
	PatrolIndex = 0;
	PatrolDirection = 1;

	if (InSquadId.IsNone() || InSquadId == SquadId)
	{
		return;
	}

	// Changing squad after possession means moving between squads in the subsystem.
	UNawatSquadSubsystem* Squad = GetSquad();
	if (bRegistered && Squad)
	{
		Squad->UnregisterMember(this);
	}

	SquadId = InSquadId;
	if (Agent)
	{
		Agent->SquadId = SquadId;
	}

	if (bRegistered && Squad)
	{
		Squad->RegisterMember(this);
	}
}

void ANawatAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn || GetPawn() != InPawn)
	{
		return;
	}

	bHadPawn = true;
	bLeaving = false;
	HomeLocation = InPawn->GetActorLocation();

	UNawatAIStatics::EnsureAgentComponents(InPawn);
	Agent = InPawn->FindComponentByClass<UNawatAgentComponent>();
	Health = InPawn->FindComponentByClass<UNawatHealthComponent>();
	Weapon = InPawn->FindComponentByClass<UNawatAIWeaponComponent>();

	if (Agent)
	{
		Team = Agent->Team;
		if (SquadId.IsNone())
		{
			SquadId = Agent->SquadId;
		}
		else
		{
			Agent->SquadId = SquadId;
		}
	}

	if (Health)
	{
		Health->OnDamaged.AddUniqueDynamic(this, &ANawatAIController::HandlePawnDamaged);
		Health->OnDeath.AddUniqueDynamic(this, &ANawatAIController::HandlePawnDeath);
	}

	if (ACharacter* MyCharacter = Cast<ACharacter>(InPawn))
	{
		if (UCharacterMovementComponent* Movement = MyCharacter->GetCharacterMovement())
		{
			Movement->NavAgentProps.bCanCrouch = true;
			BaseWalkSpeed = Movement->MaxWalkSpeed;

			if (bManageRotation)
			{
				// Follow the control rotation: the brain's focus while fighting, the path direction otherwise.
				MyCharacter->bUseControllerRotationYaw = false;
				Movement->bOrientRotationToMovement = false;
				Movement->bUseControllerDesiredRotation = true;
			}
		}
	}

	ApplyPerceptionConfig();

	CurrentOrder = (Team == ENawatTeam::Player) ? ENawatSquadOrder::Follow : ENawatSquadOrder::None;
	Awareness = 0.f;
	TargetActor = nullptr;
	VisibleHostiles.Reset();
	BestVisibleHostile = nullptr;

	if (UNawatSquadSubsystem* Squad = GetSquad())
	{
		Squad->RegisterMember(this);
		bRegistered = true;
	}

	if (Health && Health->IsDead())
	{
		SetAIState(ENawatAIState::Dead);
		return;
	}

	// Force the enter logic even when the home state equals the initial value.
	State = ENawatAIState::Dead;
	SetAIState(GetHomeState());
}

void ANawatAIController::OnUnPossess()
{
	ReleaseAgent();

	Super::OnUnPossess();
}

void ANawatAIController::ReleaseAgent()
{
	StopBurst();
	ReleaseCover();

	if (bRegistered)
	{
		bRegistered = false;
		if (UNawatSquadSubsystem* Squad = GetSquad())
		{
			Squad->UnregisterMember(this);
		}
	}

	if (Health)
	{
		Health->OnDamaged.RemoveDynamic(this, &ANawatAIController::HandlePawnDamaged);
		Health->OnDeath.RemoveDynamic(this, &ANawatAIController::HandlePawnDeath);
	}

	Agent = nullptr;
	Health = nullptr;
	Weapon = nullptr;
	VisibleHostiles.Reset();
	BestVisibleHostile = nullptr;
	TargetActor = nullptr;
}

UNawatSquadSubsystem* ANawatAIController::GetSquad() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UNawatSquadSubsystem>() : nullptr;
}

void ANawatAIController::ApplyPerceptionConfig()
{
	UAIPerceptionComponent* Perception = GetAIPerceptionComponent();
	if (!Perception || !SightConfig || !HearingConfig)
	{
		return;
	}

	// The constructor only saw class defaults; a Blueprint subclass may have changed the tunables.
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = FMath::Max(LoseSightRadius, SightRadius);
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;
	HearingConfig->HearingRange = HearingRange;

	Perception->ConfigureSense(*SightConfig);
	Perception->ConfigureSense(*HearingConfig);
}

int32 ANawatAIController::GetAlertLevel() const
{
	if (!GetPawn())
	{
		return 0;
	}

	switch (State)
	{
	case ENawatAIState::Combat:
		return 2;
	case ENawatAIState::Suspicious:
	case ENawatAIState::Search:
		return 1;
	default:
		return 0;
	}
}

// ---- Tick ----

void ANawatAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!GetPawn())
	{
		// The pawn died or was removed; a brain without a body has nothing left to do.
		if (bHadPawn)
		{
			Destroy();
		}
		return;
	}

	if (State == ENawatAIState::Dead)
	{
		return;
	}

	UpdateFiring();

	ThinkAccumulator += DeltaSeconds;
	if (ThinkAccumulator >= ThinkInterval)
	{
		const float Step = ThinkAccumulator;
		ThinkAccumulator = 0.f;
		Think(Step);
	}
}

void ANawatAIController::Think(float DeltaSeconds)
{
	StateTime += DeltaSeconds;
	BurstCooldown = FMath::Max(BurstCooldown - DeltaSeconds, 0.f);
	CombatLockout = FMath::Max(CombatLockout - DeltaSeconds, 0.f);
	OrderMoveCooldown = FMath::Max(OrderMoveCooldown - DeltaSeconds, 0.f);

	if (bLeaving)
	{
		TickLeaving(DeltaSeconds);
		return;
	}

	UpdateSenses(DeltaSeconds);
	UpdateAlertTransitions();

	switch (State)
	{
	case ENawatAIState::Idle:
		TickIdle(DeltaSeconds);
		break;
	case ENawatAIState::Patrol:
		TickPatrol(DeltaSeconds);
		break;
	case ENawatAIState::Suspicious:
		TickSuspicious(DeltaSeconds);
		break;
	case ENawatAIState::Search:
		TickSearch(DeltaSeconds);
		break;
	case ENawatAIState::Combat:
		TickCombat(DeltaSeconds);
		break;
	default:
		break;
	}
}

void ANawatAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// An aborted move was replaced by a newer request and says nothing about reachability.
	if (Result.Code == EPathFollowingResult::Blocked || Result.Code == EPathFollowingResult::OffPath || Result.Code == EPathFollowingResult::Invalid)
	{
		bLastMoveFailed = true;
	}
}

// ---- State machine ----

ENawatAIState ANawatAIController::GetHomeState() const
{
	if (Team == ENawatTeam::Player)
	{
		return ENawatAIState::Idle;
	}
	return (PatrolRoute || bWanderWithoutRoute) ? ENawatAIState::Patrol : ENawatAIState::Idle;
}

void ANawatAIController::SetAIState(ENawatAIState NewState)
{
	if (NewState == State)
	{
		return;
	}

	const ENawatAIState OldState = State;
	ExitState(OldState);
	State = NewState;
	StateTime = 0.f;
	EnterState(NewState);

	if (UNawatSquadSubsystem* Squad = GetSquad())
	{
		Squad->RefreshAlert();
	}

	OnStateChanged.Broadcast(this, NewState, OldState);
}

void ANawatAIController::ExitState(ENawatAIState OldState)
{
	if (OldState == ENawatAIState::Combat)
	{
		StopBurst();
		ReleaseCover();
		SetCrouched(false);

		if (UNawatSquadSubsystem* Squad = GetSquad())
		{
			Squad->ReleaseAttackToken(this);
		}
	}

	ClearFocus(EAIFocusPriority::Gameplay);
}

void ANawatAIController::EnterState(ENawatAIState NewState)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	switch (NewState)
	{
	case ENawatAIState::Idle:
		ApplySpeed(true);
		bOrderMoveStarted = false;
		OrderMoveCooldown = 0.f;
		break;

	case ENawatAIState::Patrol:
		ApplySpeed(true);
		bPatrolEnRoute = false;
		PatrolWait = 0.f;
		break;

	case ENawatAIState::Suspicious:
		ApplySpeed(true);
		StopMovement();
		RestartInvestigation(InvestigateLocation);
		break;

	case ENawatAIState::Search:
		ApplySpeed(false);
		SearchPause = 1.5f;
		GoTo(LastKnownLocation, 80.f);
		break;

	case ENawatAIState::Combat:
		ApplySpeed(false);
		Awareness = 1.f;
		CombatPhase = ECombatPhase::SeekCover;
		PhaseTimer = 0.f;
		BurstsSinceRelocate = 0;
		FailedPeeks = 0;
		TimeTargetLastSeen = Now;
		if (UNawatSquadSubsystem* Squad = GetSquad())
		{
			Squad->NotifyCombatStarted(this, TargetActor.Get(), LastKnownLocation);
		}
		break;

	case ENawatAIState::Dead:
		StopMovement();
		ReleaseCover();
		if (bRegistered)
		{
			bRegistered = false;
			if (UNawatSquadSubsystem* Squad = GetSquad())
			{
				Squad->UnregisterMember(this);
			}
		}
		break;

	default:
		break;
	}
}

// ---- Senses ----

bool ANawatAIController::IsValidTarget(const AActor* Actor) const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || !IsValid(Actor) || !UNawatAgentComponent::IsHostile(MyPawn, Actor))
	{
		return false;
	}

	const UNawatHealthComponent* TargetHealth = Actor->FindComponentByClass<UNawatHealthComponent>();
	return !TargetHealth || !TargetHealth->IsDead();
}

bool ANawatAIController::IsVisibleHostile(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& Entry : VisibleHostiles)
	{
		if (Entry.Get() == Actor)
		{
			return true;
		}
	}
	return false;
}

void ANawatAIController::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || !Actor || State == ENawatAIState::Dead)
	{
		return;
	}

	if (!UNawatAgentComponent::IsHostile(MyPawn, Actor))
	{
		return;
	}

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			VisibleHostiles.AddUnique(TWeakObjectPtr<AActor>(Actor));
		}
		else
		{
			VisibleHostiles.Remove(TWeakObjectPtr<AActor>(Actor));
		}
	}
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() && Stimulus.WasSuccessfullySensed())
	{
		HearHostile(Actor, Stimulus.StimulusLocation);
	}
}

void ANawatAIController::HearHostile(AActor* Actor, const FVector& Location)
{
	// Companions react to what they see; chasing every shot would pull them off the player.
	if (Team != ENawatTeam::Enemy || bLeaving)
	{
		return;
	}

	if (State == ENawatAIState::Combat)
	{
		if (Actor == TargetActor.Get() && !IsVisibleHostile(Actor))
		{
			LastKnownLocation = Location;
		}
		return;
	}

	LastKnownLocation = Location;
	Awareness = FMath::Max(Awareness, FMath::Min(SuspiciousThreshold + 0.2f, 0.95f));

	if (State == ENawatAIState::Search)
	{
		StateTime = 0.f;
		GoTo(Location, 80.f);
	}
	else if (State == ENawatAIState::Suspicious)
	{
		RestartInvestigation(Location);
	}
	else
	{
		InvestigateLocation = Location;
		SetAIState(ENawatAIState::Suspicious);
	}
}

void ANawatAIController::HandlePawnDamaged(UNawatHealthComponent* Component, float Damage, AActor* InstigatorActor)
{
	if (State == ENawatAIState::Dead || bLeaving || !IsValidTarget(InstigatorActor))
	{
		return;
	}

	if (State == ENawatAIState::Combat || CurrentOrder == ENawatSquadOrder::Regroup)
	{
		return;
	}

	// Being shot leaves no doubt.
	TargetActor = InstigatorActor;
	LastKnownLocation = InstigatorActor->GetActorLocation();
	CombatLockout = 0.f;
	SetAIState(ENawatAIState::Combat);
}

void ANawatAIController::HandlePawnDeath(UNawatHealthComponent* Component, AActor* Killer)
{
	SetAIState(ENawatAIState::Dead);
}

void ANawatAIController::UpdateSenses(float DeltaSeconds)
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	const FVector MyLocation = MyPawn->GetActorLocation();
	AActor* Best = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (int32 Index = VisibleHostiles.Num() - 1; Index >= 0; --Index)
	{
		AActor* Candidate = VisibleHostiles[Index].Get();
		if (!IsValidTarget(Candidate))
		{
			VisibleHostiles.RemoveAtSwap(Index);
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(MyLocation, Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Best = Candidate;
		}
	}

	BestVisibleHostile = Best;

	if (State == ENawatAIState::Combat)
	{
		Awareness = 1.f;
		return;
	}

	if (!Best)
	{
		Awareness = FMath::Max(Awareness - AwarenessDecay * DeltaSeconds, 0.f);
		return;
	}

	if (Team == ENawatTeam::Player)
	{
		// Companions are not part of the stealth game.
		Awareness = 1.f;
		return;
	}

	const FVector BestLocation = Best->GetActorLocation();
	const float RangeAlpha = FMath::Clamp(FMath::Sqrt(BestDistanceSquared) / FMath::Max(SightRadius, 1.f), 0.f, 1.f);
	float Gain = FMath::Lerp(AwarenessGainNear, AwarenessGainFar, RangeAlpha);

	const FVector ToTarget = (BestLocation - MyLocation).GetSafeNormal2D();
	const float InnerConeCos = FMath::Cos(FMath::DegreesToRadians(PeripheralVisionAngle * 0.5f));
	if (FVector::DotProduct(MyPawn->GetActorForwardVector(), ToTarget) < InnerConeCos)
	{
		Gain *= PeripheralGainScale;
	}

	Awareness = FMath::Min(Awareness + Gain * DeltaSeconds, 1.f);
}

void ANawatAIController::UpdateAlertTransitions()
{
	if (State == ENawatAIState::Combat || State == ENawatAIState::Dead)
	{
		return;
	}

	if (CurrentOrder == ENawatSquadOrder::Regroup || CombatLockout > 0.f)
	{
		return;
	}

	AActor* Seen = BestVisibleHostile.Get();
	if (!Seen)
	{
		return;
	}

	if (Awareness >= 1.f)
	{
		TargetActor = Seen;
		LastKnownLocation = Seen->GetActorLocation();
		SetAIState(ENawatAIState::Combat);
	}
	else if (Awareness >= SuspiciousThreshold && (State == ENawatAIState::Idle || State == ENawatAIState::Patrol))
	{
		InvestigateLocation = Seen->GetActorLocation();
		SetAIState(ENawatAIState::Suspicious);
	}
}

// ---- Calm states ----

void ANawatAIController::TickIdle(float DeltaSeconds)
{
	if (Team == ENawatTeam::Player)
	{
		TickCompanion(DeltaSeconds);
		return;
	}

	// A guard without a route goes back to its post after an alert.
	if (!HasArrived(HomeLocation, 150.f) && !IsPathMoving() && OrderMoveCooldown <= 0.f)
	{
		OrderMoveCooldown = 3.f;
		GoTo(HomeLocation, 50.f);
	}
}

void ANawatAIController::TickPatrol(float DeltaSeconds)
{
	if (PatrolWait > 0.f)
	{
		PatrolWait -= DeltaSeconds;
		return;
	}

	if (IsPathMoving())
	{
		return;
	}

	const bool bUseRoute = PatrolRoute && PatrolRoute->GetNumPoints() > 0;

	if (bPatrolEnRoute)
	{
		// The move ended: reached the stop, or could not get closer. Either way wait, then go on.
		bPatrolEnRoute = false;
		if (bUseRoute)
		{
			PatrolWait = PatrolRoute->GetWait(PatrolIndex);
			PatrolIndex = PatrolRoute->GetNextIndex(PatrolIndex, PatrolDirection);
		}
		else
		{
			PatrolWait = FMath::FRandRange(WanderWaitMin, FMath::Max(WanderWaitMin, WanderWaitMax));
		}
		LookAround();
		return;
	}

	FVector Destination;
	if (bUseRoute)
	{
		Destination = PatrolRoute->GetPointLocation(PatrolIndex);
	}
	else if (!PickReachablePoint(HomeLocation, WanderRadius, Destination))
	{
		PatrolWait = 2.f;
		return;
	}

	ClearFocus(EAIFocusPriority::Gameplay);
	if (GoTo(Destination, 50.f))
	{
		bPatrolEnRoute = true;
	}
	else
	{
		// Unreachable stop: skip it instead of retrying every think.
		PatrolWait = 2.f;
		if (bUseRoute)
		{
			PatrolIndex = PatrolRoute->GetNextIndex(PatrolIndex, PatrolDirection);
		}
	}
}

void ANawatAIController::RestartInvestigation(const FVector& Location)
{
	InvestigateLocation = Location;
	bInvestigateMoveStarted = false;
	bInvestigateArrived = false;
	LookTimer = SuspiciousLookTime;
	LookAroundTimer = 0.f;
	StateTime = 0.f;
	SetFocalPoint(Location);
}

void ANawatAIController::TickSuspicious(float DeltaSeconds)
{
	if (const AActor* Seen = BestVisibleHostile.Get())
	{
		// Stand and stare while the meter fills.
		InvestigateLocation = Seen->GetActorLocation();
		SetFocalPoint(InvestigateLocation);
		if (IsPathMoving())
		{
			StopMovement();
		}
		bInvestigateMoveStarted = false;
		bInvestigateArrived = false;
		return;
	}

	if (StateTime < SuspiciousReactionDelay)
	{
		return;
	}

	if (!bInvestigateArrived)
	{
		if (HasArrived(InvestigateLocation, 150.f) || StateTime > NawatBrain::InvestigateTimeout)
		{
			bInvestigateArrived = true;
		}
		else if (!IsPathMoving())
		{
			if (bInvestigateMoveStarted)
			{
				// The move ended short of the spot; this is as close as it gets.
				bInvestigateArrived = true;
			}
			else
			{
				bInvestigateMoveStarted = true;
				ClearFocus(EAIFocusPriority::Gameplay);
				if (!GoTo(InvestigateLocation, 80.f))
				{
					bInvestigateArrived = true;
				}
			}
		}

		if (bInvestigateArrived)
		{
			LookTimer = SuspiciousLookTime;
			LookAroundTimer = 0.f;
		}
		return;
	}

	LookTimer -= DeltaSeconds;
	LookAroundTimer -= DeltaSeconds;
	if (LookAroundTimer <= 0.f)
	{
		LookAroundTimer = 1.3f;
		LookAround();
	}

	if (LookTimer <= 0.f)
	{
		Awareness = 0.f;
		SetAIState(GetHomeState());
	}
}

void ANawatAIController::TickSearch(float DeltaSeconds)
{
	if (StateTime > SearchDuration)
	{
		Awareness = 0.f;
		TargetActor = nullptr;
		SetAIState(GetHomeState());
		return;
	}

	if (IsPathMoving())
	{
		return;
	}

	SearchPause -= DeltaSeconds;
	if (SearchPause > 0.f)
	{
		return;
	}

	SearchPause = FMath::FRandRange(1.5f, 3.f);

	FVector Destination;
	if (PickReachablePoint(LastKnownLocation, SearchRadius, Destination))
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		GoTo(Destination, 60.f);
	}
	else
	{
		LookAround();
	}
}

// ---- Combat ----

void ANawatAIController::TickCombat(float DeltaSeconds)
{
	APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!MyPawn || !World)
	{
		return;
	}

	AActor* Target = TargetActor.Get();
	AActor* Seen = BestVisibleHostile.Get();

	// Keep the current target while it is in view; otherwise take whoever is.
	if (!IsValidTarget(Target) || (Seen && !IsVisibleHostile(Target)))
	{
		Target = IsValidTarget(Seen) ? Seen : (IsValidTarget(Target) ? Target : nullptr);
		TargetActor = Target;
	}

	if (!Target)
	{
		LoseTarget();
		return;
	}

	const float Now = World->GetTimeSeconds();
	const FVector TargetLocation = Target->GetActorLocation();
	const bool bOrderedTarget = CurrentOrder == ENawatSquadOrder::Attack && OrderTarget.Get() == Target;
	UNawatSquadSubsystem* Squad = GetSquad();

	if (IsVisibleHostile(Target) || bOrderedTarget)
	{
		TimeTargetLastSeen = Now;
		LastKnownLocation = TargetLocation;
		if (Squad)
		{
			Squad->ReportTarget(this, Target, TargetLocation);
		}
		SetFocus(Target);
	}
	else
	{
		float LastSeen = TimeTargetLastSeen;
		FVector SharedLocation;
		float SharedTime = 0.f;
		if (Squad && Squad->GetSharedTarget(this, Target, SharedLocation, SharedTime) && SharedTime > LastSeen)
		{
			LastSeen = SharedTime;
			LastKnownLocation = SharedLocation;
		}

		if (Now - LastSeen > LoseTargetTime)
		{
			LoseTarget();
			return;
		}

		SetFocalPoint(LastKnownLocation);
	}

	// A following companion does not chase a fight away from the player.
	if (Team == ENawatTeam::Player && CurrentOrder == ENawatSquadOrder::Follow)
	{
		const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (PlayerPawn && FVector::Dist(PlayerPawn->GetActorLocation(), MyPawn->GetActorLocation()) > LeashDistance)
		{
			CombatLockout = 4.f;
			TargetActor = nullptr;
			SetAIState(GetHomeState());
			return;
		}
	}

	if (Weapon && Weapon->NeedsReload() && !Weapon->IsReloading())
	{
		StopBurst();
		Weapon->StartReload();
		if (CombatPhase == ECombatPhase::Peek)
		{
			BeginHide();
		}
	}
	const bool bReloading = Weapon && Weapon->IsReloading();

	switch (CombatPhase)
	{
	case ECombatPhase::SeekCover:
		if (!TryTakeCover())
		{
			EnterNoCover();
		}
		break;

	case ECombatPhase::MoveToCover:
		PhaseTimer += DeltaSeconds;
		if (HasArrived(Cover.Location, NawatBrain::ArriveTolerance) || (!IsPathMoving() && HasArrived(Cover.Location, 250.f)))
		{
			BeginHide();
		}
		else if (!IsPathMoving() || bLastMoveFailed || PhaseTimer > NawatBrain::MoveToCoverTimeout)
		{
			EnterNoCover();
		}
		else if (!bReloading)
		{
			TryStartBurst(Target);
		}
		break;

	case ECombatPhase::Hidden:
		PhaseTimer -= DeltaSeconds;
		if (bReloading || bBursting || PhaseTimer > 0.f)
		{
			break;
		}
		{
			const UNawatCoverSubsystem* Covers = World->GetSubsystem<UNawatCoverSubsystem>();
			const bool bStillHidden = !Covers || Covers->IsHidden(MyPawn, LastKnownLocation, Cover.Location);
			const bool bRoleMoves = SquadRole != ENawatSquadRole::Suppress || bOrderedTarget;
			if (!bStillHidden || (bRoleMoves && BurstsSinceRelocate >= BurstsBeforeRelocate))
			{
				Relocate();
			}
			else
			{
				BeginPeek(Target);
			}
		}
		break;

	case ECombatPhase::Peek:
		PhaseTimer += DeltaSeconds;
		if (bBursting)
		{
			break;
		}
		if (bFiredThisPeek)
		{
			BeginHide();
		}
		else if (!bReloading && TryStartBurst(Target))
		{
			bFiredThisPeek = true;
			FailedPeeks = 0;
		}
		else if (PhaseTimer > NawatBrain::PeekTimeout)
		{
			// With a clear line the shot was only withheld (no attack token yet), so the cover is fine.
			// Without one, twice in a row means this cover is useless against the target.
			if (LineOfSightTo(Target))
			{
				TimeTargetLastSeen = Now;
				LastKnownLocation = TargetLocation;
				FailedPeeks = 0;
			}
			else
			{
				++FailedPeeks;
			}

			if (FailedPeeks >= NawatBrain::MaxFailedPeeks)
			{
				EnterNoCover();
			}
			else
			{
				BeginHide();
			}
		}
		break;

	case ECombatPhase::NoCover:
		PhaseTimer -= DeltaSeconds;
		CoverRetryTimer -= DeltaSeconds;
		if (!bReloading)
		{
			TryStartBurst(Target);
		}
		if (CoverRetryTimer <= 0.f && !bBursting)
		{
			CoverRetryTimer = NawatBrain::CoverRetryInterval;
			if (TryTakeCover())
			{
				break;
			}
		}
		if (PhaseTimer <= 0.f)
		{
			PhaseTimer = FMath::FRandRange(1.5f, 3.f);
			Reposition(Target, LineOfSightTo(Target));
		}
		break;

	default:
		break;
	}
}

void ANawatAIController::LoseTarget()
{
	StopBurst();

	if (Team == ENawatTeam::Player)
	{
		// Companions do not hunt; they fall back into the order they were given.
		if (CurrentOrder == ENawatSquadOrder::Attack)
		{
			CurrentOrder = ENawatSquadOrder::Follow;
		}
		TargetActor = nullptr;
		OrderTarget = nullptr;
		Awareness = 0.f;
		SetAIState(GetHomeState());
		return;
	}

	Awareness = NawatBrain::AwarenessAfterLosingTarget;
	SetAIState(ENawatAIState::Search);
}

FVector ANawatAIController::GetCoverSearchOrigin(float& OutRadius) const
{
	OutRadius = CoverSearchRadius;

	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return FVector::ZeroVector;
	}

	const FVector MyLocation = MyPawn->GetActorLocation();

	if (Team == ENawatTeam::Player)
	{
		if (CurrentOrder == ENawatSquadOrder::Hold)
		{
			OutRadius = FMath::Min(CoverSearchRadius, 500.f);
			return HoldLocation;
		}

		if (CurrentOrder == ENawatSquadOrder::Follow)
		{
			if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				OutRadius = FMath::Min(CoverSearchRadius, 1200.f);
				return PlayerPawn->GetActorLocation();
			}
		}
	}

	const FVector ToThreat = LastKnownLocation - MyLocation;

	if (SquadRole == ENawatSquadRole::Advance || CurrentOrder == ENawatSquadOrder::Attack)
	{
		return MyLocation + ToThreat * 0.4f;
	}

	if (SquadRole == ENawatSquadRole::Flank)
	{
		// Swing around the threat while keeping roughly the same distance to it.
		const FVector FromThreat = (-ToThreat).GetSafeNormal2D();
		const float Distance = FMath::Max(ToThreat.Size2D() * 0.8f, 700.f);
		return LastKnownLocation + FromThreat.RotateAngleAxis(60.f * FlankSign, FVector::UpVector) * Distance;
	}

	return MyLocation;
}

bool ANawatAIController::TryTakeCover()
{
	APawn* MyPawn = GetPawn();
	UWorld* World = GetWorld();
	UNawatCoverSubsystem* Covers = World ? World->GetSubsystem<UNawatCoverSubsystem>() : nullptr;
	if (!MyPawn || !Covers)
	{
		return false;
	}

	float Radius = CoverSearchRadius;
	const FVector Origin = GetCoverSearchOrigin(Radius);

	FNawatCoverResult Result;
	if (!Covers->FindCoverNear(MyPawn, Origin, LastKnownLocation, Radius, Result))
	{
		return false;
	}

	Covers->ClaimCover(MyPawn, Result);
	Cover = Result;
	bHasCover = true;

	SetCrouched(false);
	if (!GoTo(Result.Location, 30.f))
	{
		ReleaseCover();
		return false;
	}

	CombatPhase = ECombatPhase::MoveToCover;
	PhaseTimer = 0.f;
	return true;
}

void ANawatAIController::ReleaseCover()
{
	if (!bHasCover)
	{
		return;
	}

	bHasCover = false;

	const APawn* MyPawn = GetPawn();
	const UWorld* World = GetWorld();
	UNawatCoverSubsystem* Covers = World ? World->GetSubsystem<UNawatCoverSubsystem>() : nullptr;
	if (MyPawn && Covers)
	{
		Covers->ReleaseCover(MyPawn);
	}
}

void ANawatAIController::BeginHide()
{
	// A burst started on the way in would otherwise be fired into the cover itself.
	StopBurst();

	CombatPhase = ECombatPhase::Hidden;
	PhaseTimer = FMath::FRandRange(HideTimeMin, FMath::Max(HideTimeMin, HideTimeMax));

	if (Cover.bCrouch)
	{
		SetCrouched(true);
	}
	else if (!HasArrived(Cover.Location, 60.f))
	{
		GoTo(Cover.Location, 30.f);
	}
}

void ANawatAIController::BeginPeek(const AActor* Target)
{
	CombatPhase = ECombatPhase::Peek;
	PhaseTimer = 0.f;
	bFiredThisPeek = false;

	if (Cover.bCrouch)
	{
		SetCrouched(false);
		return;
	}

	// Full-height cover: step out sideways to where the target can be seen.
	FVector PeekLocation;
	if (ComputePeekLocation(Target, PeekLocation))
	{
		GoTo(PeekLocation, 20.f);
	}
}

bool ANawatAIController::ComputePeekLocation(const AActor* Target, FVector& OutLocation) const
{
	const APawn* MyPawn = GetPawn();
	const UWorld* World = GetWorld();
	if (!MyPawn || !World || !Target)
	{
		return false;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const FVector ToTarget = (TargetLocation - Cover.Location).GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget);
	const float FirstSide = FMath::RandBool() ? 1.f : -1.f;

	FCollisionQueryParams Params(FName(TEXT("NawatPeek")), false, MyPawn);
	Params.AddIgnoredActor(Target);

	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		const float Side = (Attempt == 0) ? FirstSide : -FirstSide;
		const FVector Candidate = Cover.Location + Right * Side * NawatBrain::PeekStep;

		FVector Projected;
		if (!UNawatAIStatics::ProjectToNavigation(this, Candidate, FVector(60.f, 60.f, 200.f), Projected))
		{
			continue;
		}

		const FVector Eye = Projected + FVector(0.f, 0.f, NawatBrain::PeekEyeHeight);
		if (!World->LineTraceTestByChannel(Eye, TargetLocation, ECC_Visibility, Params))
		{
			OutLocation = Projected;
			return true;
		}
	}

	return false;
}

void ANawatAIController::Relocate()
{
	ReleaseCover();
	SetCrouched(false);
	BurstsSinceRelocate = 0;
	FailedPeeks = 0;
	CombatPhase = ECombatPhase::SeekCover;
}

void ANawatAIController::EnterNoCover()
{
	ReleaseCover();
	SetCrouched(false);
	BurstsSinceRelocate = 0;
	FailedPeeks = 0;
	CombatPhase = ECombatPhase::NoCover;
	PhaseTimer = 0.f;
	CoverRetryTimer = NawatBrain::CoverRetryInterval;
}

void ANawatAIController::Reposition(const AActor* Target, bool bHasLineOfSight)
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || !Target)
	{
		return;
	}

	if (!bHasLineOfSight)
	{
		// Push toward where the target was until it comes into view.
		GoTo(LastKnownLocation, 300.f);
		return;
	}

	const FVector MyLocation = MyPawn->GetActorLocation();
	const FVector ToTarget = Target->GetActorLocation() - MyLocation;
	const float Distance = ToTarget.Size2D();
	const FVector Forward = ToTarget.GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

	float Advance = 0.f;
	if (Distance > PreferredRange)
	{
		Advance = 300.f;
	}
	else if (Distance < PreferredRange * 0.3f)
	{
		Advance = -300.f;
	}

	const float Side = (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(250.f, 450.f);
	const FVector Desired = MyLocation + Right * Side + Forward * Advance;

	FVector Projected;
	if (UNawatAIStatics::ProjectToNavigation(this, Desired, FVector(150.f, 150.f, 300.f), Projected))
	{
		GoTo(Projected, 40.f);
	}
}

bool ANawatAIController::TryStartBurst(AActor* Target)
{
	const APawn* MyPawn = GetPawn();
	const UWorld* World = GetWorld();
	if (bBursting || BurstCooldown > 0.f || !MyPawn || !World || !Weapon || !Target || !Weapon->CanFire())
	{
		return false;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	if (FVector::Dist(MyPawn->GetActorLocation(), TargetLocation) > Weapon->Range || !LineOfSightTo(Target))
	{
		return false;
	}

	UNawatSquadSubsystem* Squad = GetSquad();
	if (Squad && !Squad->RequestAttackToken(this))
	{
		return false;
	}

	bBursting = true;
	ShotsLeft = FMath::RandRange(FMath::Max(Weapon->BurstSizeMin, 1), FMath::Max(Weapon->BurstSizeMax, Weapon->BurstSizeMin));
	TimeTargetLastSeen = World->GetTimeSeconds();
	LastKnownLocation = TargetLocation;
	return true;
}

void ANawatAIController::StopBurst()
{
	if (!bBursting)
	{
		return;
	}

	bBursting = false;
	ShotsLeft = 0;
	BurstCooldown = (Weapon ? Weapon->BurstPause : 1.f) * FMath::FRandRange(0.8f, 1.3f);
	++BurstsSinceRelocate;

	if (UNawatSquadSubsystem* Squad = GetSquad())
	{
		Squad->ReleaseAttackToken(this);
	}
}

void ANawatAIController::UpdateFiring()
{
	if (!bBursting)
	{
		return;
	}

	const AActor* Target = TargetActor.Get();
	if (State != ENawatAIState::Combat || !Weapon || !IsValidTarget(Target) || Weapon->NeedsReload() || Weapon->IsReloading())
	{
		StopBurst();
		return;
	}

	if (Weapon->CanFire())
	{
		Weapon->FireAt(Target->GetActorLocation());
		if (--ShotsLeft <= 0)
		{
			StopBurst();
		}
	}
}

// ---- Squad interface ----

void ANawatAIController::SetSquadRole(ENawatSquadRole InRole, float InFlankSign)
{
	SquadRole = InRole;
	FlankSign = InFlankSign;
}

void ANawatAIController::ReceiveSquadAlert(AActor* Target, const FVector& Location)
{
	if (!GetPawn() || State == ENawatAIState::Dead || State == ENawatAIState::Combat || bLeaving)
	{
		return;
	}

	if (CurrentOrder == ENawatSquadOrder::Regroup || CombatLockout > 0.f)
	{
		return;
	}

	if (IsValidTarget(Target) && IsVisibleHostile(Target))
	{
		TargetActor = Target;
		LastKnownLocation = Location;
		SetAIState(ENawatAIState::Combat);
		return;
	}

	// Companions hold their formation until they see the threat themselves.
	if (Team != ENawatTeam::Enemy)
	{
		return;
	}

	Awareness = FMath::Max(Awareness, NawatBrain::AwarenessWhenAlerted);
	LastKnownLocation = Location;

	if (State == ENawatAIState::Suspicious)
	{
		RestartInvestigation(Location);
	}
	else
	{
		InvestigateLocation = Location;
		SetAIState(ENawatAIState::Suspicious);
	}
}

void ANawatAIController::ReceiveAlarm(const FVector& Location)
{
	if (!GetPawn() || Team != ENawatTeam::Enemy || State == ENawatAIState::Dead || State == ENawatAIState::Combat || bLeaving)
	{
		return;
	}

	AActor* Seen = BestVisibleHostile.Get();
	if (IsValidTarget(Seen))
	{
		TargetActor = Seen;
		LastKnownLocation = Seen->GetActorLocation();
		SetAIState(ENawatAIState::Combat);
		return;
	}

	LastKnownLocation = Location;
	Awareness = NawatBrain::AwarenessAfterLosingTarget;

	if (State == ENawatAIState::Search)
	{
		StateTime = 0.f;
		GoTo(Location, 80.f);
	}
	else
	{
		SetAIState(ENawatAIState::Search);
	}
}

// ---- Companions ----

void ANawatAIController::SetOrder(ENawatSquadOrder Order, FVector Location, AActor* Target)
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || Team != ENawatTeam::Player || State == ENawatAIState::Dead || bLeaving)
	{
		return;
	}

	CurrentOrder = (Order == ENawatSquadOrder::None) ? ENawatSquadOrder::Follow : Order;
	OrderTarget = Target;
	OrderLocation = Location;
	bOrderMoveStarted = false;
	OrderMoveCooldown = 0.f;
	CombatLockout = 0.f;

	switch (CurrentOrder)
	{
	case ENawatSquadOrder::Hold:
		HoldLocation = Location.IsNearlyZero() ? MyPawn->GetActorLocation() : Location;
		break;

	case ENawatSquadOrder::Regroup:
		// Drop everything, including a fight.
		TargetActor = nullptr;
		SetAIState(ENawatAIState::Idle);
		break;

	case ENawatSquadOrder::Attack:
		if (IsValidTarget(Target))
		{
			TargetActor = Target;
			LastKnownLocation = Target->GetActorLocation();
			if (State == ENawatAIState::Combat)
			{
				Relocate();
			}
			else
			{
				SetAIState(ENawatAIState::Combat);
			}
		}
		else if (State == ENawatAIState::Combat && !Location.IsNearlyZero())
		{
			// A location order pulls the companion out of its current fight to move there.
			TargetActor = nullptr;
			CombatLockout = 2.f;
			SetAIState(ENawatAIState::Idle);
		}
		break;

	default:
		break;
	}

	if (State == ENawatAIState::Combat && CurrentOrder != ENawatSquadOrder::Attack)
	{
		// Cover is chosen relative to the order, so pick again.
		Relocate();
	}
}

void ANawatAIController::TickCompanion(float DeltaSeconds)
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	const FVector MyLocation = MyPawn->GetActorLocation();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	switch (CurrentOrder)
	{
	case ENawatSquadOrder::Attack:
	{
		AActor* Ordered = OrderTarget.Get();
		if (IsValidTarget(Ordered))
		{
			if (CombatLockout <= 0.f)
			{
				TargetActor = Ordered;
				LastKnownLocation = Ordered->GetActorLocation();
				SetAIState(ENawatAIState::Combat);
			}
			break;
		}

		if (OrderLocation.IsNearlyZero())
		{
			CurrentOrder = ENawatSquadOrder::Follow;
			break;
		}

		const bool bArrived = HasArrived(OrderLocation, 150.f);
		const bool bStoppedShort = bOrderMoveStarted && !IsPathMoving();
		if (bArrived || bStoppedShort)
		{
			// Arrived, or as close as the navmesh allows: keep the position.
			CurrentOrder = ENawatSquadOrder::Hold;
			HoldLocation = bArrived ? OrderLocation : MyLocation;
		}
		else if (!bOrderMoveStarted)
		{
			bOrderMoveStarted = true;
			if (!GoTo(OrderLocation, 80.f))
			{
				CurrentOrder = ENawatSquadOrder::Hold;
				HoldLocation = MyLocation;
			}
		}
		break;
	}

	case ENawatSquadOrder::Hold:
	{
		// Do not pin the player in a doorway: step away and hold the new spot.
		if (PlayerPawn && PersonalSpace > 0.f && FVector::Dist2D(PlayerPawn->GetActorLocation(), MyLocation) < PersonalSpace && !IsPathMoving())
		{
			FVector Away = (MyLocation - PlayerPawn->GetActorLocation()).GetSafeNormal2D();
			if (Away.IsNearlyZero())
			{
				Away = -PlayerPawn->GetActorForwardVector();
			}

			FVector Projected;
			if (UNawatAIStatics::ProjectToNavigation(this, MyLocation + Away * 250.f, FVector(150.f, 150.f, 300.f), Projected))
			{
				HoldLocation = Projected;
			}
		}

		if (!HasArrived(HoldLocation, 120.f) && !IsPathMoving() && OrderMoveCooldown <= 0.f)
		{
			OrderMoveCooldown = 1.f;
			GoTo(HoldLocation, 50.f);
		}
		break;
	}

	case ENawatSquadOrder::Regroup:
	case ENawatSquadOrder::Follow:
	default:
	{
		if (!PlayerPawn)
		{
			break;
		}

		const UNawatSquadSubsystem* Squad = GetSquad();
		const FVector Slot = Squad ? Squad->GetFormationSlot(this) : PlayerPawn->GetActorLocation();
		const float PlayerDistance = FVector::Dist2D(PlayerPawn->GetActorLocation(), MyLocation);
		const bool bRegrouping = CurrentOrder == ENawatSquadOrder::Regroup;

		if (bRegrouping && (PlayerDistance <= FollowDistance || HasArrived(Slot, 200.f)))
		{
			CurrentOrder = ENawatSquadOrder::Follow;
			break;
		}

		// Inside the comfortable band the companion stands still, so looking around does not drag it about.
		// Back-row slots lie beyond FollowDistance, so the band is never tighter than the slot itself.
		const float SlotDistance = FVector::Dist2D(PlayerPawn->GetActorLocation(), Slot);
		const bool bTooFar = PlayerDistance > FMath::Max(FollowDistance, SlotDistance + 150.f);
		const bool bTooClose = PersonalSpace > 0.f && PlayerDistance < PersonalSpace;
		if (!bRegrouping && !bTooFar && !bTooClose)
		{
			break;
		}

		const bool bSlotMoved = FVector::DistSquared(Slot, LastFollowDestination) > FMath::Square(NawatBrain::FollowRepathDistance);
		if (OrderMoveCooldown <= 0.f && (!IsPathMoving() || bSlotMoved) && !HasArrived(Slot, 80.f))
		{
			OrderMoveCooldown = 0.4f;
			LastFollowDestination = Slot;
			if (!GoTo(Slot, 40.f))
			{
				OrderMoveCooldown = 1.5f;
			}
		}
		break;
	}
	}
}

void ANawatAIController::BeginLeaving(const FVector& Location)
{
	if (bLeaving)
	{
		return;
	}

	// Leave the squad first so orders, formation slots and alerts no longer count this agent.
	SetAIState(ENawatAIState::Idle);
	bLeaving = true;
	LeaveLocation = Location;
	LeaveTimer = 0.f;
	TargetActor = nullptr;

	if (bRegistered)
	{
		bRegistered = false;
		if (UNawatSquadSubsystem* Squad = GetSquad())
		{
			Squad->UnregisterMember(this);
		}
	}

	if (Location.IsNearlyZero() || !GoTo(Location, 80.f))
	{
		DestroyAgent();
	}
}

void ANawatAIController::TickLeaving(float DeltaSeconds)
{
	LeaveTimer += DeltaSeconds;

	const bool bStopped = LeaveTimer > 1.f && !IsPathMoving();
	if (HasArrived(LeaveLocation, 200.f) || bStopped || LeaveTimer > NawatBrain::LeaveTimeout)
	{
		DestroyAgent();
	}
}

void ANawatAIController::DestroyAgent()
{
	APawn* MyPawn = GetPawn();
	ReleaseAgent();

	if (MyPawn)
	{
		MyPawn->Destroy();
	}
	Destroy();
}

// ---- Movement helpers ----

bool ANawatAIController::GoTo(const FVector& Destination, float AcceptanceRadius)
{
	if (!GetPawn())
	{
		return false;
	}

	bLastMoveFailed = false;

	// No strafing: without a gameplay focus the pawn then faces along its path.
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Destination, AcceptanceRadius, false, true, true, false, TSubclassOf<UNavigationQueryFilter>(), true);
	return Result != EPathFollowingRequestResult::Failed;
}

bool ANawatAIController::IsPathMoving() const
{
	return GetMoveStatus() != EPathFollowingStatus::Idle;
}

bool ANawatAIController::HasArrived(const FVector& Destination, float Tolerance) const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return false;
	}

	// Destinations are at floor level and the pawn's location is its capsule centre, so height is compared loosely.
	const FVector MyLocation = MyPawn->GetActorLocation();
	return FVector::DistSquared2D(MyLocation, Destination) <= FMath::Square(Tolerance) && FMath::Abs(MyLocation.Z - Destination.Z) < 250.f;
}

bool ANawatAIController::PickReachablePoint(const FVector& Origin, float Radius, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavSystem)
	{
		return false;
	}

	FNavLocation NavLocation;
	if (!NavSystem->GetRandomReachablePointInRadius(Origin, Radius, NavLocation))
	{
		return false;
	}

	OutLocation = NavLocation.Location;
	return true;
}

void ANawatAIController::LookAround()
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	const FVector Direction = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
	SetFocalPoint(MyPawn->GetActorLocation() + Direction * 600.f);
}

void ANawatAIController::ApplySpeed(bool bCalm)
{
	if (BaseWalkSpeed <= 0.f)
	{
		return;
	}

	ACharacter* MyCharacter = Cast<ACharacter>(GetPawn());
	UCharacterMovementComponent* Movement = MyCharacter ? MyCharacter->GetCharacterMovement() : nullptr;
	if (Movement)
	{
		const bool bSlow = bCalm && Team == ENawatTeam::Enemy && PatrolSpeed > 0.f;
		Movement->MaxWalkSpeed = bSlow ? FMath::Min(PatrolSpeed, BaseWalkSpeed) : BaseWalkSpeed;
	}
}

void ANawatAIController::SetCrouched(bool bCrouched)
{
	ACharacter* MyCharacter = Cast<ACharacter>(GetPawn());
	if (!MyCharacter)
	{
		return;
	}

	if (bCrouched)
	{
		MyCharacter->Crouch();
	}
	else
	{
		MyCharacter->UnCrouch();
	}
}
