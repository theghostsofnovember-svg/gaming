#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "NawatDialogueSubsystem.h"
#include "NawatPauseSubsystem.h"
#include "NawatStoryDirector.h"
#include "NawatWorldState.h"
#include "TimerManager.h"
#include "NawatHUDWidget.generated.h"

class UButton;
class UImage;
class UNawatChapter;
class UNawatDialogue;
class UNawatInteractorComponent;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UWidget;

/**
 * The story HUD: objectives, chapter card, subtitles and choices, interaction prompt, alert
 * indicator, squad hint, cinematic letterbox and the pause menu.
 *
 * Used as is, it builds its own layout in C++. A Widget Blueprint subclass may supply its own
 * layout instead: any widget named like one of the properties below is picked up, and any that
 * is missing is simply skipped. The HUD only listens; it never drives game logic except through
 * the three pause menu buttons.
 */
UCLASS()
class NAWATPRESENTATION_API UNawatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The interactor whose focus drives the interaction prompt. Null clears the prompt. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|HUD")
	void SetInteractor(UNawatInteractorComponent* InInteractor);

	/** 0..1 while the skip key is held, 0 when it is not. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|HUD")
	void SetSkipProgress(float Progress);

	/** Re-reads everything from the kernel subsystems. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|HUD")
	void RefreshAll();

	// ---- Localisable strings ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText PauseTitleLabel = NSLOCTEXT("Nawat", "PauseTitle", "Paused");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText ResumeLabel = NSLOCTEXT("Nawat", "Resume", "Resume");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText SaveLabel = NSLOCTEXT("Nawat", "Save", "Save");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText QuitLabel = NSLOCTEXT("Nawat", "Quit", "Quit to desktop");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText SavedLabel = NSLOCTEXT("Nawat", "Saved", "Game saved.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText SaveFailedLabel = NSLOCTEXT("Nawat", "SaveFailed", "Could not save.");

	/** {0} = key name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText SkipHintFormat = NSLOCTEXT("Nawat", "SkipHint", "Hold [{0}] to skip");

	/** {0} = key name, {1} = prompt of the focused object. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText PromptFormat = NSLOCTEXT("Nawat", "Prompt", "[{0}] {1}");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText AlertSuspiciousLabel = NSLOCTEXT("Nawat", "AlertSuspicious", "Suspicious");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText AlertCombatLabel = NSLOCTEXT("Nawat", "AlertCombat", "Combat");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText SquadFollowLabel = NSLOCTEXT("Nawat", "SquadFollow", "Follow");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText SquadHoldLabel = NSLOCTEXT("Nawat", "SquadHold", "Hold");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText SquadAttackLabel = NSLOCTEXT("Nawat", "SquadAttack", "Attack");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText SquadRegroupLabel = NSLOCTEXT("Nawat", "SquadRegroup", "Regroup");

	/** Put before a completed objective. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText ObjectiveDoneMarker = NSLOCTEXT("Nawat", "ObjectiveDone", "[x]");

	/** Put before an objective that is still open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText ObjectivePendingMarker = NSLOCTEXT("Nawat", "ObjectivePending", "[  ]");

	/** Appended to optional objectives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText ObjectiveOptionalSuffix = NSLOCTEXT("Nawat", "ObjectiveOptional", "(optional)");

	// ---- Behaviour ----

	/** Seconds the chapter title card stays on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD", meta = (ClampMin = "0.0"))
	float ChapterCardSeconds = 4.f;

	/** Switches the owning player to Game And UI input while the pause menu is open, and back to Game Only after. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	bool bSwitchInputModeForPauseMenu = true;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---- Hooks for Widget Blueprint subclasses; the C++ HUD does not depend on them ----

	UFUNCTION(BlueprintImplementableEvent, Category = "Nawat|HUD")
	void OnObjectivesChanged(const TArray<FNawatObjectiveState>& Objectives);

	UFUNCTION(BlueprintImplementableEvent, Category = "Nawat|HUD")
	void OnChapterCardShown(UNawatChapter* Chapter);

	/** bVisible is false when the dialogue ended. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Nawat|HUD")
	void OnSubtitleChanged(const FNawatDialogueLine& Line, bool bVisible);

	UFUNCTION(BlueprintImplementableEvent, Category = "Nawat|HUD")
	void OnChoicesChanged(const TArray<FText>& Choices);

	/** 0 calm, 1 suspicious, 2 combat. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Nawat|HUD")
	void OnAlertChanged(int32 AlertLevel);

	UFUNCTION(BlueprintImplementableEvent, Category = "Nawat|HUD")
	void OnCinematicModeChanged(bool bCinematicPlaying);

	UFUNCTION(BlueprintImplementableEvent, Category = "Nawat|HUD")
	void OnPauseMenuChanged(bool bMenuOpen);

	// ---- Named widgets. A Widget Blueprint supplies any of these by using the same name ----

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ObjectivePanel;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StageTitleText;

	/** One text row per objective is added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ObjectiveList;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ChapterCard;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChapterTitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChapterSubtitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SubtitlePanel;

	/** Collapsed together with PortraitImage when the line has no portrait. */
	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PortraitPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeakerText;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LineText;

	/** One numbered text row per choice is added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ChoiceList;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AlertText;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SquadHintText;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LetterboxTop;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LetterboxBottom;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SkipPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkipText;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> SkipProgressBar;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PauseMenuPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PauseTitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResumeButtonText;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SaveButtonText;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuitButtonText;

	/** Result of the last save. */
	UPROPERTY(BlueprintReadOnly, Category = "Nawat|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PauseStatusText;

private:
	// ---- Default layout ----

	void BuildDefaultTree();
	UTextBlock* MakeText(FName Name, int32 FontSize, const FLinearColor& Color, bool bShadow = true);
	UButton* MakeButton(UPanelWidget* Parent, FName ButtonName, FName TextName, TObjectPtr<UTextBlock>& OutText);

	// ---- Story ----

	UFUNCTION()
	void HandleChapterStarted(UNawatChapter* Chapter);

	UFUNCTION()
	void HandleChapterCompleted(UNawatChapter* Chapter);

	UFUNCTION()
	void HandleStageChanged(UNawatChapter* Chapter, int32 StageIndex);

	UFUNCTION()
	void HandleObjectiveCompleted(UNawatChapter* Chapter, int32 StageIndex, FName ObjectiveId);

	void RefreshObjectives();
	void HideChapterCard();

	// ---- Dialogue ----

	UFUNCTION()
	void HandleDialogueStarted(UNawatDialogue* Dialogue);

	UFUNCTION()
	void HandleDialogueFinished(UNawatDialogue* Dialogue);

	UFUNCTION()
	void HandleLineShown(const FNawatDialogueLine& Line);

	UFUNCTION()
	void HandleChoicesShown(const TArray<FText>& Choices);

	void ShowLine(const FNawatDialogueLine& Line);
	void ShowChoices(const TArray<FText>& Choices);

	// ---- Interaction ----

	UFUNCTION()
	void HandleFocusChanged(AActor* NewFocus, FText Prompt);

	void BindInteractor();
	void UnbindInteractor();
	void ShowPrompt(const FText& Prompt);

	// ---- Bus and facts ----

	void HandleAlertEvent(FGameplayTag Event, const FNawatEventPayload& Payload);
	void HandleCinematicStartedEvent(FGameplayTag Event, const FNawatEventPayload& Payload);
	void HandleCinematicFinishedEvent(FGameplayTag Event, const FNawatEventPayload& Payload);
	void HandleFactChanged(FGameplayTag Fact, int32 OldValue, int32 NewValue);
	void HandleFactsRestored();

	void SetAlertLevel(int32 NewLevel);
	void SetCinematicMode(bool bNewCinematic);
	void RefreshSquadPresence();

	// ---- Pause ----

	UFUNCTION()
	void HandlePauseChanged(bool bAnyReasonActive, const FNawatPausePolicy& EffectivePolicy);

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleQuitClicked();

	void ApplyStaticTexts();

	/** Single place that decides what is on screen from the flags below. */
	void ApplyVisibility();
	void ApplyPauseMenuInputMode(bool bMenuOpen);

	TWeakObjectPtr<UNawatInteractorComponent> Interactor;
	bool bInteractorBound = false;
	bool bConstructed = false;

	bool bHasObjectives = false;
	bool bChapterCardVisible = false;
	bool bSubtitleVisible = false;
	bool bPromptVisible = false;
	bool bSquadPresent = false;
	bool bCinematic = false;
	bool bCinematicSkippable = false;
	bool bHideGameplayPanels = false;
	bool bPauseMenuOpen = false;
	bool bInputModeApplied = false;
	int32 AlertLevel = 0;

	FTimerHandle ChapterCardTimer;
};
