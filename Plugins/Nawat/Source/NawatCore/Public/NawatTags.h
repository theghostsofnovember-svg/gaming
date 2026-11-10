#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags owned by the kernel. This file is the contract between modules: they talk to each
 * other only through these events, requests and facts on UNawatWorldState.
 * Game-specific facts and events (Story.*) are declared by the project.
 *
 * Payload conventions are noted per tag as (Instigator / Subject / Value / Location).
 */
namespace NawatTags
{
	// ---- Pause reasons ----
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pause_Menu);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pause_Dialogue);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pause_Cinematic);

	// ---- Events: something happened ----
	/** Subject = chapter asset. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Story_ChapterStarted);
	/** Subject = chapter asset, Value = new stage index. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Story_StageChanged);
	/** Subject = chapter asset, Value = objective index within the current stage. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Story_ObjectiveCompleted);
	/** Subject = chapter asset. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Story_ChapterCompleted);
	/** Subject = dialogue asset. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Dialogue_Started);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Dialogue_Finished);
	/** Subject = level sequence asset. Finished: Value = 1 if skipped. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Cinematic_Started);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Cinematic_Finished);
	/** Value = highest alert among AI hostile to the player: 0 calm, 1 suspicious, 2 combat. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_AI_AlertChanged);
	/** Instigator = killer (may be null), Subject = the actor that died, Location = where. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_AI_Death);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Squad_Dismissed);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Squad_Summoned);
	/** Instigator = checkpoint actor, Location = respawn location. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Checkpoint_Reached);

	// ---- Requests: ask whichever system owns the behaviour to do something ----
	/** Companions leave the player (solo mission). Location = where they walk to, zero = vanish in place. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Request_Squad_Dismiss);
	/** Companions rejoin. Location = where they appear, zero = near the player. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Request_Squad_Summon);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Request_Squad_Follow);
	/** Location = where to hold, zero = where each companion stands. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Request_Squad_Hold);
	/** Subject = target actor (optional), Location = target location. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Request_Squad_Attack);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Request_Squad_Regroup);
	/** Puts every enemy in combat alert. Location = where they converge. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Request_AI_RaiseAlarm);

	// ---- Facts maintained by the kernel ----
	/** 1 while companions are with the player. */
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fact_Squad_Present);

	// ---- Music states ----
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Explore);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Tension);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Combat);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Music_Cinematic);
}
