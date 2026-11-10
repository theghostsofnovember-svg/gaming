#include "NawatHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Layout/Margin.h"
#include "NawatChapter.h"
#include "NawatCinematicSubsystem.h"
#include "NawatDialogue.h"
#include "NawatInteractorComponent.h"
#include "NawatSaveSubsystem.h"
#include "NawatSettings.h"
#include "NawatTags.h"
#include "Styling/SlateColor.h"
#include "Widgets/Layout/Anchors.h"

namespace
{
	const float HudLetterboxHeight = 90.f;
	const FLinearColor HudPanelColor(0.f, 0.f, 0.f, 0.5f);
	const FLinearColor HudAccentColor(1.f, 0.82f, 0.4f, 1.f);
	const FLinearColor HudDoneColor(0.55f, 0.8f, 0.55f, 1.f);
	const FLinearColor HudSuspiciousColor(1.f, 0.85f, 0.2f, 1.f);
	const FLinearColor HudCombatColor(1.f, 0.25f, 0.2f, 1.f);

	template <typename SubsystemType>
	SubsystemType* FindHudSubsystem(const UObject* Context)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<SubsystemType>() : nullptr;
	}

	void SetHudShown(UWidget* Widget, bool bShown, ESlateVisibility ShownVisibility = ESlateVisibility::HitTestInvisible)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShown ? ShownVisibility : ESlateVisibility::Collapsed);
		}
	}

	void SetHudFontSize(UTextBlock* Text, int32 Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}

	void PlaceOnCanvas(UCanvasPanel* Canvas, UWidget* Child, const FAnchors& Anchors, const FVector2D& Alignment, const FMargin& Offsets, bool bAutoSize, int32 ZOrder)
	{
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child))
		{
			CanvasSlot->SetAnchors(Anchors);
			CanvasSlot->SetAlignment(Alignment);
			CanvasSlot->SetOffsets(Offsets);
			CanvasSlot->SetAutoSize(bAutoSize);
			CanvasSlot->SetZOrder(ZOrder);
		}
	}
}

void UNawatHUDWidget::SetInteractor(UNawatInteractorComponent* InInteractor)
{
	if (Interactor.Get() == InInteractor)
	{
		return;
	}

	UnbindInteractor();
	Interactor = InInteractor;

	if (bConstructed)
	{
		BindInteractor();
	}
}

void UNawatHUDWidget::SetSkipProgress(float Progress)
{
	if (SkipProgressBar)
	{
		SkipProgressBar->SetPercent(FMath::Clamp(Progress, 0.f, 1.f));
	}
}

void UNawatHUDWidget::RefreshAll()
{
	RefreshObjectives();
	RefreshSquadPresence();

	if (const UNawatDialogueSubsystem* Dialogue = UNawatDialogueSubsystem::Get(this))
	{
		if (Dialogue->IsActive())
		{
			ShowLine(Dialogue->GetCurrentLine());
			ShowChoices(Dialogue->GetAvailableChoices());
		}
		else
		{
			bSubtitleVisible = false;
		}
	}

	if (const UNawatCinematicSubsystem* Cinematics = UNawatCinematicSubsystem::Get(this))
	{
		bCinematic = Cinematics->IsPlaying();
		bCinematicSkippable = Cinematics->CanSkip();
	}

	if (const UNawatPauseSubsystem* Pause = FindHudSubsystem<UNawatPauseSubsystem>(this))
	{
		bHideGameplayPanels = Pause->GetEffectivePolicy().bHideHUD;
		bPauseMenuOpen = Pause->HasReason(NawatTags::Pause_Menu);
		ApplyPauseMenuInputMode(bPauseMenuOpen);
	}

	ShowPrompt(Interactor.IsValid() ? Interactor->GetFocusedPrompt() : FText::GetEmpty());
	ApplyVisibility();
}

TSharedRef<SWidget> UNawatHUDWidget::RebuildWidget()
{
	// A Widget Blueprint subclass with its own layout already has a root; only the bare class builds one.
	if (!IsDesignTime())
	{
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		if (!WidgetTree->RootWidget)
		{
			BuildDefaultTree();
		}
	}

	return Super::RebuildWidget();
}

void UNawatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bConstructed = true;
	ApplyStaticTexts();
	SetSkipProgress(0.f);

	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddUniqueDynamic(this, &UNawatHUDWidget::HandleResumeClicked);
	}
	if (SaveButton)
	{
		SaveButton->OnClicked.AddUniqueDynamic(this, &UNawatHUDWidget::HandleSaveClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &UNawatHUDWidget::HandleQuitClicked);
	}

	if (UNawatStoryDirector* Director = UNawatStoryDirector::Get(this))
	{
		Director->OnChapterStarted.AddUniqueDynamic(this, &UNawatHUDWidget::HandleChapterStarted);
		Director->OnChapterCompleted.AddUniqueDynamic(this, &UNawatHUDWidget::HandleChapterCompleted);
		Director->OnStageChanged.AddUniqueDynamic(this, &UNawatHUDWidget::HandleStageChanged);
		Director->OnObjectiveCompleted.AddUniqueDynamic(this, &UNawatHUDWidget::HandleObjectiveCompleted);
	}

	if (UNawatDialogueSubsystem* Dialogue = UNawatDialogueSubsystem::Get(this))
	{
		Dialogue->OnDialogueStarted.AddUniqueDynamic(this, &UNawatHUDWidget::HandleDialogueStarted);
		Dialogue->OnDialogueFinished.AddUniqueDynamic(this, &UNawatHUDWidget::HandleDialogueFinished);
		Dialogue->OnLineShown.AddUniqueDynamic(this, &UNawatHUDWidget::HandleLineShown);
		Dialogue->OnChoicesShown.AddUniqueDynamic(this, &UNawatHUDWidget::HandleChoicesShown);
	}

	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		WorldState->OnEventNative(NawatTags::Event_AI_AlertChanged).AddUObject(this, &UNawatHUDWidget::HandleAlertEvent);
		WorldState->OnEventNative(NawatTags::Event_Cinematic_Started).AddUObject(this, &UNawatHUDWidget::HandleCinematicStartedEvent);
		WorldState->OnEventNative(NawatTags::Event_Cinematic_Finished).AddUObject(this, &UNawatHUDWidget::HandleCinematicFinishedEvent);
		WorldState->OnFactChangedNative.AddUObject(this, &UNawatHUDWidget::HandleFactChanged);
		WorldState->OnFactsRestoredNative.AddUObject(this, &UNawatHUDWidget::HandleFactsRestored);
	}

	if (UNawatPauseSubsystem* Pause = FindHudSubsystem<UNawatPauseSubsystem>(this))
	{
		Pause->OnPauseChanged.AddUniqueDynamic(this, &UNawatHUDWidget::HandlePauseChanged);
	}

	BindInteractor();
	RefreshAll();
}

void UNawatHUDWidget::NativeDestruct()
{
	bConstructed = false;

	if (ResumeButton)
	{
		ResumeButton->OnClicked.RemoveDynamic(this, &UNawatHUDWidget::HandleResumeClicked);
	}
	if (SaveButton)
	{
		SaveButton->OnClicked.RemoveDynamic(this, &UNawatHUDWidget::HandleSaveClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.RemoveDynamic(this, &UNawatHUDWidget::HandleQuitClicked);
	}

	if (UNawatStoryDirector* Director = UNawatStoryDirector::Get(this))
	{
		Director->OnChapterStarted.RemoveDynamic(this, &UNawatHUDWidget::HandleChapterStarted);
		Director->OnChapterCompleted.RemoveDynamic(this, &UNawatHUDWidget::HandleChapterCompleted);
		Director->OnStageChanged.RemoveDynamic(this, &UNawatHUDWidget::HandleStageChanged);
		Director->OnObjectiveCompleted.RemoveDynamic(this, &UNawatHUDWidget::HandleObjectiveCompleted);
	}

	if (UNawatDialogueSubsystem* Dialogue = UNawatDialogueSubsystem::Get(this))
	{
		Dialogue->OnDialogueStarted.RemoveDynamic(this, &UNawatHUDWidget::HandleDialogueStarted);
		Dialogue->OnDialogueFinished.RemoveDynamic(this, &UNawatHUDWidget::HandleDialogueFinished);
		Dialogue->OnLineShown.RemoveDynamic(this, &UNawatHUDWidget::HandleLineShown);
		Dialogue->OnChoicesShown.RemoveDynamic(this, &UNawatHUDWidget::HandleChoicesShown);
	}

	if (UNawatWorldState* WorldState = UNawatWorldState::Get(this))
	{
		WorldState->OnEventNative(NawatTags::Event_AI_AlertChanged).RemoveAll(this);
		WorldState->OnEventNative(NawatTags::Event_Cinematic_Started).RemoveAll(this);
		WorldState->OnEventNative(NawatTags::Event_Cinematic_Finished).RemoveAll(this);
		WorldState->OnFactChangedNative.RemoveAll(this);
		WorldState->OnFactsRestoredNative.RemoveAll(this);
	}

	if (UNawatPauseSubsystem* Pause = FindHudSubsystem<UNawatPauseSubsystem>(this))
	{
		Pause->OnPauseChanged.RemoveDynamic(this, &UNawatHUDWidget::HandlePauseChanged);
	}

	UnbindInteractor();

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChapterCardTimer);
	}

	ApplyPauseMenuInputMode(false);

	Super::NativeDestruct();
}

// ---- Default layout ----

void UNawatHUDWidget::BuildDefaultTree()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	// Letterbox bars.
	{
		UBorder* TopBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LetterboxTop"));
		TopBar->SetBrushColor(FLinearColor::Black);
		PlaceOnCanvas(Canvas, TopBar, FAnchors(0.f, 0.f, 1.f, 0.f), FVector2D(0.f, 0.f), FMargin(0.f, 0.f, 0.f, HudLetterboxHeight), false, 5);
		LetterboxTop = TopBar;

		UBorder* BottomBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LetterboxBottom"));
		BottomBar->SetBrushColor(FLinearColor::Black);
		PlaceOnCanvas(Canvas, BottomBar, FAnchors(0.f, 1.f, 1.f, 1.f), FVector2D(0.f, 1.f), FMargin(0.f, 0.f, 0.f, HudLetterboxHeight), false, 5);
		LetterboxBottom = BottomBar;
	}

	// Objectives, top-left.
	{
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ObjectivePanel"));
		Panel->SetBrushColor(HudPanelColor);
		Panel->SetPadding(FMargin(14.f, 10.f));

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ObjectiveBox"));
		Panel->SetContent(Column);

		StageTitleText = MakeText(TEXT("StageTitleText"), 20, HudAccentColor);
		Column->AddChildToVerticalBox(StageTitleText);

		UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ObjectiveList"));
		if (UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(List))
		{
			ListSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
		ObjectiveList = List;

		PlaceOnCanvas(Canvas, Panel, FAnchors(0.f, 0.f, 0.f, 0.f), FVector2D(0.f, 0.f), FMargin(40.f, 40.f, 0.f, 0.f), true, 1);
		ObjectivePanel = Panel;
	}

	// Alert indicator, top-centre.
	{
		AlertText = MakeText(TEXT("AlertText"), 20, HudSuspiciousColor);
		PlaceOnCanvas(Canvas, AlertText, FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(0.5f, 0.f), FMargin(0.f, 36.f, 0.f, 0.f), true, 1);
	}

	// Chapter title card, centre.
	{
		UVerticalBox* Card = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChapterCard"));

		ChapterTitleText = MakeText(TEXT("ChapterTitleText"), 48, FLinearColor::White);
		ChapterTitleText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* TitleSlot = Card->AddChildToVerticalBox(ChapterTitleText))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
		}

		ChapterSubtitleText = MakeText(TEXT("ChapterSubtitleText"), 22, HudAccentColor);
		ChapterSubtitleText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* SubtitleSlot = Card->AddChildToVerticalBox(ChapterSubtitleText))
		{
			SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
			SubtitleSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}

		PlaceOnCanvas(Canvas, Card, FAnchors(0.5f, 0.35f, 0.5f, 0.35f), FVector2D(0.5f, 0.5f), FMargin(0.f), true, 2);
		ChapterCard = Card;
	}

	// Interaction prompt, centre-lower.
	{
		PromptText = MakeText(TEXT("PromptText"), 20, FLinearColor::White);
		PlaceOnCanvas(Canvas, PromptText, FAnchors(0.5f, 0.66f, 0.5f, 0.66f), FVector2D(0.5f, 0.5f), FMargin(0.f), true, 1);
	}

	// Squad hint, bottom-left.
	{
		SquadHintText = MakeText(TEXT("SquadHintText"), 16, FLinearColor::White);
		PlaceOnCanvas(Canvas, SquadHintText, FAnchors(0.f, 1.f, 0.f, 1.f), FVector2D(0.f, 1.f), FMargin(40.f, -40.f, 0.f, 0.f), true, 1);
	}

	// Subtitles and choices, bottom-centre, above the letterbox.
	{
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SubtitlePanel"));
		Panel->SetBrushColor(HudPanelColor);
		Panel->SetPadding(FMargin(16.f, 12.f));

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SubtitleRow"));
		Panel->SetContent(Row);

		USizeBox* PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PortraitPanel"));
		PortraitBox->SetWidthOverride(96.f);
		PortraitBox->SetHeightOverride(96.f);
		PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitImage"));
		PortraitBox->SetContent(PortraitImage);
		if (UHorizontalBoxSlot* PortraitSlot = Row->AddChildToHorizontalBox(PortraitBox))
		{
			PortraitSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
			PortraitSlot->SetVerticalAlignment(VAlign_Top);
		}
		PortraitPanel = PortraitBox;

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SubtitleBox"));
		if (UHorizontalBoxSlot* ColumnSlot = Row->AddChildToHorizontalBox(Column))
		{
			// Fill gives the wrapped line a width to wrap against.
			ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		SpeakerText = MakeText(TEXT("SpeakerText"), 18, HudAccentColor);
		Column->AddChildToVerticalBox(SpeakerText);

		LineText = MakeText(TEXT("LineText"), 22, FLinearColor::White);
		LineText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* LineSlot = Column->AddChildToVerticalBox(LineText))
		{
			LineSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		UVerticalBox* Choices = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChoiceList"));
		if (UVerticalBoxSlot* ChoicesSlot = Column->AddChildToVerticalBox(Choices))
		{
			ChoicesSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}
		ChoiceList = Choices;

		PlaceOnCanvas(Canvas, Panel, FAnchors(0.15f, 1.f, 0.85f, 1.f), FVector2D(0.5f, 1.f), FMargin(0.f, -(HudLetterboxHeight + 20.f), 0.f, 0.f), true, 6);
		SubtitlePanel = Panel;
	}

	// Hold-to-skip, bottom-right, over the letterbox.
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SkipPanel"));

		SkipText = MakeText(TEXT("SkipText"), 16, FLinearColor::White);
		Column->AddChildToVerticalBox(SkipText);

		SkipProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("SkipProgressBar"));
		if (UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(SkipProgressBar))
		{
			BarSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		PlaceOnCanvas(Canvas, Column, FAnchors(1.f, 1.f, 1.f, 1.f), FVector2D(1.f, 1.f), FMargin(-40.f, -24.f, 0.f, 0.f), true, 7);
		SkipPanel = Column;
	}

	// Pause menu, over everything.
	{
		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PauseMenuPanel"));
		Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.75f));
		Dim->SetHorizontalAlignment(HAlign_Center);
		Dim->SetVerticalAlignment(VAlign_Center);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseMenuBox"));
		Dim->SetContent(Column);

		PauseTitleText = MakeText(TEXT("PauseTitleText"), 40, FLinearColor::White);
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(PauseTitleText))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
		}

		ResumeButton = MakeButton(Column, TEXT("ResumeButton"), TEXT("ResumeButtonText"), ResumeButtonText);
		SaveButton = MakeButton(Column, TEXT("SaveButton"), TEXT("SaveButtonText"), SaveButtonText);
		QuitButton = MakeButton(Column, TEXT("QuitButton"), TEXT("QuitButtonText"), QuitButtonText);

		PauseStatusText = MakeText(TEXT("PauseStatusText"), 16, HudAccentColor);
		if (UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(PauseStatusText))
		{
			StatusSlot->SetHorizontalAlignment(HAlign_Center);
			StatusSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
		}

		PlaceOnCanvas(Canvas, Dim, FAnchors(0.f, 0.f, 1.f, 1.f), FVector2D(0.f, 0.f), FMargin(0.f), false, 20);
		PauseMenuPanel = Dim;
	}
}

UTextBlock* UNawatHUDWidget::MakeText(FName Name, int32 FontSize, const FLinearColor& Color, bool bShadow)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	SetHudFontSize(Text, FontSize);
	Text->SetColorAndOpacity(FSlateColor(Color));
	if (bShadow)
	{
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	}
	return Text;
}

UButton* UNawatHUDWidget::MakeButton(UPanelWidget* Parent, FName ButtonName, FName TextName, TObjectPtr<UTextBlock>& OutText)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);

	// The engine's default button is light, so its label is dark and has no shadow.
	UTextBlock* Label = MakeText(TextName, 22, FLinearColor(0.02f, 0.02f, 0.02f, 1.f), false);
	Button->SetContent(Label);
	OutText = Label;

	if (UVerticalBoxSlot* ButtonSlot = Cast<UVerticalBoxSlot>(Parent->AddChild(Button)))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetPadding(FMargin(0.f, 6.f));
	}
	return Button;
}

// ---- Story ----

void UNawatHUDWidget::HandleChapterStarted(UNawatChapter* Chapter)
{
	RefreshObjectives();

	if (!Chapter || ChapterCardSeconds <= 0.f)
	{
		return;
	}

	if (ChapterTitleText)
	{
		ChapterTitleText->SetText(Chapter->Title);
	}
	if (ChapterSubtitleText)
	{
		ChapterSubtitleText->SetText(Chapter->Subtitle);
	}

	bChapterCardVisible = true;
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ChapterCardTimer, this, &UNawatHUDWidget::HideChapterCard, ChapterCardSeconds, false);
	}

	OnChapterCardShown(Chapter);
	ApplyVisibility();
}

void UNawatHUDWidget::HandleChapterCompleted(UNawatChapter* Chapter)
{
	RefreshObjectives();
}

void UNawatHUDWidget::HandleStageChanged(UNawatChapter* Chapter, int32 StageIndex)
{
	RefreshObjectives();
}

void UNawatHUDWidget::HandleObjectiveCompleted(UNawatChapter* Chapter, int32 StageIndex, FName ObjectiveId)
{
	RefreshObjectives();
}

void UNawatHUDWidget::RefreshObjectives()
{
	TArray<FNawatObjectiveState> States;
	FText StageTitle;
	bHasObjectives = false;

	if (const UNawatStoryDirector* Director = UNawatStoryDirector::Get(this))
	{
		FNawatStage Stage;
		if (Director->IsChapterActive() && Director->GetCurrentStage(Stage))
		{
			bHasObjectives = true;
			StageTitle = Stage.Title;
			States = Director->GetObjectiveStates();
		}
	}

	if (StageTitleText)
	{
		StageTitleText->SetText(StageTitle);
	}

	if (ObjectiveList)
	{
		ObjectiveList->ClearChildren();

		if (WidgetTree)
		{
			const FText RowFormat = FText::FromString(TEXT("{0} {1}"));
			for (const FNawatObjectiveState& State : States)
			{
				FText RowText = FText::Format(RowFormat, State.bComplete ? ObjectiveDoneMarker : ObjectivePendingMarker, State.Text);
				if (State.bOptional && !ObjectiveOptionalSuffix.IsEmpty())
				{
					RowText = FText::Format(RowFormat, RowText, ObjectiveOptionalSuffix);
				}

				UTextBlock* Row = MakeText(NAME_None, 16, State.bComplete ? HudDoneColor : FLinearColor::White);
				Row->SetText(RowText);
				ObjectiveList->AddChild(Row);
			}
		}
	}

	OnObjectivesChanged(States);
	ApplyVisibility();
}

void UNawatHUDWidget::HideChapterCard()
{
	bChapterCardVisible = false;
	ApplyVisibility();
}

// ---- Dialogue ----

void UNawatHUDWidget::HandleDialogueStarted(UNawatDialogue* Dialogue)
{
	// The first line follows through OnLineShown.
}

void UNawatHUDWidget::HandleDialogueFinished(UNawatDialogue* Dialogue)
{
	bSubtitleVisible = false;
	ShowChoices(TArray<FText>());
	OnSubtitleChanged(FNawatDialogueLine(), false);
	ApplyVisibility();
}

void UNawatHUDWidget::HandleLineShown(const FNawatDialogueLine& Line)
{
	ShowLine(Line);
	// Choices of this line, if any, arrive right after through OnChoicesShown.
	ShowChoices(TArray<FText>());
	ApplyVisibility();
}

void UNawatHUDWidget::HandleChoicesShown(const TArray<FText>& Choices)
{
	ShowChoices(Choices);
}

void UNawatHUDWidget::ShowLine(const FNawatDialogueLine& Line)
{
	bSubtitleVisible = true;

	if (SpeakerText)
	{
		SpeakerText->SetText(Line.SpeakerName);
		SetHudShown(SpeakerText, !Line.SpeakerName.IsEmpty());
	}
	if (LineText)
	{
		LineText->SetText(Line.Text);
	}

	const bool bHasPortrait = Line.Portrait != nullptr;
	if (PortraitImage && bHasPortrait)
	{
		PortraitImage->SetBrushFromTexture(Line.Portrait);
	}
	SetHudShown(PortraitImage, bHasPortrait);
	SetHudShown(PortraitPanel, bHasPortrait);

	OnSubtitleChanged(Line, true);
}

void UNawatHUDWidget::ShowChoices(const TArray<FText>& Choices)
{
	if (ChoiceList)
	{
		ChoiceList->ClearChildren();

		if (WidgetTree)
		{
			const FText RowFormat = FText::FromString(TEXT("{0}. {1}"));
			for (int32 Index = 0; Index < Choices.Num(); ++Index)
			{
				UTextBlock* Row = MakeText(NAME_None, 20, HudAccentColor);
				Row->SetText(FText::Format(RowFormat, FText::AsNumber(Index + 1), Choices[Index]));
				Row->SetAutoWrapText(true);
				ChoiceList->AddChild(Row);
			}
		}
		SetHudShown(ChoiceList, Choices.Num() > 0);
	}

	OnChoicesChanged(Choices);
}

// ---- Interaction ----

void UNawatHUDWidget::HandleFocusChanged(AActor* NewFocus, FText Prompt)
{
	ShowPrompt(Prompt);
	ApplyVisibility();
}

void UNawatHUDWidget::BindInteractor()
{
	if (UNawatInteractorComponent* Component = Interactor.Get())
	{
		if (!bInteractorBound)
		{
			Component->OnFocusChanged.AddUniqueDynamic(this, &UNawatHUDWidget::HandleFocusChanged);
			bInteractorBound = true;
		}
		ShowPrompt(Component->GetFocusedPrompt());
	}
	else
	{
		ShowPrompt(FText::GetEmpty());
	}
	ApplyVisibility();
}

void UNawatHUDWidget::UnbindInteractor()
{
	if (bInteractorBound)
	{
		if (UNawatInteractorComponent* Component = Interactor.Get())
		{
			Component->OnFocusChanged.RemoveDynamic(this, &UNawatHUDWidget::HandleFocusChanged);
		}
		bInteractorBound = false;
	}
}

void UNawatHUDWidget::ShowPrompt(const FText& Prompt)
{
	bPromptVisible = !Prompt.IsEmpty();
	if (PromptText && bPromptVisible)
	{
		const FText KeyName = GetDefault<UNawatSettings>()->InteractKey.GetDisplayName();
		PromptText->SetText(FText::Format(PromptFormat, KeyName, Prompt));
	}
}

// ---- Bus and facts ----

void UNawatHUDWidget::HandleAlertEvent(FGameplayTag Event, const FNawatEventPayload& Payload)
{
	SetAlertLevel(FMath::RoundToInt(Payload.Value));
}

void UNawatHUDWidget::HandleCinematicStartedEvent(FGameplayTag Event, const FNawatEventPayload& Payload)
{
	SetCinematicMode(true);
}

void UNawatHUDWidget::HandleCinematicFinishedEvent(FGameplayTag Event, const FNawatEventPayload& Payload)
{
	SetCinematicMode(false);
}

void UNawatHUDWidget::HandleFactChanged(FGameplayTag Fact, int32 OldValue, int32 NewValue)
{
	if (Fact == NawatTags::Fact_Squad_Present)
	{
		bSquadPresent = NewValue != 0;
		ApplyVisibility();
	}
}

void UNawatHUDWidget::HandleFactsRestored()
{
	RefreshSquadPresence();
	ApplyVisibility();
}

void UNawatHUDWidget::SetAlertLevel(int32 NewLevel)
{
	AlertLevel = FMath::Clamp(NewLevel, 0, 2);

	if (AlertText && AlertLevel > 0)
	{
		const bool bCombat = AlertLevel >= 2;
		AlertText->SetText(bCombat ? AlertCombatLabel : AlertSuspiciousLabel);
		AlertText->SetColorAndOpacity(FSlateColor(bCombat ? HudCombatColor : HudSuspiciousColor));
	}

	OnAlertChanged(AlertLevel);
	ApplyVisibility();
}

void UNawatHUDWidget::SetCinematicMode(bool bNewCinematic)
{
	const UNawatCinematicSubsystem* Cinematics = UNawatCinematicSubsystem::Get(this);

	bCinematic = bNewCinematic;
	bCinematicSkippable = bNewCinematic && Cinematics && Cinematics->CanSkip();
	SetSkipProgress(0.f);

	OnCinematicModeChanged(bCinematic);
	ApplyVisibility();
}

void UNawatHUDWidget::RefreshSquadPresence()
{
	const UNawatWorldState* WorldState = UNawatWorldState::Get(this);
	bSquadPresent = WorldState && WorldState->GetFact(NawatTags::Fact_Squad_Present) != 0;
}

// ---- Pause ----

void UNawatHUDWidget::HandlePauseChanged(bool bAnyReasonActive, const FNawatPausePolicy& EffectivePolicy)
{
	bHideGameplayPanels = EffectivePolicy.bHideHUD;

	const UNawatPauseSubsystem* Pause = FindHudSubsystem<UNawatPauseSubsystem>(this);
	const bool bMenuNow = Pause && Pause->HasReason(NawatTags::Pause_Menu);
	if (bMenuNow != bPauseMenuOpen)
	{
		bPauseMenuOpen = bMenuNow;
		if (PauseStatusText)
		{
			PauseStatusText->SetText(FText::GetEmpty());
		}
		ApplyPauseMenuInputMode(bPauseMenuOpen);
		OnPauseMenuChanged(bPauseMenuOpen);
	}

	ApplyVisibility();
}

void UNawatHUDWidget::HandleResumeClicked()
{
	UNawatPauseSubsystem* Pause = FindHudSubsystem<UNawatPauseSubsystem>(this);
	if (Pause && Pause->HasReason(NawatTags::Pause_Menu))
	{
		Pause->TogglePauseMenu();
	}
}

void UNawatHUDWidget::HandleSaveClicked()
{
	UNawatSaveSubsystem* Save = FindHudSubsystem<UNawatSaveSubsystem>(this);
	const bool bSaved = Save && Save->SaveToSlot();

	if (PauseStatusText)
	{
		PauseStatusText->SetText(bSaved ? SavedLabel : SaveFailedLabel);
	}
}

void UNawatHUDWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UNawatHUDWidget::ApplyStaticTexts()
{
	const UNawatSettings* Settings = GetDefault<UNawatSettings>();

	if (PauseTitleText)
	{
		PauseTitleText->SetText(PauseTitleLabel);
	}
	if (ResumeButtonText)
	{
		ResumeButtonText->SetText(ResumeLabel);
	}
	if (SaveButtonText)
	{
		SaveButtonText->SetText(SaveLabel);
	}
	if (QuitButtonText)
	{
		QuitButtonText->SetText(QuitLabel);
	}
	if (PauseStatusText)
	{
		PauseStatusText->SetText(FText::GetEmpty());
	}
	if (SkipText)
	{
		SkipText->SetText(FText::Format(SkipHintFormat, Settings->SkipCinematicKey.GetDisplayName()));
	}
	if (SquadHintText)
	{
		const FText HintFormat = FText::FromString(TEXT("[{0}] {1}   [{2}] {3}   [{4}] {5}   [{6}] {7}"));
		SquadHintText->SetText(FText::Format(HintFormat,
			Settings->SquadFollowKey.GetDisplayName(), SquadFollowLabel,
			Settings->SquadHoldKey.GetDisplayName(), SquadHoldLabel,
			Settings->SquadAttackKey.GetDisplayName(), SquadAttackLabel,
			Settings->SquadRegroupKey.GetDisplayName(), SquadRegroupLabel));
	}
}

void UNawatHUDWidget::ApplyVisibility()
{
	const bool bGameplay = !bHideGameplayPanels;

	SetHudShown(ObjectivePanel, bHasObjectives && bGameplay);
	SetHudShown(PromptText, bPromptVisible && bGameplay);
	SetHudShown(AlertText, AlertLevel > 0 && bGameplay);
	SetHudShown(SquadHintText, bSquadPresent && bGameplay);

	SetHudShown(ChapterCard, bChapterCardVisible);
	SetHudShown(SubtitlePanel, bSubtitleVisible);
	SetHudShown(LetterboxTop, bCinematic);
	SetHudShown(LetterboxBottom, bCinematic);
	SetHudShown(SkipPanel, bCinematic && bCinematicSkippable);

	// The menu is the only part that takes clicks.
	SetHudShown(PauseMenuPanel, bPauseMenuOpen, ESlateVisibility::Visible);
}

void UNawatHUDWidget::ApplyPauseMenuInputMode(bool bMenuOpen)
{
	if (!bSwitchInputModeForPauseMenu || bMenuOpen == bInputModeApplied)
	{
		return;
	}

	APlayerController* Controller = GetOwningPlayer();
	if (!Controller)
	{
		return;
	}

	if (bMenuOpen)
	{
		// Game And UI lets the buttons take clicks while the pause key still reaches the game.
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(Mode);
	}
	else
	{
		Controller->SetInputMode(FInputModeGameOnly());
	}
	bInputModeApplied = bMenuOpen;
}
