#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NawatAction.h"
#include "NawatCondition.h"
#include "UObject/SoftObjectPtr.h"
#include "NawatDialogue.generated.h"

class USoundBase;
class UTexture2D;

/** One answer the player can pick. */
USTRUCT(BlueprintType)
struct NAWATNARRATIVE_API FNawatDialogueChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Choice")
	FText Text;

	/** The choice is offered only while these pass. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Choice")
	TArray<TObjectPtr<UNawatCondition>> Conditions;

	/** Run when the player picks this choice. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Choice")
	TArray<TObjectPtr<UNawatAction>> Actions;

	/** None = the node after the one offering this choice, in array order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Choice")
	FName NextNodeId;
};

/** One spoken line, optionally ending in a choice. */
USTRUCT(BlueprintType)
struct NAWATNARRATIVE_API FNawatDialogueNode
{
	GENERATED_BODY()

	/** Name other nodes and choices jump to. Unique within its dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node", meta = (MultiLine = true))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	TSoftObjectPtr<USoundBase> Voice;

	/** Seconds on screen. 0 = automatic, from the text length and the voice clip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node", meta = (ClampMin = "0.0"))
	float Duration = 0.f;

	/** When these fail the node is skipped and the next node in array order is tried. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Node")
	TArray<TObjectPtr<UNawatCondition>> ShowWhen;

	/** Run when the line appears. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Node")
	TArray<TObjectPtr<UNawatAction>> OnShown;

	/** A line with available choices waits for the player instead of advancing on its own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	TArray<FNawatDialogueChoice> Choices;

	/** None = the next node in array order; running past the end finishes the dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	FName NextNodeId;
};

/** A conversation: an ordered list of lines played by UNawatDialogueSubsystem. */
UCLASS(BlueprintType)
class NAWATNARRATIVE_API UNawatDialogue : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FNawatDialogueNode> Nodes;

	/** Returns -1 when no node has that Id. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Dialogue")
	int32 FindNodeIndex(FName NodeId) const;
};
