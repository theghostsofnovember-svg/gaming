#include "NawatDialogueSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NawatAction.h"
#include "NawatCondition.h"
#include "NawatLog.h"
#include "NawatPauseSubsystem.h"
#include "NawatTags.h"
#include "NawatWorldState.h"
#include "Sound/SoundBase.h"

namespace
{
	const float NawatDialogueMinDuration = 2.f;
	const float NawatDialogueBaseDuration = 1.f;
	const float NawatDialogueSecondsPerCharacter = 0.06f;
	const float NawatDialogueVoiceTail = 0.3f;
	// Longer than any spoken line; filters out looping sounds, which report a huge duration.
	const float NawatDialogueMaxVoiceDuration = 300.f;
}

UNawatDialogueSubsystem* UNawatDialogueSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UNawatDialogueSubsystem>() : nullptr;
}

void UNawatDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UNawatWorldState>();
	Collection.InitializeDependency<UNawatPauseSubsystem>();
}

void UNawatDialogueSubsystem::Deinitialize()
{
	ClearLineTimer();
	StopVoice();

	Current = nullptr;
	CurrentLine = FNawatDialogueLine();
	NodeIndex = INDEX_NONE;
	AvailableChoices.Reset();
	++Serial;

	Super::Deinitialize();
}

UWorld* UNawatDialogueSubsystem::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

bool UNawatDialogueSubsystem::StartDialogue(UNawatDialogue* Dialogue, bool bBlockPlayerInput)
{
	if (!Dialogue || Dialogue->Nodes.Num() == 0)
	{
		UE_LOG(LogNawat, Warning, TEXT("StartDialogue: %s is null or has no nodes; ignored."), *GetNameSafe(Dialogue));
		return false;
	}

	if (Current)
	{
		Finish();
	}

	const int32 MySerial = ++Serial;
	Current = Dialogue;
	NodeIndex = INDEX_NONE;

	const UGameInstance* GameInstance = GetGameInstance();
	if (UNawatPauseSubsystem* Pause = GameInstance ? GameInstance->GetSubsystem<UNawatPauseSubsystem>() : nullptr)
	{
		FNawatPausePolicy Policy;
		Policy.bPauseTime = false;
		Policy.bBlockGameInput = bBlockPlayerInput;
		Pause->PushReason(NawatTags::Pause_Dialogue, Policy);
	}

	OnDialogueStarted.Broadcast(Dialogue);
	BroadcastDialogueEvent(NawatTags::Event_Dialogue_Started, Dialogue);

	// A listener may have stopped or replaced the dialogue.
	if (Serial == MySerial)
	{
		EnterNode(0);
	}
	return true;
}

void UNawatDialogueSubsystem::Advance()
{
	if (!Current || AvailableChoices.Num() > 0)
	{
		return;
	}

	if (!Current->Nodes.IsValidIndex(NodeIndex))
	{
		Finish();
		return;
	}

	EnterNode(ResolveNext(Current->Nodes[NodeIndex].NextNodeId, NodeIndex));
}

bool UNawatDialogueSubsystem::Choose(int32 ChoiceIndex)
{
	UNawatDialogue* Dialogue = Current;
	if (!Dialogue || !AvailableChoices.IsValidIndex(ChoiceIndex) || !Dialogue->Nodes.IsValidIndex(NodeIndex))
	{
		return false;
	}

	const FNawatDialogueNode& Node = Dialogue->Nodes[NodeIndex];
	const int32 SourceIndex = AvailableChoices[ChoiceIndex];
	if (!Node.Choices.IsValidIndex(SourceIndex))
	{
		return false;
	}

	const FNawatDialogueChoice& Choice = Node.Choices[SourceIndex];
	const int32 FromIndex = NodeIndex;
	const int32 MySerial = Serial;

	AvailableChoices.Reset();
	CurrentLine.bHasChoices = false;

	UNawatAction::ExecuteAll(Choice.Actions, this);

	// The actions may have stopped or replaced the dialogue.
	if (Serial == MySerial && Current == Dialogue)
	{
		EnterNode(ResolveNext(Choice.NextNodeId, FromIndex));
	}
	return true;
}

void UNawatDialogueSubsystem::Stop()
{
	Finish();
}

bool UNawatDialogueSubsystem::IsActive() const
{
	return Current != nullptr;
}

UNawatDialogue* UNawatDialogueSubsystem::GetCurrentDialogue() const
{
	return Current;
}

FNawatDialogueLine UNawatDialogueSubsystem::GetCurrentLine() const
{
	return CurrentLine;
}

TArray<FText> UNawatDialogueSubsystem::GetAvailableChoices() const
{
	TArray<FText> Result;
	if (!Current || !Current->Nodes.IsValidIndex(NodeIndex))
	{
		return Result;
	}

	const FNawatDialogueNode& Node = Current->Nodes[NodeIndex];
	for (const int32 SourceIndex : AvailableChoices)
	{
		if (Node.Choices.IsValidIndex(SourceIndex))
		{
			Result.Add(Node.Choices[SourceIndex].Text);
		}
	}
	return Result;
}

void UNawatDialogueSubsystem::EnterNode(int32 NewNodeIndex)
{
	UNawatDialogue* Dialogue = Current;
	if (!Dialogue)
	{
		return;
	}

	ClearLineTimer();
	StopVoice();
	AvailableChoices.Reset();

	if (NewNodeIndex < 0)
	{
		Finish();
		return;
	}

	while (Dialogue->Nodes.IsValidIndex(NewNodeIndex) && !UNawatCondition::AllPass(Dialogue->Nodes[NewNodeIndex].ShowWhen, this))
	{
		++NewNodeIndex;
	}

	if (!Dialogue->Nodes.IsValidIndex(NewNodeIndex))
	{
		Finish();
		return;
	}

	const int32 MySerial = ++Serial;
	NodeIndex = NewNodeIndex;
	const FNawatDialogueNode& Node = Dialogue->Nodes[NewNodeIndex];

	CurrentLine = FNawatDialogueLine();
	CurrentLine.SpeakerName = Node.SpeakerName;
	CurrentLine.Text = Node.Text;

	// Actions run first so the choices below see the facts they set.
	UNawatAction::ExecuteAll(Node.OnShown, this);
	if (Serial != MySerial)
	{
		return;
	}

	for (int32 Index = 0; Index < Node.Choices.Num(); ++Index)
	{
		if (UNawatCondition::AllPass(Node.Choices[Index].Conditions, this))
		{
			AvailableChoices.Add(Index);
		}
	}

	CurrentLine.Portrait = Node.Portrait.IsNull() ? nullptr : Node.Portrait.LoadSynchronous();
	CurrentLine.bHasChoices = AvailableChoices.Num() > 0;

	USoundBase* Voice = Node.Voice.IsNull() ? nullptr : Node.Voice.LoadSynchronous();
	if (Voice)
	{
		VoiceComponent = UGameplayStatics::SpawnSound2D(this, Voice);
	}

	if (!CurrentLine.bHasChoices)
	{
		float Duration = Node.Duration;
		if (Duration <= 0.f)
		{
			Duration = FMath::Max(NawatDialogueMinDuration, NawatDialogueBaseDuration + NawatDialogueSecondsPerCharacter * Node.Text.ToString().Len());

			const float VoiceDuration = Voice ? Voice->GetDuration() : 0.f;
			if (VoiceDuration > 0.f && VoiceDuration < NawatDialogueMaxVoiceDuration)
			{
				Duration = FMath::Max(Duration, VoiceDuration + NawatDialogueVoiceTail);
			}
		}
		CurrentLine.Duration = Duration;

		if (UGameInstance* GameInstance = GetGameInstance())
		{
			GameInstance->GetTimerManager().SetTimer(LineTimer, this, &UNawatDialogueSubsystem::HandleLineTimer, Duration, false);
		}
	}

	const FNawatDialogueLine Line = CurrentLine;
	OnLineShown.Broadcast(Line);
	if (Serial != MySerial)
	{
		return;
	}

	if (Line.bHasChoices)
	{
		OnChoicesShown.Broadcast(GetAvailableChoices());
	}
}

int32 UNawatDialogueSubsystem::ResolveNext(FName NextNodeId, int32 FromIndex) const
{
	if (NextNodeId.IsNone())
	{
		return FromIndex + 1;
	}

	const int32 TargetIndex = Current ? Current->FindNodeIndex(NextNodeId) : INDEX_NONE;
	if (TargetIndex == INDEX_NONE)
	{
		UE_LOG(LogNawat, Warning, TEXT("%s: no node is named '%s'; the dialogue ends here."), *GetNameSafe(Current.Get()), *NextNodeId.ToString());
	}
	return TargetIndex;
}

void UNawatDialogueSubsystem::Finish()
{
	UNawatDialogue* Finished = Current;
	if (!Finished)
	{
		return;
	}

	++Serial;
	Current = nullptr;
	CurrentLine = FNawatDialogueLine();
	NodeIndex = INDEX_NONE;
	AvailableChoices.Reset();

	ClearLineTimer();
	StopVoice();

	const UGameInstance* GameInstance = GetGameInstance();
	if (UNawatPauseSubsystem* Pause = GameInstance ? GameInstance->GetSubsystem<UNawatPauseSubsystem>() : nullptr)
	{
		Pause->PopReason(NawatTags::Pause_Dialogue);
	}

	OnDialogueFinished.Broadcast(Finished);
	BroadcastDialogueEvent(NawatTags::Event_Dialogue_Finished, Finished);
}

void UNawatDialogueSubsystem::HandleLineTimer()
{
	Advance();
}

void UNawatDialogueSubsystem::StopVoice()
{
	if (UAudioComponent* Voice = VoiceComponent.Get())
	{
		Voice->Stop();
	}
	VoiceComponent.Reset();
}

void UNawatDialogueSubsystem::ClearLineTimer()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(LineTimer);
	}
}

void UNawatDialogueSubsystem::BroadcastDialogueEvent(FGameplayTag Event, UNawatDialogue* Subject) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	UNawatWorldState* WorldState = GameInstance ? GameInstance->GetSubsystem<UNawatWorldState>() : nullptr;
	if (!WorldState)
	{
		return;
	}

	FNawatEventPayload Payload;
	Payload.Subject = Subject;
	WorldState->BroadcastEvent(Event, Payload);
}
