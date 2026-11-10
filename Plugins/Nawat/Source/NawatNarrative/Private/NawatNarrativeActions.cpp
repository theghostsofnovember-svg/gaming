#include "NawatNarrativeActions.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NawatChapter.h"
#include "NawatDialogue.h"
#include "NawatDialogueSubsystem.h"
#include "NawatLog.h"
#include "NawatSaveSubsystem.h"
#include "NawatStoryDirector.h"

void UNawatAction_StartChapter::Run_Implementation(UObject* Context)
{
	UNawatStoryDirector* Director = UNawatStoryDirector::Get(Context);
	if (!Director)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a world context; skipped."), *GetName());
		return;
	}

	Director->StartChapter(Chapter, StartStageId);
}

void UNawatAction_AdvanceStage::Run_Implementation(UObject* Context)
{
	UNawatStoryDirector* Director = UNawatStoryDirector::Get(Context);
	if (!Director)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a world context; skipped."), *GetName());
		return;
	}

	Director->AdvanceStage();
}

void UNawatAction_GoToStage::Run_Implementation(UObject* Context)
{
	UNawatStoryDirector* Director = UNawatStoryDirector::Get(Context);
	if (!Director)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a world context; skipped."), *GetName());
		return;
	}

	Director->GoToStage(StageId);
}

void UNawatAction_CompleteObjective::Run_Implementation(UObject* Context)
{
	UNawatStoryDirector* Director = UNawatStoryDirector::Get(Context);
	if (!Director)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a world context; skipped."), *GetName());
		return;
	}

	Director->CompleteObjective(ObjectiveId);
}

void UNawatAction_StartDialogue::Run_Implementation(UObject* Context)
{
	UNawatDialogueSubsystem* DialogueSubsystem = UNawatDialogueSubsystem::Get(Context);
	if (!DialogueSubsystem)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a world context; skipped."), *GetName());
		return;
	}

	DialogueSubsystem->StartDialogue(Dialogue, bBlockPlayerInput);
}

void UNawatAction_SaveGame::Run_Implementation(UObject* Context)
{
	const UWorld* World = (GEngine && Context) ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UNawatSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UNawatSaveSubsystem>() : nullptr;
	if (!SaveSubsystem)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s ran without a world context; skipped."), *GetName());
		return;
	}

	SaveSubsystem->SaveToSlot(SlotName);
}
