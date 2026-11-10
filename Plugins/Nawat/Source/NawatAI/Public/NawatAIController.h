#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "NawatAITypes.h"
#include "NawatCoverSubsystem.h"
#include "NawatAIController.generated.h"

class ANawatAIController;
class ANawatPatrolRoute;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UNawatAIWeaponComponent;
class UNawatAgentComponent;
class UNawatHealthComponent;
class UNawatSquadSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNawatAIStateChanged, ANawatAIController*, Controller, ENawatAIState, NewState, ENawatAIState, OldState);

/**
 * The brain of a Nawat agent: a state machine that drives any ACharacter with no Behavior Tree,
 * Blackboard or EQS asset. Spawners create it; it can also be set as the AI Controller Class of a
 * character placed in the level, in which case the missing Nawat components are added on possess.
 *
 * Enemies patrol or wander, grow suspicious gradually, fight from cover and search when they lose
 * the target. Companions (team Player) follow the player's orders instead of patrolling.
 */
UCLASS()
class NAWATAI_API ANawatAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANawatAIController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	/** Called by the spawner before Possess. A None squad id keeps the one on the pawn's agent component. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|AI")
	void InitAgent(FName InSquadId, ANawatPatrolRoute* InPatrolRoute);

	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	ENawatAIState GetState() const { return State; }

	/** 0 = unaware, 1 = fully alerted. Crossing SuspiciousThreshold makes the agent investigate. */
	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	float GetAwareness() const { return Awareness; }

	/** 0 calm, 1 suspicious or searching, 2 combat. */
	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	int32 GetAlertLevel() const;

	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	ENawatTeam GetAgentTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	FName GetSquadId() const { return SquadId; }

	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	ENawatSquadOrder GetOrder() const { return CurrentOrder; }

	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	ENawatSquadRole GetSquadRole() const { return SquadRole; }

	UFUNCTION(BlueprintPure, Category = "Nawat|AI")
	AActor* GetTargetActor() const { return TargetActor.Get(); }

	/** Companions only. Location and Target are used by Hold and Attack. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|AI")
	void SetOrder(ENawatSquadOrder Order, FVector Location, AActor* Target);

	// ---- Called by the squad subsystem ----

	void SetSquadRole(ENawatSquadRole InRole, float InFlankSign);

	/** A squadmate entered combat. */
	void ReceiveSquadAlert(AActor* Target, const FVector& Location);

	/** A general alarm was raised. */
	void ReceiveAlarm(const FVector& Location);

	/** Dismissed: walk to Location and disappear there, or disappear now when Location is zero. */
	void BeginLeaving(const FVector& Location);
	bool IsLeaving() const { return bLeaving; }

	/** Destroys the pawn and this controller. */
	void DestroyAgent();

	UPROPERTY(BlueprintAssignable, Category = "Nawat|AI")
	FNawatAIStateChanged OnStateChanged;

	// ---- Perception ----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Perception", meta = (ClampMin = "100.0"))
	float SightRadius = 2500.f;

	/** Once seen, a target is lost only beyond this distance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Perception", meta = (ClampMin = "100.0"))
	float LoseSightRadius = 3000.f;

	/** Half-angle of the vision cone, in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Perception", meta = (ClampMin = "10.0", ClampMax = "180.0"))
	float PeripheralVisionAngle = 70.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Perception", meta = (ClampMin = "0.0"))
	float HearingRange = 3000.f;

	/** Seconds between decisions. Firing is timed per frame and does not depend on this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Perception", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.2f;

	// ---- Awareness ----

	/** Awareness gained per second while a hostile is seen at point-blank range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Awareness", meta = (ClampMin = "0.01"))
	float AwarenessGainNear = 3.f;

	/** Awareness gained per second while a hostile is seen at the edge of the sight radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Awareness", meta = (ClampMin = "0.01"))
	float AwarenessGainFar = 0.4f;

	/** Multiplies the gain while the hostile is in the outer half of the vision cone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Awareness", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PeripheralGainScale = 0.5f;

	/** Awareness lost per second while no hostile is seen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Awareness", meta = (ClampMin = "0.0"))
	float AwarenessDecay = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Awareness", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float SuspiciousThreshold = 0.3f;

	// ---- Suspicion and search ----

	/** Seconds the agent stares at a stimulus before walking to it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Search", meta = (ClampMin = "0.0"))
	float SuspiciousReactionDelay = 0.8f;

	/** Seconds spent looking around at the stimulus location before giving up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Search", meta = (ClampMin = "0.0"))
	float SuspiciousLookTime = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Search", meta = (ClampMin = "1.0"))
	float SearchDuration = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Search", meta = (ClampMin = "100.0"))
	float SearchRadius = 600.f;

	// ---- Combat ----

	/** Seconds without anyone in the squad seeing the target before the agent starts searching. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Combat", meta = (ClampMin = "1.0"))
	float LoseTargetTime = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Combat", meta = (ClampMin = "200.0"))
	float CoverSearchRadius = 1500.f;

	/** Flanking and advancing agents move to new cover after this many bursts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Combat", meta = (ClampMin = "1"))
	int32 BurstsBeforeRelocate = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Combat", meta = (ClampMin = "0.0"))
	float HideTimeMin = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Combat", meta = (ClampMin = "0.0"))
	float HideTimeMax = 2.f;

	/** Without cover the agent closes in beyond this distance and backs off when very close. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Combat", meta = (ClampMin = "200.0"))
	float PreferredRange = 1500.f;

	// ---- Patrol ----

	/** Without a patrol route, wander around the spawn location instead of standing still. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Patrol")
	bool bWanderWithoutRoute = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Patrol", meta = (ClampMin = "100.0"))
	float WanderRadius = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Patrol", meta = (ClampMin = "0.0"))
	float WanderWaitMin = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Patrol", meta = (ClampMin = "0.0"))
	float WanderWaitMax = 5.f;

	/** Walk speed of enemies while calm or suspicious. Zero keeps the character's own speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Patrol", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 200.f;

	// ---- Companions ----

	/** A following companion starts moving when farther than this from the player. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Companion", meta = (ClampMin = "100.0"))
	float FollowDistance = 450.f;

	/** A companion closer than this to the player steps aside. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Companion", meta = (ClampMin = "0.0"))
	float PersonalSpace = 140.f;

	/** A following companion breaks off a fight when farther than this from the player. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Companion", meta = (ClampMin = "500.0"))
	float LeashDistance = 2500.f;

	/**
	 * Lets the brain own the pawn's rotation settings so it faces its focus while moving.
	 * Turn off when the character's own rotation setup must be kept.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nawat|Movement")
	bool bManageRotation = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	enum class ECombatPhase : uint8
	{
		SeekCover,
		MoveToCover,
		/** Behind cover, crouched or tucked in. */
		Hidden,
		/** Standing up or stepped out to fire. */
		Peek,
		/** No cover available: fight in the open. */
		NoCover
	};

	UFUNCTION()
	void HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandlePawnDamaged(UNawatHealthComponent* Component, float Damage, AActor* InstigatorActor);

	UFUNCTION()
	void HandlePawnDeath(UNawatHealthComponent* Component, AActor* Killer);

	// ---- State machine ----

	void Think(float DeltaSeconds);
	void SetAIState(ENawatAIState NewState);
	void EnterState(ENawatAIState NewState);
	void ExitState(ENawatAIState OldState);
	ENawatAIState GetHomeState() const;

	void UpdateSenses(float DeltaSeconds);
	void UpdateAlertTransitions();

	void TickIdle(float DeltaSeconds);
	void TickPatrol(float DeltaSeconds);
	void TickSuspicious(float DeltaSeconds);
	void TickSearch(float DeltaSeconds);
	void TickCombat(float DeltaSeconds);
	void TickCompanion(float DeltaSeconds);
	void TickLeaving(float DeltaSeconds);

	// ---- Combat helpers ----

	void LoseTarget();
	bool TryTakeCover();
	void BeginHide();
	void BeginPeek(const AActor* Target);
	void Relocate();
	void EnterNoCover();
	void Reposition(const AActor* Target, bool bHasLineOfSight);
	bool ComputePeekLocation(const AActor* Target, FVector& OutLocation) const;
	FVector GetCoverSearchOrigin(float& OutRadius) const;
	void ReleaseCover();

	bool TryStartBurst(AActor* Target);
	void StopBurst();
	void UpdateFiring();

	// ---- Shared helpers ----

	bool IsValidTarget(const AActor* Actor) const;
	bool IsVisibleHostile(const AActor* Actor) const;
	void HearHostile(AActor* Actor, const FVector& Location);
	void RestartInvestigation(const FVector& Location);

	bool GoTo(const FVector& Destination, float AcceptanceRadius);
	bool IsPathMoving() const;
	bool HasArrived(const FVector& Destination, float Tolerance) const;
	bool PickReachablePoint(const FVector& Origin, float Radius, FVector& OutLocation) const;
	void LookAround();

	void ApplyPerceptionConfig();
	void ApplySpeed(bool bCalm);
	void SetCrouched(bool bCrouched);
	void ReleaseAgent();
	UNawatSquadSubsystem* GetSquad() const;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(Transient)
	TObjectPtr<UNawatAgentComponent> Agent;

	UPROPERTY(Transient)
	TObjectPtr<UNawatHealthComponent> Health;

	UPROPERTY(Transient)
	TObjectPtr<UNawatAIWeaponComponent> Weapon;

	UPROPERTY(Transient)
	TObjectPtr<ANawatPatrolRoute> PatrolRoute;

	ENawatAIState State = ENawatAIState::Idle;
	ENawatTeam Team = ENawatTeam::Enemy;
	FName SquadId = NAME_None;
	ENawatSquadRole SquadRole = ENawatSquadRole::Suppress;
	float FlankSign = 1.f;

	bool bHadPawn = false;
	bool bRegistered = false;
	float ThinkAccumulator = 0.f;
	float StateTime = 0.f;
	float BaseWalkSpeed = 0.f;
	FVector HomeLocation = FVector::ZeroVector;

	// Senses
	TArray<TWeakObjectPtr<AActor>> VisibleHostiles;
	TWeakObjectPtr<AActor> BestVisibleHostile;
	float Awareness = 0.f;

	// Target
	TWeakObjectPtr<AActor> TargetActor;
	FVector LastKnownLocation = FVector::ZeroVector;
	float TimeTargetLastSeen = -1000.f;

	// Patrol
	int32 PatrolIndex = 0;
	int32 PatrolDirection = 1;
	float PatrolWait = 0.f;
	bool bPatrolEnRoute = false;

	// Suspicion and search
	FVector InvestigateLocation = FVector::ZeroVector;
	bool bInvestigateMoveStarted = false;
	bool bInvestigateArrived = false;
	float LookTimer = 0.f;
	float LookAroundTimer = 0.f;
	float SearchPause = 0.f;

	// Combat
	ECombatPhase CombatPhase = ECombatPhase::SeekCover;
	FNawatCoverResult Cover;
	bool bHasCover = false;
	float PhaseTimer = 0.f;
	float CoverRetryTimer = 0.f;
	float CombatLockout = 0.f;
	int32 BurstsSinceRelocate = 0;
	int32 FailedPeeks = 0;
	bool bFiredThisPeek = false;
	bool bBursting = false;
	int32 ShotsLeft = 0;
	float BurstCooldown = 0.f;
	bool bLastMoveFailed = false;

	// Companion orders
	ENawatSquadOrder CurrentOrder = ENawatSquadOrder::None;
	TWeakObjectPtr<AActor> OrderTarget;
	FVector OrderLocation = FVector::ZeroVector;
	FVector HoldLocation = FVector::ZeroVector;
	FVector LastFollowDestination = FVector::ZeroVector;
	bool bOrderMoveStarted = false;
	float OrderMoveCooldown = 0.f;

	// Dismissal
	bool bLeaving = false;
	FVector LeaveLocation = FVector::ZeroVector;
	float LeaveTimer = 0.f;
};
