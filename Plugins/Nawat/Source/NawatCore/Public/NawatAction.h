#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "NawatAction.generated.h"

/**
 * Something the story does, authored inline inside data assets.
 * Subclass in C++ (override Run_Implementation) or in Blueprint (override Run).
 * Actions are instantaneous; long-running work is started here and reports back through an event.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class NAWATCORE_API UNawatAction : public UObject
{
	GENERATED_BODY()

public:
	/** Context is any object living in the world, typically the actor or director that triggered it. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Action")
	void Execute(UObject* Context);

	/** Runs every non-null action in order. */
	static void ExecuteAll(const TArray<TObjectPtr<UNawatAction>>& Actions, UObject* Context);

	virtual UWorld* GetWorld() const override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Nawat|Action")
	void Run(UObject* Context);

private:
	// Same reason as UNawatCondition: asset-owned objects borrow the caller's world while running.
	TWeakObjectPtr<UObject> ActiveContext;
};

UENUM(BlueprintType)
enum class ENawatFactOperation : uint8
{
	Set,
	Add
};

/** Sets or increments a fact. With the defaults it raises a flag. */
UCLASS(meta = (DisplayName = "Set Fact"))
class NAWATCORE_API UNawatAction_SetFact : public UNawatAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FGameplayTag Fact;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	ENawatFactOperation Operation = ENawatFactOperation::Set;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	int32 Value = 1;

protected:
	virtual void Run_Implementation(UObject* Context) override;
};

/** Broadcasts an event on the bus, with Context as the instigator. */
UCLASS(meta = (DisplayName = "Broadcast Event"))
class NAWATCORE_API UNawatAction_BroadcastEvent : public UNawatAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FGameplayTag Event;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	float Value = 0.f;

protected:
	virtual void Run_Implementation(UObject* Context) override;
};
