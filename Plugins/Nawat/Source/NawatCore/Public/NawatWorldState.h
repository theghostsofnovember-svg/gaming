#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NawatWorldState.generated.h"

/** Optional data carried by an event. Every field may be left empty. */
USTRUCT(BlueprintType)
struct NAWATCORE_API FNawatEventPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	TObjectPtr<UObject> Instigator = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	TObjectPtr<UObject> Subject = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	float Value = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nawat")
	FVector Location = FVector::ZeroVector;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNawatFactChanged, FGameplayTag, Fact, int32, OldValue, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNawatFactsRestored);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNawatEvent, FGameplayTag, Event, const FNawatEventPayload&, Payload);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FNawatFactChangedNative, FGameplayTag /*Fact*/, int32 /*OldValue*/, int32 /*NewValue*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FNawatEventNative, FGameplayTag /*Event*/, const FNawatEventPayload& /*Payload*/);

/**
 * Shared story state and the event bus. This is the only thing kernel systems know about each other.
 *
 * Facts are integers keyed by gameplay tag; a fact that was never set reads as 0, and a flag is
 * simply a fact that is non-zero. Lives on the game instance so it survives level travel.
 */
UCLASS()
class NAWATCORE_API UNawatWorldState : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Resolves the subsystem from any object that lives in a world. Returns null outside of gameplay. */
	static UNawatWorldState* Get(const UObject* WorldContextObject);

	// ---- Facts ----

	UFUNCTION(BlueprintPure, Category = "Nawat|Facts")
	int32 GetFact(FGameplayTag Fact) const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Facts")
	bool GetFlag(FGameplayTag Fact) const;

	UFUNCTION(BlueprintCallable, Category = "Nawat|Facts")
	void SetFact(FGameplayTag Fact, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Nawat|Facts")
	void SetFlag(FGameplayTag Fact, bool bValue);

	/** Adds Delta to the fact and returns the new value. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Facts")
	int32 AddToFact(FGameplayTag Fact, int32 Delta);

	/** Replaces every fact at once (loading a save, starting a new game). Fires OnFactsRestored only. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Facts")
	void RestoreFacts(const TMap<FGameplayTag, int32>& NewFacts);

	const TMap<FGameplayTag, int32>& GetAllFacts() const { return Facts; }

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Facts")
	FNawatFactChanged OnFactChanged;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Facts")
	FNawatFactsRestored OnFactsRestored;

	FNawatFactChangedNative OnFactChangedNative;
	FSimpleMulticastDelegate OnFactsRestoredNative;

	// ---- Events ----

	/** Broadcasts to listeners of Event and of each of its parent tags. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Events", meta = (AutoCreateRefTerm = "Payload"))
	void BroadcastEvent(FGameplayTag Event, const FNawatEventPayload& Payload);

	/** Receives every event; filter with MatchesTag. */
	UPROPERTY(BlueprintAssignable, Category = "Nawat|Events")
	FNawatEvent OnEvent;

	/**
	 * Native listeners for one tag. Listening on a parent tag also receives its children,
	 * e.g. OnEventNative(Story).AddUObject(...) receives Story.StageChanged.
	 */
	FNawatEventNative& OnEventNative(FGameplayTag Event);

private:
	UPROPERTY()
	TMap<FGameplayTag, int32> Facts;

	// Shared refs keep a listener list alive while it broadcasts, even if a listener registers a new tag.
	TMap<FGameplayTag, TSharedRef<FNawatEventNative>> NativeListeners;
};
