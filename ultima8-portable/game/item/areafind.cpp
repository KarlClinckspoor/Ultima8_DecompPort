// name: ..\ITEM\AREAFIND
// Ported from AREAFIND.ASM.
// Hand-written search loop of AreaItemFinder.
//
// The assembly reached the finder by offset: referent at 0, the near vtable pointer at
// IF_VPTR = 2 (now ItemFinder::vtable), left, top, right, bottom from AF_LEFT = 0x26. The
// call through [vptr + VT_IN_AREA] is the virtual inArea(), AreaItemFinder::callInArea.

#include "u8port.h"

#include "item.h"
#include "itemdata.h"
#include "itemfind.h"

// int AreaItemFinder::inArea(void)
// 1 when the found item lies inside the finder's rectangle.
// The compares are unsigned and the edges belong to the rectangle.
Boolean AreaItemFinder::inArea(void)
{
	uint16_t x = ItemData::xArray[referent];
	uint16_t y = ItemData::yArray[referent];

	if (left > x)
		return 0;
	if (top > y)
		return 0;
	if (right < x)
		return 0;
	if (bottom < y)
		return 0;
	return 1;
}

// void AreaItemFinder::findNext(void)
// Steps along the region lists to the next item in the area that meets the criteria.
// Does nothing once the search has run out (referent 0).
void AreaItemFinder::findNext(void)
{
	for (;;)
	{
		if (referent == 0)
			return;
		referent = ItemData::nextArray[referent];
		if (referent == 0)
		{
			// End of this region's list: on to the next region of the area.
			gotoNextList();
			if (referent == 0)
				return;
		}
		if (!callInArea())
			continue;
		if (!meetsSearchCriteria())
			continue;
		return;
	}
}
