#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "NawatCondition.generated.h"

/**
 * A yes/no question about the game, authored inline inside data assets.
 * Subclass in C++ (override Test_Implementation) or in Blueprint (override Test).
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class NAWATCORE_API UNawatCondition : public UObject
{
	GENERATED_BODY()

public:
	/** Context is any object living in the world, typically the actor or director asking. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Condition")
	bool Evaluate(const UObject* Context) const;

	/** True when every non-null condition passes. An empty list passes. */
	static bool AllPass(const TArray<TObjectPtr<UNawatCondition>>& Conditions, const UObject* Context);

	/**
	 * Facts this condition reads. Owners use it to re-evaluate only when a relevant fact changes.
	 * Leave empty for conditions that depend on something other than facts; those are polled.
	 */
	virtual void GatherFacts(FGameplayTagContainer& OutFacts) const {}

	virtual UWorld* GetWorld() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	bool bInvert = false;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Nawat|Condition")
	bool Test(const UObject* Context) const;

private:
	// Conditions are owned by assets, which have no world; the caller's world is borrowed while evaluating
	// so Blueprint subclasses can use world-context nodes.
	mutable TWeakObjectPtr<const UObject> ActiveContext;
};

UENUM(BlueprintType)
enum class ENawatCompare : uint8
{
	Equal,
	NotEqual,
	Greater,
	GreaterOrEqual,
	Less,
	LessOrEqual
};

/** Compares a fact against a constant. With the defaults it reads "the flag is set". */
UCLASS(meta = (DisplayName = "Fact"))
class NAWATCORE_API UNawatCondition_Fact : public UNawatCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FGameplayTag Fact;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	ENawatCompare Comparison = ENawatCompare::GreaterOrEqual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	int32 Value = 1;

	virtual void GatherFacts(FGameplayTagContainer& OutFacts) const override;

protected:
	virtual bool Test_Implementation(const UObject* Context) const override;
};

UENUM(BlueprintType)
enum class ENawatGroupMode : uint8
{
	All,
	Any
};

/** Combines child conditions with AND / OR. */
UCLASS(meta = (DisplayName = "Group"))
class NAWATCORE_API UNawatCondition_Group : public UNawatCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	ENawatGroupMode Mode = ENawatGroupMode::All;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Condition")
	TArray<TObjectPtr<UNawatCondition>> Conditions;

	virtual void GatherFacts(FGameplayTagContainer& OutFacts) const override;

protected:
	virtual bool Test_Implementation(const UObject* Context) const override;
};
