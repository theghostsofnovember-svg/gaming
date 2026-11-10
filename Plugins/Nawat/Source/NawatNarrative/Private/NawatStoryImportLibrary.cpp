#include "NawatStoryImportLibrary.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Texture2D.h"
#include "GameplayTagContainer.h"
#include "Misc/FileHelper.h"
#include "NawatAction.h"
#include "NawatChapter.h"
#include "NawatCondition.h"
#include "NawatDialogue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	typedef TArray<TSharedPtr<FJsonValue>> FNawatJsonArray;

	/** Collects every problem so the author can fix them in one pass. */
	struct FNawatImportContext
	{
		/** Outer of the conditions and actions being created. */
		UObject* Owner = nullptr;
		TArray<FString> Errors;

		void AddError(const FString& Where, const FString& Message)
		{
			Errors.Add(FString::Printf(TEXT("%s: %s"), *Where, *Message));
		}
	};

	TSharedPtr<FJsonObject> ParseRoot(const FString& Json, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = FString::Printf(TEXT("Not valid JSON, or the root is not an object: %s"), *Reader->GetErrorMessage());
			return nullptr;
		}
		return Root;
	}

	FString ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		FString Result;
		Object->TryGetStringField(Field, Result);
		return Result;
	}

	FName ReadName(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		const FString Value = ReadString(Object, Field);
		return Value.IsEmpty() ? FName() : FName(*Value);
	}

	FText ReadText(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		return FText::FromString(ReadString(Object, Field));
	}

	int32 ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32 Default)
	{
		int32 Result = Default;
		return Object->TryGetNumberField(Field, Result) ? Result : Default;
	}

	float ReadFloat(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float Default)
	{
		double Result = 0.0;
		return Object->TryGetNumberField(Field, Result) ? static_cast<float>(Result) : Default;
	}

	bool ReadBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool bDefault)
	{
		bool bResult = bDefault;
		return Object->TryGetBoolField(Field, bResult) ? bResult : bDefault;
	}

	/** Returns null when the field is missing or is not an array. */
	const FNawatJsonArray* ReadArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		const FNawatJsonArray* Result = nullptr;
		return Object->TryGetArrayField(Field, Result) ? Result : nullptr;
	}

	/** Returns null, after recording an error, when the value is not a JSON object. */
	TSharedPtr<FJsonObject> AsObject(const TSharedPtr<FJsonValue>& Value, const FString& Where, FNawatImportContext& Import)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			Import.AddError(Where, TEXT("expected a JSON object"));
			return nullptr;
		}
		return Value->AsObject();
	}

	bool ResolveTag(const FString& TagName, const FString& Where, FNawatImportContext& Import, FGameplayTag& OutTag)
	{
		OutTag = TagName.IsEmpty() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(FName(*TagName), false);
		if (!OutTag.IsValid())
		{
			Import.AddError(Where, FString::Printf(TEXT("unknown gameplay tag '%s'"), *TagName));
			return false;
		}
		return true;
	}

	bool ParseComparison(const FString& Op, ENawatCompare& OutComparison)
	{
		if (Op.IsEmpty() || Op == TEXT(">="))
		{
			OutComparison = ENawatCompare::GreaterOrEqual;
		}
		else if (Op == TEXT("==") || Op == TEXT("="))
		{
			OutComparison = ENawatCompare::Equal;
		}
		else if (Op == TEXT("!="))
		{
			OutComparison = ENawatCompare::NotEqual;
		}
		else if (Op == TEXT(">"))
		{
			OutComparison = ENawatCompare::Greater;
		}
		else if (Op == TEXT("<"))
		{
			OutComparison = ENawatCompare::Less;
		}
		else if (Op == TEXT("<="))
		{
			OutComparison = ENawatCompare::LessOrEqual;
		}
		else
		{
			return false;
		}
		return true;
	}

	void ParseConditions(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Where, FNawatImportContext& Import, TArray<TObjectPtr<UNawatCondition>>& OutConditions);

	UNawatCondition* ParseCondition(const TSharedPtr<FJsonValue>& Value, const FString& Where, FNawatImportContext& Import)
	{
		const TSharedPtr<FJsonObject> Object = AsObject(Value, Where, Import);
		if (!Object.IsValid())
		{
			return nullptr;
		}

		const bool bIsAll = Object->HasField(TEXT("all"));
		const bool bIsAny = Object->HasField(TEXT("any"));
		if (bIsAll || bIsAny)
		{
			UNawatCondition_Group* Group = NewObject<UNawatCondition_Group>(Import.Owner);
			Group->Mode = bIsAll ? ENawatGroupMode::All : ENawatGroupMode::Any;
			Group->bInvert = ReadBool(Object, TEXT("not"), false);
			ParseConditions(Object, bIsAll ? TEXT("all") : TEXT("any"), Where, Import, Group->Conditions);
			return Group;
		}

		if (!Object->HasField(TEXT("fact")))
		{
			Import.AddError(Where, TEXT("a condition needs \"fact\", \"all\" or \"any\""));
			return nullptr;
		}

		FGameplayTag Fact;
		ENawatCompare Comparison = ENawatCompare::GreaterOrEqual;
		const FString Op = ReadString(Object, TEXT("op"));
		const bool bTagOk = ResolveTag(ReadString(Object, TEXT("fact")), Where, Import, Fact);
		const bool bOpOk = ParseComparison(Op, Comparison);
		if (!bOpOk)
		{
			Import.AddError(Where, FString::Printf(TEXT("unknown op '%s' (use ==, !=, >, >=, <, <=)"), *Op));
		}
		if (!bTagOk || !bOpOk)
		{
			return nullptr;
		}

		UNawatCondition_Fact* Condition = NewObject<UNawatCondition_Fact>(Import.Owner);
		Condition->Fact = Fact;
		Condition->Comparison = Comparison;
		Condition->Value = ReadInt(Object, TEXT("value"), 1);
		Condition->bInvert = ReadBool(Object, TEXT("not"), false);
		return Condition;
	}

	void ParseConditions(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Where, FNawatImportContext& Import, TArray<TObjectPtr<UNawatCondition>>& OutConditions)
	{
		if (!Object->HasField(Field))
		{
			return;
		}

		const FNawatJsonArray* Values = ReadArray(Object, Field);
		if (!Values)
		{
			Import.AddError(Where, FString::Printf(TEXT("\"%s\" must be an array"), Field));
			return;
		}

		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const FString ItemWhere = FString::Printf(TEXT("%s.%s[%d]"), *Where, Field, Index);
			if (UNawatCondition* Condition = ParseCondition((*Values)[Index], ItemWhere, Import))
			{
				OutConditions.Add(Condition);
			}
		}
	}

	UNawatAction* ParseAction(const TSharedPtr<FJsonValue>& Value, const FString& Where, FNawatImportContext& Import)
	{
		const TSharedPtr<FJsonObject> Object = AsObject(Value, Where, Import);
		if (!Object.IsValid())
		{
			return nullptr;
		}

		const bool bIsSet = Object->HasField(TEXT("setFact"));
		const bool bIsAdd = Object->HasField(TEXT("addFact"));
		if (bIsSet || bIsAdd)
		{
			FGameplayTag Fact;
			if (!ResolveTag(ReadString(Object, bIsSet ? TEXT("setFact") : TEXT("addFact")), Where, Import, Fact))
			{
				return nullptr;
			}

			UNawatAction_SetFact* Action = NewObject<UNawatAction_SetFact>(Import.Owner);
			Action->Fact = Fact;
			Action->Operation = bIsSet ? ENawatFactOperation::Set : ENawatFactOperation::Add;
			Action->Value = ReadInt(Object, TEXT("value"), 1);
			return Action;
		}

		if (Object->HasField(TEXT("event")))
		{
			FGameplayTag Event;
			if (!ResolveTag(ReadString(Object, TEXT("event")), Where, Import, Event))
			{
				return nullptr;
			}

			UNawatAction_BroadcastEvent* Action = NewObject<UNawatAction_BroadcastEvent>(Import.Owner);
			Action->Event = Event;
			Action->Value = ReadFloat(Object, TEXT("value"), 0.f);
			return Action;
		}

		Import.AddError(Where, TEXT("an action needs \"setFact\", \"addFact\" or \"event\""));
		return nullptr;
	}

	void ParseActions(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Where, FNawatImportContext& Import, TArray<TObjectPtr<UNawatAction>>& OutActions)
	{
		if (!Object->HasField(Field))
		{
			return;
		}

		const FNawatJsonArray* Values = ReadArray(Object, Field);
		if (!Values)
		{
			Import.AddError(Where, FString::Printf(TEXT("\"%s\" must be an array"), Field));
			return;
		}

		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const FString ItemWhere = FString::Printf(TEXT("%s.%s[%d]"), *Where, Field, Index);
			if (UNawatAction* Action = ParseAction((*Values)[Index], ItemWhere, Import))
			{
				OutActions.Add(Action);
			}
		}
	}

	/** Returns null, after recording an error, when a required array is missing. */
	const FNawatJsonArray* ReadRequiredArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Where, FNawatImportContext& Import)
	{
		const FNawatJsonArray* Values = ReadArray(Object, Field);
		if (!Values || Values->Num() == 0)
		{
			Import.AddError(Where, FString::Printf(TEXT("\"%s\" must be a non-empty array"), Field));
			return nullptr;
		}
		return Values;
	}

	bool FinishImport(const FNawatImportContext& Import, FString& OutError)
	{
		OutError = FString::Join(Import.Errors, TEXT("\n"));
		return Import.Errors.Num() == 0;
	}

	UObject* ResolveOuter(UObject* Outer)
	{
		return Outer ? Outer : static_cast<UObject*>(GetTransientPackage());
	}
}

UNawatDialogue* UNawatStoryImportLibrary::ImportDialogueFromJson(UObject* Outer, const FString& Json, FString& OutError)
{
	OutError.Reset();

	const TSharedPtr<FJsonObject> Root = ParseRoot(Json, OutError);
	if (!Root.IsValid())
	{
		return nullptr;
	}

	UNawatDialogue* Dialogue = NewObject<UNawatDialogue>(ResolveOuter(Outer), NAME_None, RF_Transient);

	FNawatImportContext Import;
	Import.Owner = Dialogue;

	if (const FNawatJsonArray* NodeValues = ReadRequiredArray(Root, TEXT("nodes"), TEXT("dialogue"), Import))
	{
		for (int32 NodeIndex = 0; NodeIndex < NodeValues->Num(); ++NodeIndex)
		{
			const FString NodeWhere = FString::Printf(TEXT("nodes[%d]"), NodeIndex);
			const TSharedPtr<FJsonObject> NodeObject = AsObject((*NodeValues)[NodeIndex], NodeWhere, Import);
			if (!NodeObject.IsValid())
			{
				continue;
			}

			FNawatDialogueNode& Node = Dialogue->Nodes.AddDefaulted_GetRef();
			Node.Id = ReadName(NodeObject, TEXT("id"));
			Node.SpeakerName = ReadText(NodeObject, TEXT("speaker"));
			Node.Text = ReadText(NodeObject, TEXT("text"));
			Node.Duration = FMath::Max(0.f, ReadFloat(NodeObject, TEXT("duration"), 0.f));
			Node.NextNodeId = ReadName(NodeObject, TEXT("next"));

			const FString PortraitPath = ReadString(NodeObject, TEXT("portrait"));
			if (!PortraitPath.IsEmpty())
			{
				Node.Portrait = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(PortraitPath));
			}

			const FString VoicePath = ReadString(NodeObject, TEXT("voice"));
			if (!VoicePath.IsEmpty())
			{
				Node.Voice = TSoftObjectPtr<USoundBase>(FSoftObjectPath(VoicePath));
			}

			ParseConditions(NodeObject, TEXT("showWhen"), NodeWhere, Import, Node.ShowWhen);
			ParseActions(NodeObject, TEXT("onShown"), NodeWhere, Import, Node.OnShown);

			if (!NodeObject->HasField(TEXT("choices")))
			{
				continue;
			}

			const FNawatJsonArray* ChoiceValues = ReadArray(NodeObject, TEXT("choices"));
			if (!ChoiceValues)
			{
				Import.AddError(NodeWhere, TEXT("\"choices\" must be an array"));
				continue;
			}

			for (int32 ChoiceIndex = 0; ChoiceIndex < ChoiceValues->Num(); ++ChoiceIndex)
			{
				const FString ChoiceWhere = FString::Printf(TEXT("%s.choices[%d]"), *NodeWhere, ChoiceIndex);
				const TSharedPtr<FJsonObject> ChoiceObject = AsObject((*ChoiceValues)[ChoiceIndex], ChoiceWhere, Import);
				if (!ChoiceObject.IsValid())
				{
					continue;
				}

				FNawatDialogueChoice Choice;
				Choice.Text = ReadText(ChoiceObject, TEXT("text"));
				Choice.NextNodeId = ReadName(ChoiceObject, TEXT("next"));
				ParseConditions(ChoiceObject, TEXT("when"), ChoiceWhere, Import, Choice.Conditions);
				ParseActions(ChoiceObject, TEXT("actions"), ChoiceWhere, Import, Choice.Actions);
				Node.Choices.Add(Choice);
			}
		}
	}

	// Jumps are checked once every node is known, so forward references work.
	for (int32 NodeIndex = 0; NodeIndex < Dialogue->Nodes.Num(); ++NodeIndex)
	{
		const FNawatDialogueNode& Node = Dialogue->Nodes[NodeIndex];
		const FString NodeWhere = FString::Printf(TEXT("nodes[%d]"), NodeIndex);

		if (!Node.Id.IsNone() && Dialogue->FindNodeIndex(Node.Id) != NodeIndex)
		{
			Import.AddError(NodeWhere, FString::Printf(TEXT("duplicate id '%s'"), *Node.Id.ToString()));
		}
		if (!Node.NextNodeId.IsNone() && Dialogue->FindNodeIndex(Node.NextNodeId) == INDEX_NONE)
		{
			Import.AddError(NodeWhere, FString::Printf(TEXT("\"next\" names unknown node '%s'"), *Node.NextNodeId.ToString()));
		}
		for (int32 ChoiceIndex = 0; ChoiceIndex < Node.Choices.Num(); ++ChoiceIndex)
		{
			const FName ChoiceNext = Node.Choices[ChoiceIndex].NextNodeId;
			if (!ChoiceNext.IsNone() && Dialogue->FindNodeIndex(ChoiceNext) == INDEX_NONE)
			{
				Import.AddError(FString::Printf(TEXT("%s.choices[%d]"), *NodeWhere, ChoiceIndex), FString::Printf(TEXT("\"next\" names unknown node '%s'"), *ChoiceNext.ToString()));
			}
		}
	}

	return FinishImport(Import, OutError) ? Dialogue : nullptr;
}

UNawatChapter* UNawatStoryImportLibrary::ImportChapterFromJson(UObject* Outer, const FString& Json, FString& OutError)
{
	OutError.Reset();

	const TSharedPtr<FJsonObject> Root = ParseRoot(Json, OutError);
	if (!Root.IsValid())
	{
		return nullptr;
	}

	UNawatChapter* Chapter = NewObject<UNawatChapter>(ResolveOuter(Outer), NAME_None, RF_Transient);
	Chapter->Title = ReadText(Root, TEXT("title"));
	Chapter->Subtitle = ReadText(Root, TEXT("subtitle"));

	FNawatImportContext Import;
	Import.Owner = Chapter;

	if (const FNawatJsonArray* StageValues = ReadRequiredArray(Root, TEXT("stages"), TEXT("chapter"), Import))
	{
		for (int32 StageIndex = 0; StageIndex < StageValues->Num(); ++StageIndex)
		{
			const FString StageWhere = FString::Printf(TEXT("stages[%d]"), StageIndex);
			const TSharedPtr<FJsonObject> StageObject = AsObject((*StageValues)[StageIndex], StageWhere, Import);
			if (!StageObject.IsValid())
			{
				continue;
			}

			FNawatStage& Stage = Chapter->Stages.AddDefaulted_GetRef();
			Stage.Id = ReadName(StageObject, TEXT("id"));
			Stage.Title = ReadText(StageObject, TEXT("title"));
			Stage.NextStageId = ReadName(StageObject, TEXT("next"));

			ParseActions(StageObject, TEXT("enterActions"), StageWhere, Import, Stage.EnterActions);
			ParseActions(StageObject, TEXT("exitActions"), StageWhere, Import, Stage.ExitActions);
			ParseConditions(StageObject, TEXT("completeWhen"), StageWhere, Import, Stage.CompleteWhen);

			if (StageObject->HasField(TEXT("objectives")))
			{
				const FNawatJsonArray* ObjectiveValues = ReadArray(StageObject, TEXT("objectives"));
				if (!ObjectiveValues)
				{
					Import.AddError(StageWhere, TEXT("\"objectives\" must be an array"));
				}
				else
				{
					for (int32 ObjectiveIndex = 0; ObjectiveIndex < ObjectiveValues->Num(); ++ObjectiveIndex)
					{
						const FString ObjectiveWhere = FString::Printf(TEXT("%s.objectives[%d]"), *StageWhere, ObjectiveIndex);
						const TSharedPtr<FJsonObject> ObjectiveObject = AsObject((*ObjectiveValues)[ObjectiveIndex], ObjectiveWhere, Import);
						if (!ObjectiveObject.IsValid())
						{
							continue;
						}

						FNawatObjective Objective;
						Objective.Id = ReadName(ObjectiveObject, TEXT("id"));
						Objective.Text = ReadText(ObjectiveObject, TEXT("text"));
						Objective.bOptional = ReadBool(ObjectiveObject, TEXT("optional"), false);
						ParseConditions(ObjectiveObject, TEXT("completeWhen"), ObjectiveWhere, Import, Objective.CompleteWhen);
						Stage.Objectives.Add(Objective);
					}
				}
			}

			if (StageObject->HasField(TEXT("branches")))
			{
				const FNawatJsonArray* BranchValues = ReadArray(StageObject, TEXT("branches"));
				if (!BranchValues)
				{
					Import.AddError(StageWhere, TEXT("\"branches\" must be an array"));
				}
				else
				{
					for (int32 BranchIndex = 0; BranchIndex < BranchValues->Num(); ++BranchIndex)
					{
						const FString BranchWhere = FString::Printf(TEXT("%s.branches[%d]"), *StageWhere, BranchIndex);
						const TSharedPtr<FJsonObject> BranchObject = AsObject((*BranchValues)[BranchIndex], BranchWhere, Import);
						if (!BranchObject.IsValid())
						{
							continue;
						}

						FNawatBranch Branch;
						Branch.TargetStageId = ReadName(BranchObject, TEXT("target"));
						ParseConditions(BranchObject, TEXT("when"), BranchWhere, Import, Branch.Conditions);
						Stage.Branches.Add(Branch);
					}
				}
			}
		}
	}

	// Jumps are checked once every stage is known, so forward references work.
	for (int32 StageIndex = 0; StageIndex < Chapter->Stages.Num(); ++StageIndex)
	{
		const FNawatStage& Stage = Chapter->Stages[StageIndex];
		const FString StageWhere = FString::Printf(TEXT("stages[%d]"), StageIndex);

		if (!Stage.Id.IsNone() && Chapter->FindStageIndex(Stage.Id) != StageIndex)
		{
			Import.AddError(StageWhere, FString::Printf(TEXT("duplicate id '%s'"), *Stage.Id.ToString()));
		}
		if (!Stage.NextStageId.IsNone() && Chapter->FindStageIndex(Stage.NextStageId) == INDEX_NONE)
		{
			Import.AddError(StageWhere, FString::Printf(TEXT("\"next\" names unknown stage '%s'"), *Stage.NextStageId.ToString()));
		}
		for (int32 BranchIndex = 0; BranchIndex < Stage.Branches.Num(); ++BranchIndex)
		{
			const FName Target = Stage.Branches[BranchIndex].TargetStageId;
			if (!Target.IsNone() && Chapter->FindStageIndex(Target) == INDEX_NONE)
			{
				Import.AddError(FString::Printf(TEXT("%s.branches[%d]"), *StageWhere, BranchIndex), FString::Printf(TEXT("\"target\" names unknown stage '%s'"), *Target.ToString()));
			}
		}
		for (int32 ObjectiveIndex = 0; ObjectiveIndex < Stage.Objectives.Num(); ++ObjectiveIndex)
		{
			const FName ObjectiveId = Stage.Objectives[ObjectiveIndex].Id;
			if (!ObjectiveId.IsNone() && Stage.FindObjectiveIndex(ObjectiveId) != ObjectiveIndex)
			{
				Import.AddError(FString::Printf(TEXT("%s.objectives[%d]"), *StageWhere, ObjectiveIndex), FString::Printf(TEXT("duplicate id '%s'"), *ObjectiveId.ToString()));
			}
		}
	}

	return FinishImport(Import, OutError) ? Chapter : nullptr;
}

UNawatDialogue* UNawatStoryImportLibrary::ImportDialogueFromFile(UObject* Outer, const FString& FilePath, FString& OutError)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FilePath))
	{
		OutError = FString::Printf(TEXT("Could not read file '%s'."), *FilePath);
		return nullptr;
	}
	return ImportDialogueFromJson(Outer, Json, OutError);
}

UNawatChapter* UNawatStoryImportLibrary::ImportChapterFromFile(UObject* Outer, const FString& FilePath, FString& OutError)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FilePath))
	{
		OutError = FString::Printf(TEXT("Could not read file '%s'."), *FilePath);
		return nullptr;
	}
	return ImportChapterFromJson(Outer, Json, OutError);
}
