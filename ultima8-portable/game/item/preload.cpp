// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\ITEM\PRELOAD.C

#include "u8port.h"

#include <string.h>
// #include <mem.h>  (DOS only)
#include "cache.h"
#include "item.h"
#include "itemcach.h"
#include "glob.h"
#include "shaphand.h"
#include "cfile.h"
#include "fexist.h"
#include "filespec.h"
#include "cexit.h"
#include "scratchm.h"
#include "statrest.h"
#include "uprocess.h"
#include "type.h"

// A handle on an item found in the map's region lists.
class MapItem : public Item
{
public:
	MapItem(Referent r) { referent = r; }
};

FrameSet *ShapePreloader::type[2];

void FrameSet::init(void)
{
	memset(bits, 0, sizeof(bits));
}

void FrameSet::add(uint16_t frame)
{
	bits[frame >> 3] |= 1 << (frame & 7);
}

char FrameSet::contains(uint16_t frame)
{
	return bits[frame >> 3] & (1 << (frame & 7));
}

char ShapePreloader::init(void)
{
	char ok = TRUE;
	for (int16_t i = 0; i < 2; i++)
	{
		type[i] = new FrameSet[NUM_TYPES / 2];
		if (!type[i])
			ok = FALSE;
	}
	return ok;
}

char ShapePreloader::uninit(void)
{
	char ok = TRUE;
	for (int16_t i = 0; i < 2; i++)
	{
		if (type[i])
			delete type[i];
		type[i] = 0;
	}
	return ok;
}

FrameSet *ShapePreloader::getFrameSet(uint16_t shape)
{
	if (shape < NUM_TYPES)
		return &type[shape >> 10][shape & 0x3ff];
	return 0;
}

void ShapePreloader::add(uint16_t shape, uint16_t frame)
{
	FrameSet *set;
	if ((set = getFrameSet(shape)) != 0)
		set->add(frame);
}

char ShapePreloader::contains(uint16_t shape, uint16_t frame)
{
	FrameSet *set;
	if ((set = getFrameSet(shape)) != 0)
		return set->contains(frame);
	return FALSE;
}

inline Item itemOf(Referent r) { return MapItem(r); }

char ShapePreloader::make(void)
{
	int16_t y, x, glob;
	Referent r;
	Item item;
	if (init())
	{
		for (x = 0; x < 64; x++)
		{
			for (y = 0; y < 64; y++)
			{
				r = ItemCache::regionStartHash[y][x];
				while (r)
				{
					item = itemOf(r);
					add(ItemData::typeArray[item.referent], item.getFrame());
					glob = item.getGlobNum();
					if (GlobalTypes.typeFlags[ItemData::typeArray[item.referent]].family == GLOBEGG_FAMILY && glob)
						globExpander->preload(glob);
					r = ItemData::nextArray[item.referent];
				}
			}
		}
		return TRUE;
	}
	return FALSE;
}

void ShapePreloader::preload(uint8_t)
{
	if (make())
	{
		for (int16_t shape = 0; shape < NUM_TYPES; shape++)
			for (int16_t frame = 0; frame < NUM_FRAMES; frame++)
				if (contains(shape, frame))
					shapeHandler->get(shape, frame);
	}
	uninit();
}
