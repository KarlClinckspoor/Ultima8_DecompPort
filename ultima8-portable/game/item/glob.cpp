// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\ITEM\GLOB.C

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "cfile.h"
#include "filespec.h"
#include "scratchm.h"
#include "cache.h"
#include "error.h"
#include "dlist.h"
#include "item.h"
#include "itemcach.h"
#include "type.h"
#include "glob.h"

// Inline in the shared headers when this file was built.
U8_INLINE uint8_t BaseFile::is_valid(void) { return handle != -1; }
U8_INLINE BaseFile::~BaseFile(void) { if (is_valid()) close(); }
U8_INLINE Index::Index(void) { size = 0; offset = 0; }
U8_INLINE Point::Point(void) {}
U8_INLINE void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE Rect::Rect(void) {}
U8_INLINE void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
U8_INLINE Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }
U8_INLINE uint8_t Rect::intersects(Rect &r)
{
	return f_00 <= r.f_04 && f_02 <= r.f_06 && f_04 >= r.f_00 && f_06 >= r.f_02;
}

#define REGION_SHIFT	9

// One item of a glob, placed relative to the glob's region.
struct GlobRecord
{
	uint8_t x;		// in 2-unit steps
	uint8_t y;
	uint8_t z;
	uint16_t type;
	uint8_t frame;
};

// A glob egg: the item that stands in for a glob until it is expanded.
class GlobEgg : public Item
{
public:
	GlobEgg(Item item) { referent = item.referent; }
};

// The map region holding (x, y).
U8_INLINE void worldToRegion(uint16_t x, uint16_t y, int16_t &rx, int16_t &ry)
{
	rx = x >> REGION_SHIFT;
	ry = y >> REGION_SHIFT;
}

// The world corner of region (rx, ry).
U8_INLINE void regionOrigin(int16_t rx, int16_t ry, uint16_t &x, uint16_t &y)
{
	x = rx << REGION_SHIFT;
	y = ry << REGION_SHIFT;
}

GlobFile *GlobExpander::globFile = 0;
GlobExpander *globExpander = 0;
uint16_t GlobExpander::startHandle;
uint16_t GlobExpander::globsIn[MAX_GLOBS_IN];

GlobFile::GlobFile(void) :
	FlexFile(fileSpec(0, StaticDir, globFileName, 0), (OpenMode)0, 3072)
{
}

GlobExpander::GlobExpander(void) :
	CacheHandler((CacheNodeType)2)
{
	startHandle = CacheHandler::reserve(MAX_GLOBS_IN);
	CacheNodePtr::attach(this, (CacheNodeType)2);
	memset(globsIn, 0xff, sizeof(globsIn));
	globFile = new GlobFile;
	if (globFile == 0)
		outOfMemory(__FILE__, 201);	// __LINE__
}

GlobExpander::~GlobExpander(void)
{
	if (globFile)
	{
		delete globFile;
		globFile = 0;
	}
}

int32_t GlobExpander::getSizeFromDisk(uint16_t glob)
{
	Index index;
	globFile->getIndex(glob, index);
	return index.size;
}

// The glob's data in the cache, read from disk if it is not there yet.
void *GlobExpander::get(uint16_t glob)
{
	uint16_t handle = FREE_HANDLE;
	uint32_t node = 0;
	int16_t i;

	for (i = 0; i < MAX_GLOBS_IN; i++)
	{
		if (globsIn[i] == glob)
		{
			handle = startHandle + i;
			break;
		}
	}
	if (handle != FREE_HANDLE)
	{
		node = handles[handle];
		CacheNodePtr(node - NODE_HEADER).makeMru();
	}
	else
	{
		for (i = 0; i < MAX_GLOBS_IN; i++)
		{
			if (globsIn[i] == 0xffff)
			{
				handle = startHandle + i;
				globsIn[i] = glob;
				break;
			}
		}
		if (i >= MAX_GLOBS_IN)
		{
			handle = CacheNodePtr::deallocateLru((CacheNodeType)2);
			globsIn[handle - startHandle] = glob;
		}
		int32_t size = getSizeFromDisk(glob);
		if (size < 2)
		{
			// An empty glob still holds its item count.
			size = 2;
			node = CacheNodePtr::allocate(size, handle, (CacheNodeType)2);
			*(int16_t *)CacheNodePtr::getPtr(node) = 0;
		}
		else
		{
			node = CacheNodePtr::allocate(size, handle, (CacheNodeType)2);
			globFile->readRecord(glob, CacheNodePtr::getPtr(node));
		}
		if (node == 0)
			halt(__FILE__, 293);	// __LINE__
	}
	return CacheNodePtr::getPtr(node);
}

void GlobExpander::cacheOut(uint32_t, uint16_t handle)
{
	globsIn[handle - startHandle] = 0xffff;
}

void GlobExpander::expand(Item item, uint8_t now)
{
	GlobEgg egg(item.referent);
	int16_t globNum = egg.getGlobNum();
	expand(globNum, ItemData::xArray[egg.referent], ItemData::yArray[egg.referent],
		ItemData::zArray[egg.referent], now);
}

// Creates the glob's items around (x, y, z). Unless asked to do it now, a
// glob far from the screen is only queued on its region.

int16_t GlobExpander::expand(uint16_t globNum, uint16_t x, uint16_t y, uint8_t z,
	uint8_t now)
{
	OneItem *items;
	GlobRecord *rec;
	OneItem *p;
	Item item;
	int16_t i;
	int16_t count;
	int16_t rx;
	int16_t ry;
	uint16_t x0;
	uint16_t y0;
	char *data;

	data = (char *)get(globNum);
	count = *(int16_t *)data;
	items = new OneItem[count];
	p = items;
	data = (char *)get(globNum);
	worldToRegion(x, y, rx, ry);
	regionOrigin(rx, ry, x0, y0);
	x0++;
	y0++;
	if (count > ItemCache::freeCount)
		halt(__FILE__, 351);	// __LINE__
	rec = (GlobRecord *)(data + 2);
	for (i = 0; i < count; i++)
	{
		p->x = rec->x * 2 + x0;
		p->y = rec->y * 2 + y0;
		p->z = rec->z + z;
		p->type = rec->type;
		if (p->type >= NUM_TYPES)
			halt(__FILE__, 364);	// __LINE__
		p->frame = rec->frame;
		p->flags = 0x82;
		p->quality = 0;
		p->npcNum = 0;
		p++;
		rec++;
	}

	if (!now)
	{
		int16_t sx;
		int16_t sy;
		Rect area(0, 0, 0, 0);
		Rect screen(0, 0, 0, 0);
		sx = ShadowSquareLC;
		sy = ShadowSquareTC;
		screen = Rect(sx, sy, sx + 383, sy + 319);
		sx = (x0 >> 2) - (y0 >> 2);
		sy = (x0 >> 3) + (y0 >> 3);
		area = Rect(sx - 240, sy - 250, sx + 239, sy + 127);
		if (area.intersects(screen))
			now = TRUE;
	}
	if (now)
	{
		p = items;
		for (i = 0; i < count; i++)
		{
			if (item.create(p->type, p->frame))
			{
				item.pop(p->x, p->y, p->z);
				item.orStatus(0x82);
				p++;
			}
		}
	}
	else
		ItemCache::prependToRegion(rx, ry, (char *)items, count);
	delete items;
	return count;
}

void GlobExpander::preload(int16_t globNum)
{
	GlobRecord *rec;
	int16_t i;
	int16_t count;
	char *data;

	data = (char *)get(globNum);
	count = *(int16_t *)data;
	for (i = 0; i < count; i++)
	{
		data = (char *)get(globNum);
		rec = (GlobRecord *)(data + 2) + i;
		ShapePreloader::add(rec->type, rec->frame);
	}
}

void GlobExpander::flushGlob(uint16_t globNum)
{
	int16_t i;

	for (i = 0; i < MAX_GLOBS_IN; i++)
	{
		if (globsIn[i] == globNum)
		{
			CacheNodePtr(handles[i + startHandle] - NODE_HEADER).free();
			globsIn[i] = 0xffff;
			return;
		}
	}
}

int16_t GlobExpander::countItems(int16_t globNum)
{
	int16_t *data = (int16_t *)get(globNum);
	return *data;
}

void initGlobExpander(void)
{
	globExpander = new GlobExpander;
	if (globExpander == 0)
		outOfMemory(__FILE__, 471);	// __LINE__
}

void uninitGlobExpander(void)
{
	if (globExpander)
		delete globExpander;
}
