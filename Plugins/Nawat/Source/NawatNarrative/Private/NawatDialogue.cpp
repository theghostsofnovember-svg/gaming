#include "NawatDialogue.h"

int32 UNawatDialogue::FindNodeIndex(FName NodeId) const
{
	if (NodeId.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		if (Nodes[Index].Id == NodeId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
