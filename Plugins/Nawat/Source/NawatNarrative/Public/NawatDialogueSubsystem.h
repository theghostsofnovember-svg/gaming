#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NawatDialogue.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "NawatDialogueSubsystem.generated.h"

class UAudioComponent;
class UTexture2D;

/** The line currently on screen, ready for UI. */
USTRUCT(BlueprintType)
struct NAWATNARRATIVE_API FNawatDialogueLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	FText SpeakerName;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	FText Text;

	/** Already loaded; null when the node has no portrait. */
	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	TObjectPtr<UTexture2D> Portrait = nullptr;

	/** True while the line waits for Choose instead of advancing on its own. */
	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	bool bHasChoices = false;

	/** Seconds before the line advances on its own. 0 when it waits for a choice. */
	UPROPERTY(BlueprintReadOnly, Category = "Nawat")
	float Duration = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNawatDialogueEvent, UNawatDialogue*, Dialogue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNawatDialogueLineShown, const FNawatDialogueLine&, Line);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNawatDialogueChoicesShown, const TArray<FText>&, Choices);

/**
 * Plays one dialogue at a time. Lines advance on a timer unless they offer choices; time keeps
 * running, and move/look input is blocked only when the caller asks for it.
 *
 * Conditions and actions receive this subsystem as their context.
 */
UCLASS()
class NAWATNARRATIVE_API UNawatDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Resolves the subsystem from any object that lives in a world. Returns null outside of gameplay. */
	static UNawatDialogueSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual UWorld* GetWorld() const override;

	/** Replaces the running dialogue, if any. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Dialogue")
	bool StartDialogue(UNawatDialogue* Dialogue, bool bBlockPlayerInput = true);

	/** Skips to the next line. Ignored while choices are pending. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Dialogue")
	void Advance();

	/** Picks one of GetAvailableChoices by index. Returns false if the index is not on offer. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Dialogue")
	bool Choose(int32 ChoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "Nawat|Dialogue")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "Nawat|Dialogue")
	bool IsActive() const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Dialogue")
	UNawatDialogue* GetCurrentDialogue() const;

	UFUNCTION(BlueprintPure, Category = "Nawat|Dialogue")
	FNawatDialogueLine GetCurrentLine() const;

	/** Choices of the current line whose conditions passed when it appeared. Empty when none are pending. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Dialogue")
	TArray<FText> GetAvailableChoices() const;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Dialogue")
	FNawatDialogueEvent OnDialogueStarted;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Dialogue")
	FNawatDialogueLineShown OnLineShown;

	/** Fires right after OnLineShown for a line that offers choices. */
	UPROPERTY(BlueprintAssignable, Category = "Nawat|Dialogue")
	FNawatDialogueChoicesShown OnChoicesShown;

	UPROPERTY(BlueprintAssignable, Category = "Nawat|Dialogue")
	FNawatDialogueEvent OnDialogueFinished;

private:
	/** Shows the first node at or after NewNodeIndex whose ShowWhen passes, or finishes. */
	void EnterNode(int32 NewNodeIndex);

	/** Index of the node that follows FromIndex; out of range means the dialogue is over. */
	int32 ResolveNext(FName NextNodeId, int32 FromIndex) const;

	void Finish();
	void HandleLineTimer();
	void StopVoice();
	void ClearLineTimer();
	void BroadcastDialogueEvent(FGameplayTag Event, UNawatDialogue* Subject) const;

	UPROPERTY()
	TObjectPtr<UNawatDialogue> Current;

	// Keeps the loaded portrait referenced while the line is on screen.
	UPROPERTY()
	FNawatDialogueLine CurrentLine;

	int32 NodeIndex = INDEX_NONE;

	/** Indices into the current node's Choices that are on offer. */
	TArray<int32> AvailableChoices;

	// Bumped whenever the line or dialogue is replaced, so code that ran actions or broadcasts
	// can tell that the ground moved under it.
	int32 Serial = 0;

	TWeakObjectPtr<UAudioComponent> VoiceComponent;
	FTimerHandle LineTimer;
};
