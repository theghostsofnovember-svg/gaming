#include "NawatMusicSet.h"

#include "Sound/SoundBase.h"

USoundBase* UNawatMusicSet::FindTrack(FGameplayTag State) const
{
	if (!State.IsValid())
	{
		return nullptr;
	}

	const TObjectPtr<USoundBase>* Found = Tracks.Find(State);
	return Found ? Found->Get() : nullptr;
}
