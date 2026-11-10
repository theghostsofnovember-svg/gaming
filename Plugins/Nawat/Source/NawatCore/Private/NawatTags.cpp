#include "NawatTags.h"

namespace NawatTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Pause_Menu, "Nawat.Pause.Menu", "The pause menu is open.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Pause_Dialogue, "Nawat.Pause.Dialogue", "A dialogue is holding the game.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Pause_Cinematic, "Nawat.Pause.Cinematic", "A cinematic is playing.");

	UE_DEFINE_GAMEPLAY_TAG(Event_Story_ChapterStarted, "Nawat.Event.Story.ChapterStarted");
	UE_DEFINE_GAMEPLAY_TAG(Event_Story_StageChanged, "Nawat.Event.Story.StageChanged");
	UE_DEFINE_GAMEPLAY_TAG(Event_Story_ObjectiveCompleted, "Nawat.Event.Story.ObjectiveCompleted");
	UE_DEFINE_GAMEPLAY_TAG(Event_Story_ChapterCompleted, "Nawat.Event.Story.ChapterCompleted");
	UE_DEFINE_GAMEPLAY_TAG(Event_Dialogue_Started, "Nawat.Event.Dialogue.Started");
	UE_DEFINE_GAMEPLAY_TAG(Event_Dialogue_Finished, "Nawat.Event.Dialogue.Finished");
	UE_DEFINE_GAMEPLAY_TAG(Event_Cinematic_Started, "Nawat.Event.Cinematic.Started");
	UE_DEFINE_GAMEPLAY_TAG(Event_Cinematic_Finished, "Nawat.Event.Cinematic.Finished");
	UE_DEFINE_GAMEPLAY_TAG(Event_AI_AlertChanged, "Nawat.Event.AI.AlertChanged");
	UE_DEFINE_GAMEPLAY_TAG(Event_AI_Death, "Nawat.Event.AI.Death");
	UE_DEFINE_GAMEPLAY_TAG(Event_Squad_Dismissed, "Nawat.Event.Squad.Dismissed");
	UE_DEFINE_GAMEPLAY_TAG(Event_Squad_Summoned, "Nawat.Event.Squad.Summoned");
	UE_DEFINE_GAMEPLAY_TAG(Event_Checkpoint_Reached, "Nawat.Event.Checkpoint.Reached");

	UE_DEFINE_GAMEPLAY_TAG(Request_Squad_Dismiss, "Nawat.Request.Squad.Dismiss");
	UE_DEFINE_GAMEPLAY_TAG(Request_Squad_Summon, "Nawat.Request.Squad.Summon");
	UE_DEFINE_GAMEPLAY_TAG(Request_Squad_Follow, "Nawat.Request.Squad.Follow");
	UE_DEFINE_GAMEPLAY_TAG(Request_Squad_Hold, "Nawat.Request.Squad.Hold");
	UE_DEFINE_GAMEPLAY_TAG(Request_Squad_Attack, "Nawat.Request.Squad.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Request_Squad_Regroup, "Nawat.Request.Squad.Regroup");
	UE_DEFINE_GAMEPLAY_TAG(Request_AI_RaiseAlarm, "Nawat.Request.AI.RaiseAlarm");

	UE_DEFINE_GAMEPLAY_TAG(Fact_Squad_Present, "Nawat.Fact.Squad.Present");

	UE_DEFINE_GAMEPLAY_TAG(Music_Explore, "Nawat.Music.Explore");
	UE_DEFINE_GAMEPLAY_TAG(Music_Tension, "Nawat.Music.Tension");
	UE_DEFINE_GAMEPLAY_TAG(Music_Combat, "Nawat.Music.Combat");
	UE_DEFINE_GAMEPLAY_TAG(Music_Cinematic, "Nawat.Music.Cinematic");
}
