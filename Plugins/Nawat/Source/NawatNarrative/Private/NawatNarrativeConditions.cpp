#include "NawatNarrativeConditions.h"

#include "NawatChapter.h"
#include "NawatDialogue.h"
#include "NawatDialogueSubsystem.h"
#include "NawatStoryDirector.h"

bool UNawatCondition_StageIs::Test_Implementation(const UObject* Context) const
{
	const UNawatStoryDirector* Director = UNawatStoryDirector::Get(Context);
	if (!Director || !Director->IsChapterActive())
	{
		return false;
	}

	if (Chapter && Director->GetCurrentChapter() != Chapter)
	{
		return false;
	}

	return Director->GetCurrentStageId() == StageId;
}

bool UNawatCondition_ObjectiveComplete::Test_Implementation(const UObject* Context) const
{
	const UNawatStoryDirector* Director = UNawatStoryDirector::Get(Context);
	return Director && Director->IsObjectiveComplete(ObjectiveId);
}

bool UNawatCondition_DialogueActive::Test_Implementation(const UObject* Context) const
{
	const UNawatDialogueSubsystem* DialogueSubsystem = UNawatDialogueSubsystem::Get(Context);
	if (!DialogueSubsystem || !DialogueSubsystem->IsActive())
	{
		return false;
	}

	return !Dialogue || DialogueSubsystem->GetCurrentDialogue() == Dialogue;
}
