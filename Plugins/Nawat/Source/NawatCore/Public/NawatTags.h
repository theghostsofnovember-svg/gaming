#pragma once

#include "NativeGameplayTags.h"

/** Native tags owned by the kernel. Game-specific facts and events are declared by the project. */
namespace NawatTags
{
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pause_Menu);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pause_Dialogue);
	NAWATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pause_Cinematic);
}
