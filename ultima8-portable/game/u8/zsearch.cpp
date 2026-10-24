// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: ZSEARCH.C

#include "u8port.h"

#include "cexit.h"
#include "filespec.h"
#include "scratchm.h"
#include "fastflex.h"
#include "uprocess.h"
#include "unitfile.h"

// Starts a usecode loop: over the items in container `range` when item
// is -1, within `range` of item otherwise, or (type 1) on a surface.

int16_t Searcher::findFirst(char *script, int16_t slot, int16_t range, int16_t item, int16_t scriptSize, SearchType type)
{
	local = slot;
	Item origin(item);
	int16_t found;
	Referent ref;
	if (finder)
		delete finder;
	if (type == 1)
	{
		int16_t what;
		if (range != -1 && item != -1)
		{
			what = 3;
			ref = range;
		}
		else
		{
			if (range != -1)
			{
				what = 1;
				ref = range;
			}
			if (item != -1)
			{
				what = 2;
				ref = item;
			}
		}
		finder = new SurfaceItemFinder(ref, script, scriptSize, what);
	}
	else if (item == -1)
	{
		if (type == 0)
			finder = new RecursiveContainerItemFinder(range, script, scriptSize);
		else
			finder = new ContainerItemFinder(range, script, scriptSize);
	}
	else
	{
		int16_t x = origin.getX();
		int16_t y = origin.getY();
		if (type == 0)
			finder = new RecursiveAreaItemFinder(x - range, y - range, x + range, y + range, script, scriptSize);
		else
			finder = new AreaItemFinder(x - range, y - range, x + range, y + range, script, scriptSize);
	}
	found = *(Referent *)((char *)data + data->bp + local) = found = finder->referent;
	finder->findNext();
	return found;
}

// Stores the current item in the loop variable and moves on.
int16_t Searcher::findNext(void)
{
	int16_t found;
	found = *(Referent *)((char *)data + data->bp + local) = found = finder->referent;
	finder->findNext();
	return found;
}

Searcher::~Searcher(void)
{
	if (finder)
		delete finder;
}
