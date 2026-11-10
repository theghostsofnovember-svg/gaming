#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "NawatMusicSet.generated.h"

class USoundBase;

/** One track per music state. Adding music to the game is adding a row here. Tracks should loop. */
UCLASS(BlueprintType)
class NAWATPRESENTATION_API UNawatMusicSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Keyed by state: Nawat.Music.Explore / Tension / Combat / Cinematic, or any tag of the project. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TMap<FGameplayTag, TObjectPtr<USoundBase>> Tracks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0"))
	float Volume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0"))
	float FadeOutSeconds = 2.f;

	/** Volume multiplier while a pause reason asks to duck the music (the pause menu does). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DuckedVolume = 0.35f;

	/** Null when the set has no track for that state. */
	UFUNCTION(BlueprintPure, Category = "Nawat|Music")
	USoundBase* FindTrack(FGameplayTag State) const;
};
