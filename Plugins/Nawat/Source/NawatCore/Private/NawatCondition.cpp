#include "NawatCondition.h"

#include "NawatLog.h"
#include "NawatWorldState.h"

bool UNawatCondition::Evaluate(const UObject* Context) const
{
	const TWeakObjectPtr<const UObject> Previous = ActiveContext;
	ActiveContext = Context;
	const bool bResult = Test(Context);
	ActiveContext = Previous;

	return bResult != bInvert;
}

bool UNawatCondition::AllPass(const TArray<TObjectPtr<UNawatCondition>>& Conditions, const UObject* Context)
{
	for (const UNawatCondition* Condition : Conditions)
	{
		if (Condition && !Condition->Evaluate(Context))
		{
			return false;
		}
	}
	return true;
}

UWorld* UNawatCondition::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	const UObject* Context = ActiveContext.Get();
	return Context ? Context->GetWorld() : nullptr;
}

bool UNawatCondition::Test_Implementation(const UObject* Context) const
{
	return true;
}

void UNawatCondition_Fact::GatherFacts(FGameplayTagContainer& OutFacts) const
{
	if (Fact.IsValid())
	{
		OutFacts.AddTag(Fact);
	}
}

bool UNawatCondition_Fact::Test_Implementation(const UObject* Context) const
{
	const UNawatWorldState* WorldState = UNawatWorldState::Get(Context);
	if (!WorldState)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s evaluated without a world context; failing."), *GetName());
		return false;
	}

	const int32 Current = WorldState->GetFact(Fact);
	switch (Comparison)
	{
	case ENawatCompare::Equal:          return Current == Value;
	case ENawatCompare::NotEqual:       return Current != Value;
	case ENawatCompare::Greater:        return Current > Value;
	case ENawatCompare::GreaterOrEqual: return Current >= Value;
	case ENawatCompare::Less:           return Current < Value;
	case ENawatCompare::LessOrEqual:    return Current <= Value;
	default:                            return false;
	}
}

void UNawatCondition_Group::GatherFacts(FGameplayTagContainer& OutFacts) const
{
	for (const UNawatCondition* Condition : Conditions)
	{
		if (Condition)
		{
			Condition->GatherFacts(OutFacts);
		}
	}
}

bool UNawatCondition_Group::Test_Implementation(const UObject* Context) const
{
	if (Mode == ENawatGroupMode::All)
	{
		return AllPass(Conditions, Context);
	}

	for (const UNawatCondition* Condition : Conditions)
	{
		if (Condition && Condition->Evaluate(Context))
		{
			return true;
		}
	}
	return false;
}
