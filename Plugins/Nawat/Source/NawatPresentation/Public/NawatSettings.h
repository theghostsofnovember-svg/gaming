#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "NawatSettings.generated.h"

class UNawatHUDWidget;
class UNawatMusicSet;

/** Project Settings > Nawat. Everything here has a working default, so nothing must be filled in. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Nawat"))
class NAWATPRESENTATION_API UNawatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ---- HUD ----

	/** Widget Blueprint subclass of NawatHUDWidget. Empty = the HUD built in C++. */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	TSoftClassPtr<UNawatHUDWidget> HUDClass;

	// ---- Music ----

	/** Applied by NawatPlayerLinkComponent when no music set was chosen yet. */
	UPROPERTY(Config, EditAnywhere, Category = "Music")
	TSoftObjectPtr<UNawatMusicSet> DefaultMusicSet;

	/** Derive the base music state from the AI alert level (explore / tension / combat). */
	UPROPERTY(Config, EditAnywhere, Category = "Music")
	bool bAutoMusicFromAlert = true;

	/** Seconds the combat track keeps playing after the alert drops, so it does not flap. */
	UPROPERTY(Config, EditAnywhere, Category = "Music", meta = (ClampMin = "0.0"))
	float CombatMusicHoldSeconds = 5.f;

	// ---- Cinematics ----

	/** Seconds the skip key must be held. 0 skips on press. */
	UPROPERTY(Config, EditAnywhere, Category = "Cinematics", meta = (ClampMin = "0.0"))
	float SkipHoldSeconds = 1.f;

	// ---- Keys ----

	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey PauseKey = EKeys::Escape;

	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey PauseGamepadKey = EKeys::Gamepad_Special_Right;

	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey InteractKey = EKeys::E;

	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey InteractGamepadKey = EKeys::Gamepad_FaceButton_Left;

	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey DialogueAdvanceKey = EKeys::SpaceBar;

	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey DialogueAdvanceGamepadKey = EKeys::Gamepad_FaceButton_Bottom;

	/** Held for SkipHoldSeconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey SkipCinematicKey = EKeys::SpaceBar;

	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey SkipCinematicGamepadKey = EKeys::Gamepad_FaceButton_Bottom;

	/** The four keys below also pick dialogue choices 1-4 while choices are on screen. */
	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey SquadFollowKey = EKeys::One;

	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey SquadHoldKey = EKeys::Two;

	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey SquadAttackKey = EKeys::Three;

	UPROPERTY(Config, EditAnywhere, Category = "Keys")
	FKey SquadRegroupKey = EKeys::Four;
};
