#include "NawatWorldState.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NawatLog.h"

UNawatWorldState* UNawatWorldState::Get(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UNawatWorldState>() : nullptr;
}

int32 UNawatWorldState::GetFact(FGameplayTag Fact) const
{
	const int32* Value = Facts.Find(Fact);
	return Value ? *Value : 0;
}

bool UNawatWorldState::GetFlag(FGameplayTag Fact) const
{
	return GetFact(Fact) != 0;
}

void UNawatWorldState::SetFact(FGameplayTag Fact, int32 Value)
{
	if (!Fact.IsValid())
	{
		UE_LOG(LogNawat, Warning, TEXT("SetFact called with an invalid tag; ignored."));
		return;
	}

	const int32 OldValue = GetFact(Fact);
	if (OldValue == Value)
	{
		return;
	}

	// Zero and "never set" are the same thing, so zero is not stored.
	if (Value == 0)
	{
		Facts.Remove(Fact);
	}
	else
	{
		Facts.Add(Fact, Value);
	}

	OnFactChangedNative.Broadcast(Fact, OldValue, Value);
	OnFactChanged.Broadcast(Fact, OldValue, Value);
}

void UNawatWorldState::SetFlag(FGameplayTag Fact, bool bValue)
{
	SetFact(Fact, bValue ? 1 : 0);
}

int32 UNawatWorldState::AddToFact(FGameplayTag Fact, int32 Delta)
{
	const int32 NewValue = GetFact(Fact) + Delta;
	SetFact(Fact, NewValue);
	return GetFact(Fact);
}

void UNawatWorldState::RestoreFacts(const TMap<FGameplayTag, int32>& NewFacts)
{
	Facts.Reset();
	for (const TPair<FGameplayTag, int32>& Pair : NewFacts)
	{
		// Tags deleted from the project since the save was written load as invalid.
		if (Pair.Key.IsValid() && Pair.Value != 0)
		{
			Facts.Add(Pair.Key, Pair.Value);
		}
	}

	OnFactsRestoredNative.Broadcast();
	OnFactsRestored.Broadcast();
}

void UNawatWorldState::BroadcastEvent(FGameplayTag Event, const FNawatEventPayload& Payload)
{
	if (!Event.IsValid())
	{
		UE_LOG(LogNawat, Warning, TEXT("BroadcastEvent called with an invalid tag; ignored."));
		return;
	}

	for (FGameplayTag Tag = Event; Tag.IsValid(); Tag = Tag.RequestDirectParent())
	{
		if (const TSharedRef<FNawatEventNative>* Found = NativeListeners.Find(Tag))
		{
			const TSharedRef<FNawatEventNative> Listeners = *Found;
			Listeners->Broadcast(Event, Payload);
		}
	}

	OnEvent.Broadcast(Event, Payload);
}

FNawatEventNative& UNawatWorldState::OnEventNative(FGameplayTag Event)
{
	if (const TSharedRef<FNawatEventNative>* Found = NativeListeners.Find(Event))
	{
		return Found->Get();
	}
	return NativeListeners.Add(Event, MakeShared<FNawatEventNative>()).Get();
}
