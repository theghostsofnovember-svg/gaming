#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "NawatAITypes.h"
#include "NawatSquadSubsystem.generated.h"

class ACharacter;
class ANawatAIController;
struct FNawatEventPayload;

/**
 * Coordinates agents: shared target knowledge, attack tokens and combat roles per squad, the
 * global alert level, and the orders, dismissal and return of the player's companions.
 *
 * Other modules never call this directly; they broadcast the Request tags of NawatTags.h on
 * UNawatWorldState. The Blueprint functions below exist for level scripting convenience.
 */
UCLASS()
class NAWATAI_API UNawatSquadSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UNawatSquadSubsystem* Get(const UObject* WorldContextObject);

	/** Squad id given to companions created by SpawnCompanions. */
	static const FName PlayerSquadId;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// ---- Membership (called by the controllers) ----

	void RegisterMember(ANawatAIController* Member);
	void UnregisterMember(ANawatAIController* Member);

	// ---- Shared knowledge ----

	void ReportTarget(ANawatAIController* Member, AActor* Target, const FVector& Location);

	/** What Member's squad knows about Target. OutTime is the world time it was last seen. */
	bool GetSharedTarget(const ANawatAIController* Member, const AActor* Target, FVector& OutLocation, float& OutTime) const;

	/** Alerts squadmates within AlertRadius and hands out combat roles. */
	void NotifyCombatStarted(ANawatAIController* Member, AActor* Target, const FVector& Location);

	// ---- Attack tokens ----

	/** True when Member may fire now. A token left unreleased expires after AttackTokenTimeout. */
	bool RequestAttackToken(ANawatAIController* Member);
	void ReleaseAttackToken(ANawatAIController* Member);

	// ---- Alert ----

	/** Recomputes the highest alert among enemies and broadcasts Event_AI_AlertChanged when it changed. */
	void RefreshAlert();

	/** 0 calm, 1 suspicious or searching, 2 combat. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Squad")
	int32 GetAlertLevel() const { return CurrentAlert; }

	/** Sends every enemy toward Location: into combat when it sees a hostile, searching otherwise. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Squad")
	void RaiseAlarm(FVector Location);

	// ---- Companions ----

	/** Gives an order to every companion. Location and Target are used by Hold and Attack. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Squad")
	void IssueOrder(ENawatSquadOrder Order, FVector Location, AActor* Target);

	/** Companions walk to Location and vanish there; a zero Location removes them at once. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Squad")
	void DismissCompanions(FVector Location);

	/** Brings the companions back at Location, or near the player when Location is zero. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Squad")
	void SummonCompanions(FVector Location);

	/** Remembers what the companions are, so they can be respawned after a dismissal. */
	void SetCompanionSpawnParams(TSubclassOf<ACharacter> CharacterClass, TSubclassOf<ANawatAIController> ControllerClass, int32 Count, const FNawatLoadout& Loadout);

	/** Spawns the remembered companions around Location (near the player when zero). Returns how many appeared. */
	int32 SpawnCompanions(const FVector& Location);

	UFUNCTION(BlueprintPure, Category = "Nawat|Squad")
	int32 GetNumCompanions() const;

	/** Where Member should stand while following the player. */
	FVector GetFormationSlot(const ANawatAIController* Member) const;

	// ---- Tunables (set from the level Blueprint when the defaults do not fit) ----

	/** Enemies of one squad allowed to fire at the same time. */
	UPROPERTY(BlueprintReadWrite, Category = "Nawat|Squad")
	int32 MaxSimultaneousAttackers = 2;

	UPROPERTY(BlueprintReadWrite, Category = "Nawat|Squad")
	float AttackTokenTimeout = 4.f;

	/** Squadmates within this distance of a member entering combat are alerted. */
	UPROPERTY(BlueprintReadWrite, Category = "Nawat|Squad")
	float AlertRadius = 3000.f;

	/** Distance between formation slots around the player. */
	UPROPERTY(BlueprintReadWrite, Category = "Nawat|Squad")
	float FormationSpacing = 220.f;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FToken
	{
		TWeakObjectPtr<ANawatAIController> Holder;
		float Time = 0.f;
	};

	struct FSquad
	{
		ENawatTeam Team = ENawatTeam::Neutral;
		FName Id = NAME_None;
		TArray<TWeakObjectPtr<ANawatAIController>> Members;
		TArray<FToken> Tokens;
		TWeakObjectPtr<AActor> TargetActor;
		FVector TargetLocation = FVector::ZeroVector;
		float TargetTime = -1000.f;
	};

	FSquad* FindSquad(const ANawatAIController* Member);
	const FSquad* FindSquad(const ANawatAIController* Member) const;
	void AssignRoles(const ANawatAIController* Member);

	/** Living, possessed members of a team. bIncludeLeaving also returns companions walking away. */
	void GatherTeam(ENawatTeam Team, bool bIncludeLeaving, TArray<ANawatAIController*>& OutMembers) const;

	void BindToBus();
	void HandleBusEvent(FGameplayTag Event, const FNawatEventPayload& Payload);
	void SetSquadPresent(bool bPresent);

	TArray<FSquad> Squads;
	int32 CurrentAlert = 0;
	bool bBoundToBus = false;

	UPROPERTY()
	TSubclassOf<ACharacter> CompanionClass;

	UPROPERTY()
	TSubclassOf<ANawatAIController> CompanionControllerClass;

	UPROPERTY()
	int32 CompanionCount = 0;

	UPROPERTY()
	FNawatLoadout CompanionLoadout;
};
