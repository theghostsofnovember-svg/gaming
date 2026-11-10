#include "NawatAction.h"

#include "NawatLog.h"
#include "NawatWorldState.h"

void UNawatAction::Execute(UObject* Context)
{
	const TWeakObjectPtr<UObject> Previous = ActiveContext;
	ActiveContext = Context;
	Run(Context);
	ActiveContext = Previous;
}

void UNawatAction::ExecuteAll(const TArray<TObjectPtr<UNawatAction>>& Actions, UObject* Context)
{
	for (UNawatAction* Action : Actions)
	{
		if (Action)
		{
			Action->Execute(Context);
		}
	}
}

UWorld* UNawatAction::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	const UObject* Context = ActiveContext.Get();
	return Context ? Context->GetWorld() : nullptr;
}

void UNawatAction::Run_Implementation(UObject* Context)
{
}

void UNawatAction_SetFact::Run_Implementation(UObject* Context)
{
	UNawatWorldState* WorldState = UNawatWorldState::Get(Context);
	if (!WorldState)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a world context; skipped."), *GetName());
		return;
	}

	if (Operation == ENawatFactOperation::Add)
	{
		WorldState->AddToFact(Fact, Value);
	}
	else
	{
		WorldState->SetFact(Fact, Value);
	}
}

void UNawatAction_BroadcastEvent::Run_Implementation(UObject* Context)
{
	UNawatWorldState* WorldState = UNawatWorldState::Get(Context);
	if (!WorldState)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a world context; skipped."), *GetName());
		return;
	}

	FNawatEventPayload Payload;
	Payload.Instigator = Context;
	Payload.Value = Value;
	WorldState->BroadcastEvent(Event, Payload);
}
