#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NawatStoryImportLibrary.generated.h"

class UNawatChapter;
class UNawatDialogue;

/**
 * Builds chapters and dialogues from JSON text at runtime, so story content written outside the
 * editor can be played without hand-authoring assets. The schema is documented in Docs/Narrative.md.
 *
 * Imports are all-or-nothing: on any problem the result is null and OutError lists every problem,
 * one per line. The returned object is transient; keep a reference to it or it is garbage collected.
 */
UCLASS()
class NAWATNARRATIVE_API UNawatStoryImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Outer may be null, in which case the transient package owns the result. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Import")
	static UNawatDialogue* ImportDialogueFromJson(UObject* Outer, const FString& Json, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Nawat|Import")
	static UNawatChapter* ImportChapterFromJson(UObject* Outer, const FString& Json, FString& OutError);

	/** FilePath is an absolute path or one relative to the process working directory. */
	UFUNCTION(BlueprintCallable, Category = "Nawat|Import")
	static UNawatDialogue* ImportDialogueFromFile(UObject* Outer, const FString& FilePath, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Nawat|Import")
	static UNawatChapter* ImportChapterFromFile(UObject* Outer, const FString& FilePath, FString& OutError);
};
