// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r- -y
// name: ..\ITEM\ITEMCACH.C

#include "u8port.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// #include <conio.h>  (DOS only)
#include "item.h"
#include "npc.h"
#include "contain.h"
#include "camera.h"
#include "occlude.h"
#include "dlist.h"
#include "dlitem.h"
#include "irgump.h"
#include "kernel.h"
#include "chargen.h"
#include "mapfile.h"
#include "gametime.h"
#include "basecam.h"
#include "message.h"
#include "menu.h"
#include "main.h"
#include "dispatch.h"
#include "filespec.h"
#include "fexist.h"
#include "scratchm.h"
#include "type.h"
#include "glob.h"
#include "gravity.h"
#include "error.h"
#include "itemcach.h"

#define REGION_SHIFT	9
#define REGIONS			64

#define MAX_NPCS		256
#define Z_GONE			0xfb	// z of an item out of play
#define Z_FREE			0xfe	// z of a free node

// More item status bits
#define IN_NPC_LIST		0x0040
#define FAST_ONLY		0x0080
#define EQUIPPED		0x0200
#define HAS_GUMP		0x8000	// an ItemRelativeGump follows it

// Inline in the shared headers when this file was built.
U8_INLINE uint8_t BaseFile::is_valid(void) { return handle != -1; }
U8_INLINE BaseFile::~BaseFile(void) { if (is_valid()) close(); }
U8_INLINE Index::Index(void) { size = 0; offset = 0; }
U8_INLINE NewGumpId::NewGumpId(uint16_t i) { instance = i; f_02 = ~instance; f_04 = 0; }

// One bit per item in the display list's tables.
U8_INLINE void setNew(Referent r) { DisplayList::newitem[r >> 5] |= 1L << (r & 31); }
U8_INLINE void clearNew(Referent r) { DisplayList::newitem[r >> 5] &= ~(1L << (r & 31)); }
U8_INLINE void clearAnimating(Referent r) { DisplayList::animatingitem[r >> 5] &= ~(1L << (r & 31)); }

// The game time in hours.
U8_INLINE int32_t gameHours(void) { return theAnimation->f_44; }

// The map region holding (x, y).
U8_INLINE void worldToRegion(uint16_t x, uint16_t y, int16_t &rx, int16_t &ry)
{
	rx = x >> REGION_SHIFT;
	ry = y >> REGION_SHIFT;
}

int16_t ItemCache::mapIn = -1;
uint16_t (*ItemCache::regionStartHash)[64] = 0;
uint16_t (*ItemCache::itemCount)[64] = 0;
MapFile *ItemCache::itemFile = 0;
FixedMapFile *ItemCache::fixedItemFile = 0;
uint8_t ItemCache::expandingGlobs = 1;
char U7FileName[9] = "u7items.";
char ItemCacheFileName[13] = "itemcach.dat";
RegionRectangle ItemCache::regionsIn;
uint16_t ItemCache::firstFree;
uint16_t ItemCache::firstFreeNpc;
uint16_t ItemCache::lastFree;
uint16_t ItemCache::lastFreeNpc;
int16_t ItemCache::freeCount;
uint16_t ItemCache::freeNpcCount;
uint16_t ItemCache::etherealTop;
int16_t ItemCache::etherealCount;

// The corners of the diamond are left out.
Boolean RegionRectangle::isNeeded(int16_t rx, int16_t ry)
{
	if (rx < 0 || rx >= REGIONS || ry < 0 || ry >= REGIONS)
		return FALSE;
	int16_t sum = rx + ry;
	int16_t diff = rx - ry;
	if (minSum == sum && minDiff == diff ||
		minSum == sum && maxDiff == diff ||
		maxSum == sum && minDiff == diff ||
		maxSum == sum && maxDiff == diff)
		return FALSE;
	return minSum <= sum && maxSum >= sum && minDiff <= diff && maxDiff >= diff;
}

void RegionRectangle::calculateNeeded(uint16_t x, uint16_t y)
{
	int16_t rx;
	int16_t ry;

	worldToRegion(x, y, rx, ry);
	int16_t sum = rx + ry;
	int16_t diff = rx - ry;
	minSum = sum - 7;
	maxSum = sum + 7;
	minDiff = diff - 3;
	maxDiff = diff + 3;
}

// Links a block of items, stored one after another in data, to the front
// of a region's list, taking their nodes from the free list.

void ItemCache::prependToRegion(uint16_t rx, uint16_t ry, char *data, int16_t count)
{
	uint8_t wasEmpty = 0;
	if (count > 0)
	{
		Referent oldStart = regionStartHash[rx][ry];
		regionStartHash[rx][ry] = firstFree;
		if (!oldStart)
			wasEmpty++;

		freeCount -= count;
		itemCount[rx][ry] += count;

		OneItem *item = (OneItem *)data;
		Referent last;
		Referent r = firstFree;
		Referent next;
		for (int16_t i = 0; i < count; i++)
		{
			if (!r)
				halt(__FILE__, 256);	// __LINE__
			last = r;
			next = nextArray[r];
			putItem(r, *item);
			if (typeArray[r] >= NUM_TYPES)
			{
				typeArray[r] = 0;
				halt(__FILE__, 269);	// __LINE__
			}
			nextArray[last] = next;
			statusArray[last] &= ~1;
			r = next;
			item++;
		}
		if (wasEmpty)
			nextArray[last] = 0;
		else
			nextArray[last] = oldStart;
		firstFree = next;
	}
}

void ItemCache::loadRegion(int16_t rx, int16_t ry)
{
	if (rx >= 0 && rx < REGIONS && ry >= 0 && ry <= REGIONS)
	{
		int16_t glob;
		Index index;
		Item unused;
		Referent r;
		Item item;

		if (expandingGlobs)
		{
			for (r = regionStartHash[rx][ry]; r; r = nextArray[r])
			{
				item = Item(r);
				glob = item.getGlobNum();
				if (GlobalTypes.typeFlags[typeArray[r]].family == GLOBEGG_FAMILY && glob)
					globExpander->expand(item, 0);
			}
		}
		for (r = regionStartHash[rx][ry]; r; r = nextArray[r])
			Item(r).enterFastArea();
	}
}

void ItemCache::dumpRegion(int16_t rx, int16_t ry)
{
	if (rx >= 0 && rx < REGIONS && ry >= 0 && ry <= REGIONS)
	{
		Referent next;
		Item item;
		Referent r;

		for (r = regionStartHash[rx][ry]; r; )
		{
			item = Item(r);
			r = nextArray[item.referent];
			GravityTracker *tracker = (GravityTracker *)Kernel::findValidProcess(item.referent, (ProcessType)0x203);
			if (tracker)
				tracker->cancel();
		}
		for (r = regionStartHash[rx][ry]; r; r = next)
		{
			item = Item(r);
			next = nextArray[item.referent];
			if (statusArray[r] & FAST_ONLY)
			{
				if (GlobalTypes.typeFlags[item.getType()].noisy && (statusArray[r] & FAST_AREA))
					item.leaveFastArea();
				item.destroyContents();
				item.destroy();
			}
			else
				item.leaveFastArea();
		}
	}
}

// Writes the loaded map level back to the cache file and, if asked, takes
// its items out of the world: fixed items in the first pass, movable items in
// the second. A container's contents follow it, with x set to their depth.

void ItemCache::dumpMapLevel(uint8_t save, uint8_t destroy)
{
	Referent r;
	Referent next;
	Item unused;
	Item child;
	OneItem *p;
	uint16_t depth;
	int16_t pass;
	uint16_t count;
	Index index;
	int32_t writeOffset;
	TypeFlag flags;
	FlexFile *file;
	uint8_t cantSave = avatar.f_3b;

	for (pass = 0; pass < 2; pass++)
	{
		int16_t family;

		writeOffset = 0;
		if (pass == 0)
		{
			if (inGameMode)
				file = 0;
			else
				file = fixedItemFile;
		}
		else
			file = itemFile;

		if (save && mapIn >= 0)
		{
			OneItem *buffer;
			int16_t ry;
			int16_t rx;
			Boolean fixed;

			if (file)
			{
				file->relocate(mapIn);
				file->getIndex(mapIn, index);
			}
			buffer = (OneItem *)theScratchMem->checkOut(4000 * sizeof(OneItem));
			if (!buffer)
				outOfMemory(__FILE__, 450);	// __LINE__
			p = buffer;
			count = depth = 0;
			for (ry = 0; ry < REGIONS; ry++)
			{
				for (rx = 0; rx < REGIONS; rx++)
				{
					if (destroy)
						dumpRegion(rx, ry);
					r = regionStartHash[rx][ry];
					while (r)
					{
						flags = GlobalTypes.typeFlags[typeArray[r]];
						fixed = flags.fixed;
						if (pass == 0 && !fixed || pass == 1 && fixed)
						{
							r = nextArray[r];
							continue;
						}
						if (Item(r).isNpc() && mapIn)
						{
							next = nextArray[r];
							if (destroy)
							{
								Item(r).leaveFastArea();
								push(r);
								popToLunch(0);
							}
							r = next;
							continue;
						}
						child = Item(r).getContents();
						depth = 1;
						if (count < 4000 && !(statusArray[r] & DISPOSABLE))
						{
							getItem(r, *p);
							if (((family = flags.family) == QUAN_FAMILY || family == REAGENT_FAMILY) && p->quality == 0)
								p->quality = 1;
							if (child.isValid())
								p->quality = 0;
							p->flags &= ~(CONTAINER_GUMP_OPEN | HAS_GUMP);
							p++;
							count++;
							if (count == 4000)
							{
								if (file)
									file->writeRecord(mapIn, buffer, count * sizeof(OneItem), writeOffset, 0);
								writeOffset += count * sizeof(OneItem);
								count = 0;
								p = buffer;
							}
						}
						next = nextArray[r];

						// Walk the contents tree depth first.
						if (child.isValid())
						{
							while (child.isValid() && child.referent != r)
							{
								if (count < 4000)
								{
									getItem(child.referent, *p);
									flags = GlobalTypes.typeFlags[typeArray[child.referent]];
									if (((family = flags.family) == QUAN_FAMILY || family == REAGENT_FAMILY) &&
										p->quality == 0)
										p->quality = 1;
									p->x = depth;
									if (child.getContents())
										p->quality = 0;
									p->flags &= ~(CONTAINER_GUMP_OPEN | HAS_GUMP);
									p++;
									count++;
									if (count == 4000)
									{
										if (file)
											file->writeRecord(mapIn, buffer, count * sizeof(OneItem), writeOffset, 0);
										writeOffset += count * sizeof(OneItem);
										count = 0;
										p = buffer;
									}
								}
								if (child.getContents())
								{
									child = child.getContents();
									depth++;
								}
								else
								{
									while (!child.getNext() && child.referent != r)
									{
										child = child.getContainer();
										depth--;
									}
									if (child.referent == r)
										break;
									child = child.getNext();
								}
							}
							if (destroy && mapIn)
								Container(r).destroyContents();
						}
						if (destroy)
						{
							Item(r).leaveFastArea();
							if (mapIn == 0)
							{
								push(r);
								popToLunch(0);
							}
							else
								Item(r).destroy();
						}
						r = next;
					}
				}
			}
			if (file)
			{
				file->writeRecord(mapIn, buffer, count * sizeof(OneItem), writeOffset, 0);
				index.size = count * sizeof(OneItem) + writeOffset;
				file->setIndex(mapIn, index);
			}
			theScratchMem->checkIn((char *)buffer);
		}
		else if (destroy)
		{
			int16_t ry;
			int16_t rx;
			Boolean fixed;

			for (ry = 0; ry < REGIONS; ry++)
			{
				for (rx = 0; rx < REGIONS; rx++)
				{
					r = regionStartHash[rx][ry];
					while (r)
					{
						fixed = GlobalTypes.typeFlags[typeArray[r]].fixed;
						if (pass == 0 && !fixed || pass == 1 && fixed)
						{
							r = nextArray[r];
							continue;
						}
						next = nextArray[r];
						Item(r).leaveFastArea();
						if (Item(r).isNpc())
						{
							push(r);
							popToLunch(0);
							r = next;
							continue;
						}
						if (Item(r).getContents())
							Container(r).destroyContents();
						Item(r).destroy();
						r = next;
					}
				}
			}
		}
	}
	avatar.f_3b = cantSave;
}

Scheduler::Scheduler(uint16_t t)
{
	Kernel::setIdString(pid, "Scheduler");
	npcNum = 1;
	time = t;
}

// Runs one NPC's schedule each time it is called.
void Scheduler::process(void)
{
	if (npcNum == 256)
		pop(0);
	Npc npc(npcNum);
	if (npc.getZ() < Z_GONE)
	{
		uint16_t pid = npc.schedule(time);
		if (pid)
		{
			Process *process = Kernel::getProcess(pid);
			process->then(this);
		}
	}
	npcNum++;
}

// Loads a map level's items: the fixed ones, then the movable ones. Contained
// items follow their container, their x giving the depth of nesting. Level 0
// holds the NPCs; a negative level only sets mapIn.

void ItemCache::loadMapLevel(int16_t level)
{
	int16_t chunk;
	int16_t remaining;
	int16_t badCount;
	int16_t nullCount;
	int16_t i;
	uint16_t depth;
	int16_t pass;
	uint16_t rx;
	uint16_t ry;
	Index index;
	OneItem *p;
	uint16_t n;
	Referent r;
	Referent container;
	Item unused;
	int32_t readOffset;
	FlexFile *file;

	if (level < 0)
	{
		mapIn = level;
		return;
	}
	if (level == 0)
	{
		if (zArray[1] != Z_FREE)
			Container(1).destroyContents();
		for (i = 2; i < MAX_NPCS; i++)
		{
			if (zArray[i] != Z_FREE)
			{
				Container(i).destroyContents();
				Item(i).destroy();
			}
			statusArray[i] = IN_NPC_LIST;
			zArray[i] = Z_FREE;
		}
	}
	OneItem *buffer = (OneItem *)theScratchMem->checkOut(4000 * sizeof(OneItem));
	if (!buffer)
		outOfMemory(__FILE__, 737);	// __LINE__
	for (pass = 0; pass < 2; pass++)
	{
		if (pass == 0)
			file = fixedItemFile;
		else
			file = itemFile;
		file->getIndex(level, index);
		chunk = remaining = index.size / sizeof(OneItem);
		if (chunk > freeCount)
			halt(__FILE__, 779);	// __LINE__
		container = r = 0;
		depth = 0;
		readOffset = 0;
		badCount = nullCount = 0;
		while (remaining)
		{
			p = buffer;
			chunk = remaining > 4000 ? 4000 : remaining;
			remaining -= chunk;
			file->readRecord(level, buffer, chunk * sizeof(OneItem), readOffset);
			readOffset += chunk * sizeof(OneItem);
			for (i = 0; i < chunk; i++)
			{
				if (p->type == 0)
				{
					nullCount++;
					p++;
					continue;
				}
				if ((int16_t)p->x < 0 || (int16_t)p->y < 0 || p->z >= Z_GONE || p->type >= NUM_TYPES ||
					p->npcNum == 0xff && level)
				{
					badCount++;
					p++;
					continue;
				}
				int16_t family = GlobalTypes.typeFlags[p->type].family;
				if (family == CONTAINER_FAMILY)
					p->quality = 0;
				if (p->flags & CONTAINED)
				{
					if (p->x > depth)
					{
						container = r;
						depth = p->x;
					}
					while (p->x < depth)
					{
						container = Item(container).getContainer();
						depth--;
					}
					r = create();
					putItem(r, *p);
					Item(r).andStatus(~CONTAINER_GUMP_OPEN);
					popToEnd(container);
					if ((statusArray[r] & EQUIPPED) && Item(container).isNpc())
						Npc(container).setEquip(p->z, r);
					yArray[r] = p->y;
				}
				else if (level == 0)
				{
					depth = 0;
					r = p->npcNum;
					putItem(r, *p);
					Item(r).andStatus(~CONTAINER_GUMP_OPEN);
					push(r);
					Npc(r).clearEquip();
					pop();
				}
				else
				{
					depth = 0;
					r = firstFree;
					coordToLoadedCoord(p->x, p->y, rx, ry);
					prependToRegion(rx, ry, (char *)p, 1);
				}
				if (level && GlobalTypes.typeFlags[p->type].noisy)
					Item(r).cachein();
				p++;
			}
		}
	}
	theScratchMem->checkIn((char *)buffer);

	// Rebuild the free NPC list from the slots left empty.
	firstFreeNpc = lastFreeNpc = 0;
	freeNpcCount = 0;
	for (n = MAX_NPCS - 1; n > 0; n--)
	{
		if (zArray[n] == Z_FREE && n >= 2)
		{
			nextArray[n] = firstFreeNpc;
			firstFreeNpc = n;
			if (!freeNpcCount)
				lastFreeNpc = n;
			freeNpcCount++;
		}
		else if (mapNumArray[n] == level && level)
		{
			push(n);
			pop();
			Item(n).cachein();
		}
	}
	if (level)
		ShapePreloader::preload(level);
	mapIn = level;
	uint16_t time = gameHours();
	if (level)
		new Scheduler(time);
	Camera::init();
	if (badCount)
	{
		char message[80];
		((BaseCamera *)theCamera)->show();
		sprintf(message, "WARNING!\n%d bad item%s and %d null item%s in map %d.\nContinue at your own risk.",
			badCount, badCount == 1 ? "" : "s", nullCount, nullCount == 1 ? "" : "s", level);
		Dispatch(new MessageGump(NewGumpId(0), 0, message, GumpColorMap[5]));
	}
}

// Saves the NPCs by passing them through the NPC map, level 0.
void ItemCache::saveNpcs(void)
{
	uint16_t i;
	uint16_t level = mapIn;
	int16_t x = Camera::getX();
	int16_t y = Camera::getY();

	dumpMapLevel(1, 1);
	mapIn = 0;
	for (i = 1; i < MAX_NPCS; i++)
	{
		if (zArray[i] != Z_FREE)
		{
			push(i);
			pop();
		}
	}
	dumpMapLevel(1, 1);
	goTo(x, y, level);
}

void ItemCache::loadNpcs(void)
{
	uint16_t level = mapIn;
	int16_t x = Camera::getX();
	int16_t y = Camera::getY();

	dumpMapLevel(1, 1);
	mapIn = 0;
	loadMapLevel(0);
	dumpMapLevel(1, 1);
	goTo(x, y, level);
}

// The cache file holds the item arrays in record 0 and the list heads in record 1.

void ItemCache::save(char *dir)
{
	char name[80];
	int32_t offset;

	strcpy(name, fileSpec(0, dir, ItemCacheFileName, 0));
	if (FileExists(name))
		unlink(name);
	Index index;
	FlexFile file(name, (OpenMode)2, 2);

	file.relocate(0);
	file.getIndex(0, index);
	offset = 0;
	file.writeRecord(0, xArray, MAX_ITEMS * 2, offset, 0);
	offset += MAX_ITEMS * 2;
	file.writeRecord(0, yArray, MAX_ITEMS * 2, offset, 0);
	offset += MAX_ITEMS * 2;
	file.writeRecord(0, zArray, MAX_ITEMS, offset, 0);
	offset += MAX_ITEMS;
	file.writeRecord(0, typeArray, MAX_ITEMS * 2, offset, 0);
	offset += MAX_ITEMS * 2;
	file.writeRecord(0, frameArray, MAX_ITEMS, offset, 0);
	offset += MAX_ITEMS;
	file.writeRecord(0, statusArray, MAX_ITEMS * 2, offset, 0);
	offset += MAX_ITEMS * 2;
	file.writeRecord(0, qArray, MAX_ITEMS * 2, offset, 0);
	offset += MAX_ITEMS * 2;
	file.writeRecord(0, npcNumArray, MAX_ITEMS, offset, 0);
	offset += MAX_ITEMS;
	file.writeRecord(0, mapNumArray, MAX_ITEMS, offset, 0);
	offset += MAX_ITEMS;
	file.writeRecord(0, nextArray, MAX_ITEMS * 2, offset, 0);
	offset += MAX_ITEMS * 2;
	index.size = offset;
	file.setIndex(0, index);

	file.relocate(1);
	file.getIndex(1, index);
	offset = 0;
	file.writeRecord(1, &mapIn, sizeof(mapIn), offset, 0);
	offset += sizeof(mapIn);
	file.writeRecord(1, &regionsIn, sizeof(regionsIn), offset, 0);
	offset += sizeof(regionsIn);
	file.writeRecord(1, regionStartHash, REGIONS * REGIONS * 2, offset, 0);
	offset += REGIONS * REGIONS * 2;
	file.writeRecord(1, itemCount, REGIONS * REGIONS * 2, offset, 0);
	offset += REGIONS * REGIONS * 2;
	file.writeRecord(1, &freeCount, sizeof(freeCount), offset, 0);
	offset += sizeof(freeCount);
	file.writeRecord(1, &freeNpcCount, sizeof(freeNpcCount), offset, 0);
	offset += sizeof(freeNpcCount);
	file.writeRecord(1, &firstFree, sizeof(firstFree), offset, 0);
	offset += sizeof(firstFree);
	file.writeRecord(1, &firstFreeNpc, sizeof(firstFreeNpc), offset, 0);
	offset += sizeof(firstFreeNpc);
	file.writeRecord(1, &lastFree, sizeof(lastFree), offset, 0);
	offset += sizeof(lastFree);
	file.writeRecord(1, &lastFreeNpc, sizeof(lastFreeNpc), offset, 0);
	offset += sizeof(lastFreeNpc);
	file.writeRecord(1, &etherealTop, sizeof(etherealTop), offset, 0);
	offset += sizeof(etherealTop);
	file.writeRecord(1, &etherealCount, sizeof(etherealCount), offset, 0);
	offset += sizeof(etherealCount);
	index.size = offset;
	file.setIndex(1, index);
}

Boolean ItemCache::load(void)
{
	int32_t offset;

	if (FileExists(fileSpec(0, GamedatDir, ItemCacheFileName, 0)))
	{
		FlexFile file(fileSpec(0, GamedatDir, ItemCacheFileName, 0), (OpenMode)0, -1);

		offset = 0;
		file.readRecord(0, xArray, MAX_ITEMS * 2, offset);
		offset += MAX_ITEMS * 2;
		file.readRecord(0, yArray, MAX_ITEMS * 2, offset);
		offset += MAX_ITEMS * 2;
		file.readRecord(0, zArray, MAX_ITEMS, offset);
		offset += MAX_ITEMS;
		file.readRecord(0, typeArray, MAX_ITEMS * 2, offset);
		offset += MAX_ITEMS * 2;
		file.readRecord(0, frameArray, MAX_ITEMS, offset);
		offset += MAX_ITEMS;
		file.readRecord(0, statusArray, MAX_ITEMS * 2, offset);
		offset += MAX_ITEMS * 2;
		file.readRecord(0, qArray, MAX_ITEMS * 2, offset);
		offset += MAX_ITEMS * 2;
		file.readRecord(0, npcNumArray, MAX_ITEMS, offset);
		offset += MAX_ITEMS;
		file.readRecord(0, mapNumArray, MAX_ITEMS, offset);
		offset += MAX_ITEMS;
		file.readRecord(0, nextArray, MAX_ITEMS * 2, offset);
		offset += MAX_ITEMS * 2;
		for (int16_t i = 0; i < MAX_ITEMS; i++)
			statusArray[i] &= ~IN_DISPLAY_LIST;

		offset = 0;
		file.readRecord(1, &mapIn, sizeof(mapIn), offset);
		offset += sizeof(mapIn);
		file.readRecord(1, &regionsIn, sizeof(regionsIn), offset);
		offset += sizeof(regionsIn);
		file.readRecord(1, regionStartHash, REGIONS * REGIONS * 2, offset);
		offset += REGIONS * REGIONS * 2;
		file.readRecord(1, itemCount, REGIONS * REGIONS * 2, offset);
		offset += REGIONS * REGIONS * 2;
		file.readRecord(1, &freeCount, sizeof(freeCount), offset);
		offset += sizeof(freeCount);
		file.readRecord(1, &freeNpcCount, sizeof(freeNpcCount), offset);
		offset += sizeof(freeNpcCount);
		file.readRecord(1, &firstFree, sizeof(firstFree), offset);
		offset += sizeof(firstFree);
		file.readRecord(1, &firstFreeNpc, sizeof(firstFreeNpc), offset);
		offset += sizeof(firstFreeNpc);
		file.readRecord(1, &lastFree, sizeof(lastFree), offset);
		offset += sizeof(lastFree);
		file.readRecord(1, &lastFreeNpc, sizeof(lastFreeNpc), offset);
		offset += sizeof(lastFreeNpc);
		file.readRecord(1, &etherealTop, sizeof(etherealTop), offset);
		offset += sizeof(etherealTop);
		file.readRecord(1, &etherealCount, sizeof(etherealCount), offset);
	}
	ShapePreloader::preload(0);
	return TRUE;
}

// Moves the loaded area to (x, y) on a map level: regions that drop out
// are dumped, regions that come in are loaded. Level 0 keeps the current one.

void ItemCache::goTo(uint16_t x, uint16_t y, int16_t level)
{
	int16_t rx;
	int16_t ry;
	int16_t sum;
	int16_t diff;
	Boolean levelChanged;
	Boolean loaded = FALSE;

	if (!level)
		level = mapIn;
	levelChanged = mapIn != level;
	if (levelChanged)
	{
		Camera::init();
		if (mapIn)
			dumpMapLevel(1, 1);
		loadMapLevel(level);
	}
	RegionRectangle old = regionsIn;
	RegionRectangle needed;
	needed.calculateNeeded(x, y);
	if (!levelChanged)
	{
		for (sum = old.minSum; sum <= old.maxSum; sum++)
		{
			for (diff = old.minDiff; diff <= old.maxDiff; diff++)
			{
				if ((sum + diff) & 1)
					continue;
				rx = (uint16_t)(sum + diff) >> 1;
				ry = (uint16_t)(sum - diff) >> 1;
				if (!needed.isNeeded(rx, ry) && old.isNeeded(rx, ry))
					dumpRegion(rx, ry);
			}
		}
	}
	regionsIn = needed;
	for (sum = regionsIn.minSum; sum <= regionsIn.maxSum; sum++)
	{
		for (diff = regionsIn.minDiff; diff <= regionsIn.maxDiff; diff++)
		{
			if ((sum + diff) & 1)
				continue;
			rx = (uint16_t)(sum + diff) >> 1;
			ry = (uint16_t)(sum - diff) >> 1;
			if ((!old.isNeeded(rx, ry) || levelChanged) && needed.isNeeded(rx, ry))
			{
				loadRegion(rx, ry);
				loaded = TRUE;
			}
		}
	}
	if (loaded)
		theZbuffer.make();
}

void ItemCache::reloadMapLevel(void)
{
	uint16_t level = mapIn;
	mapIn = 0xffff;
	goTo(Camera::getX(), Camera::getY(), level);
}

void ItemCache::init(void)
{
	int16_t rx;
	int16_t i;

	theZbuffer.alloc();
	ItemData::init();
	itemFile = new MapFile;
	fixedItemFile = new FixedMapFile;
	if (!itemFile || !fixedItemFile)
		outOfMemory(__FILE__, 1188);	// __LINE__
	regionStartHash = new uint16_t[REGIONS][REGIONS];
	itemCount = new uint16_t[REGIONS][REGIONS];
	if (!regionStartHash || !itemCount)
		outOfMemory(__FILE__, 1199);	// __LINE__
	for (rx = 0; rx < REGIONS; rx++)
	{
		for (int16_t ry = 0; ry < REGIONS; ry++)
		{
			regionStartHash[rx][ry] = 0;
			itemCount[rx][ry] = 0;
		}
	}

	// Every node starts free: NPC slots 2-255 in one list, items from 256 in another.
	for (i = 0; i < MAX_ITEMS; i++)
	{
		nextArray[i] = i + 1;
		zArray[i] = Z_FREE;
	}
	nextArray[MAX_NPCS - 1] = 0;
	nextArray[MAX_ITEMS - 1] = 0;
	firstFreeNpc = 2;
	lastFreeNpc = MAX_NPCS - 1;
	freeNpcCount = MAX_NPCS - 1;
	firstFree = MAX_NPCS;
	lastFree = MAX_ITEMS - 1;
	nextArray[0] = 0;
	freeCount = MAX_ITEMS - MAX_NPCS;
	etherealTop = 0;
	etherealCount = 0;
}

ItemCache::~ItemCache(void)
{
	if (itemFile)
	{
		delete itemFile;
		itemFile = 0;
	}
	if (fixedItemFile)
	{
		delete fixedItemFile;
		fixedItemFile = 0;
	}
	if (regionStartHash)
		delete regionStartHash;
	regionStartHash = 0;
	if (itemCount)
		delete itemCount;
	itemCount = 0;
}

// The item before r in whichever list holds it, or 0.
int16_t ItemCache::findPrevious(Referent r)
{
	Referent node;
	Referent prev;
	uint16_t rx;
	uint16_t ry;

	if (zArray[r] == Z_FREE)
	{
		if (r < MAX_NPCS)
			node = firstFreeNpc;
		else
			node = firstFree;
	}
	else if (statusArray[r] & ETHEREAL)
		node = etherealTop;
	else if (statusArray[r] & CONTAINED)
		node = qArray[xArray[r]];
	else
	{
		coordToLoadedCoord(xArray[r], yArray[r], rx, ry);
		node = regionStartHash[rx][ry];
	}
	prev = 0;
	while (node != r && node)
	{
		prev = node;
		node = nextArray[node];
	}
	return prev;
}

void ItemCache::unlinkFromContainer(Referent r)
{
	Referent prev = findPrevious(r);
	Referent next = nextArray[r];
	if (prev)
	{
		nextArray[prev] = next;
		return;
	}
	Item container(xArray[r]);
	container.setContents(next);
}

void ItemCache::unlinkFromRegion(Referent r)
{
	Referent prev = findPrevious(r);
	Referent next = nextArray[r];
	uint16_t rx;
	uint16_t ry;

	coordToLoadedCoord(xArray[r], yArray[r], rx, ry);
	if (regionStartHash[rx][ry] == r)
		regionStartHash[rx][ry] = next;
	if (prev)
		nextArray[prev] = next;
	itemCount[rx][ry]--;
}

void ItemCache::unlinkFromFree(Referent r)
{
	Referent prev = findPrevious(r);
	if (r < MAX_NPCS)
	{
		if (r == lastFreeNpc)
			lastFreeNpc = 0;
		if (prev)
			nextArray[prev] = nextArray[r];
		else
			firstFreeNpc = nextArray[r];
		freeNpcCount--;
		return;
	}
	if (r == lastFree)
		lastFree = 0;
	if (prev)
		nextArray[prev] = nextArray[r];
	else
		firstFree = nextArray[r];
	freeCount--;
}

void ItemCache::linkToFree(Referent r)
{
	if (r < MAX_NPCS)
	{
		nextArray[r] = firstFreeNpc;
		if (firstFreeNpc == lastFreeNpc)
			lastFreeNpc = r;
		firstFreeNpc = r;
		freeNpcCount++;
		return;
	}
	nextArray[r] = firstFree;
	if (firstFree == lastFree)
		lastFree = r;
	firstFree = r;
	freeCount++;
}

void ItemCache::unlinkFromVoid(Referent r)
{
	Referent prev = findPrevious(r);
	Referent next = nextArray[r];
	if (prev)
		nextArray[prev] = next;
	else
		etherealTop = next;
	etherealCount--;
}

void ItemCache::destroy(Referent r)
{
	if (r == 1)
		return;
	Item item(r);
	if (zArray[r] >= Z_GONE || !r)
		return;
	if (!(statusArray[r] & (DISPOSABLE | FAST_ONLY)) && GlobalTypes.typeFlags[typeArray[r]].unknown2)
		avatar.f_3b = 1;
	if (item.getContents())
		Container(r).removeContents();
	if (statusArray[r] & ETHEREAL)
		unlinkFromVoid(r);
	else
	{
		push(r);
		popEthereal();
	}
	linkToFree(r);
	zArray[r] = Z_FREE;
	clearAnimating(r);
	clearNew(r);
	Kernel::killProcess(r, PT_ANY, PriorityClass(0x21));
	if (statusArray[r] & HAS_GUMP)
		ItemRelativeGump::notifyDestroyed(r);
}

Boolean ItemCache::coordToLoadedCoord(uint16_t x, uint16_t y, uint16_t &rx, uint16_t &ry)
{
	uint16_t lx = x >> REGION_SHIFT;
	uint16_t ly = y >> REGION_SHIFT;
	rx = lx;
	ry = ly;
	return regionsIn.isNeeded(lx, ly);
}

// Takes an item out of the world into the ethereal list.
void ItemCache::push(Referent r)
{
	if (freeCount <= 0)
		halt(__FILE__, 1551);	// __LINE__
	if (!r)
		halt(__FILE__, 1556);	// __LINE__
	Item item(r);
	if (statusArray[r] & EQUIPPED)
	{
		Npc npc(item.getContainer());
		npc.freeEquip(r);
		item.unequip();
		statusArray[r] &= ~EQUIPPED;
	}
	if (statusArray[r] & ETHEREAL)
		return;
	if (zArray[r] == Z_FREE)
		unlinkFromFree(r);
	else if (statusArray[r] & CONTAINED)
	{
		unlinkFromContainer(r);
		if (statusArray[item.getContainer()] & HAS_GUMP)
			ItemRelativeGump::notifyMoved(item.getContainer(), (MovedState)2);
	}
	else if (statusArray[r] & IN_NPC_LIST)
		statusArray[r] &= ~IN_NPC_LIST;
	else
	{
		DL_ItemNode::before_push(r);
		unlinkFromRegion(r);
	}
	nextArray[r] = etherealTop;
	statusArray[r] |= ETHEREAL;
	etherealTop = r;
	etherealCount++;
	if (statusArray[r] & HAS_GUMP)
		ItemRelativeGump::notifyMoved(r, (MovedState)0);
}

// Puts the top ethereal item into the map at (x, y, z).
void ItemCache::pop(uint16_t x, uint16_t y, uint8_t z)
{
	uint16_t rx = x >> REGION_SHIFT;
	uint16_t ry = y >> REGION_SHIFT;
	itemCount[rx][ry]++;
	Referent r = popEthereal();
	if (!r)
		return;
	Item item(r);
	if (!coordToLoadedCoord(x, y, rx, ry))
	{
		if (statusArray[r] & FAST_AREA)
			item.leaveFastArea();
	}
	else if (!(statusArray[r] & FAST_AREA))
		item.enterFastArea();
	nextArray[r] = regionStartHash[rx][ry];
	regionStartHash[rx][ry] = r;
	xArray[r] = x;
	yArray[r] = y;
	if (z >= Z_GONE)
		z = Z_GONE - 1;
	zArray[r] = z;
	statusArray[r] &= ~(ETHEREAL | CONTAINED);
	if ((GlobalTypes.typeFlags[typeArray[r]].draw || z < Camera::roof()) &&
		(!(statusArray[r] & INVISIBLE) || (statusArray[r] & OWNED)) &&
		!theZbuffer.isHidden(r) && (statusArray[r] & FAST_AREA))
		DL_ItemNode::after_pop(r);
	if (statusArray[r] & HAS_GUMP)
		ItemRelativeGump::notifyMoved(r, (MovedState)1);
	setNew(r);
}

// Passes the low byte of x as z.
U8_INLINE void ItemCache::pop(WorldPoint &p)
{
	pop(p.x, p.y, p.x);
}

// Puts the top ethereal item into a container, or where the container is.
void ItemCache::pop(Referent container)
{
	if (GlobalTypes.typeFlags[typeArray[container]].family == CONTAINER_FAMILY &&
		!(statusArray[container] & DISPOSABLE))
	{
		Referent r = popEthereal();
		if (!r)
			return;
		Item item(r);
		if (item.isNpc())
			pop(Item(container).getLoc());
		else
		{
			nextArray[r] = Item(container).getContents();
			Item(container).setContents(r);
			xArray[r] = container;
			zArray[r] = 0;
			statusArray[r] &= ~ETHEREAL;
			statusArray[r] |= CONTAINED;
		}
		if (statusArray[r] & HAS_GUMP)
			ItemRelativeGump::notifyMoved(r, (MovedState)1);
		if (statusArray[container] & HAS_GUMP)
			ItemRelativeGump::notifyMoved(container, (MovedState)2);
	}
	else
		pop(Item(container).getLoc());
}

// Puts the top ethereal item back where it was.
void ItemCache::pop(void)
{
	Referent r = etherealTop;
	Item item(r);
	if (statusArray[r] & CONTAINED)
	{
		uint16_t y = yArray[r];
		pop(item.getContainer());
		yArray[r] = y;
		if (statusArray[r] & HAS_GUMP)
			ItemRelativeGump::notifyMoved(r, (MovedState)1);
		return;
	}
	pop(xArray[r], yArray[r], zArray[r]);
}

// Puts the top ethereal item at the end of a container's contents.
void ItemCache::popToEnd(Referent container)
{
	if (GlobalTypes.typeFlags[typeArray[container]].family == CONTAINER_FAMILY)
	{
		Referent r = popEthereal();
		if (!r)
			return;
		Referent last = 0;
		Referent next;
		for (next = Item(container).getContents(); next; next = nextArray[next])
			last = next;
		Item item(r);
		if (last)
			nextArray[last] = r;
		else
			Item(container).setContents(r);
		nextArray[r] = 0;
		xArray[r] = container;
		zArray[r] = 0;
		statusArray[r] &= ~ETHEREAL;
		statusArray[r] |= CONTAINED;
	}
	else
		pop(Item(container).getLoc());
}

// Puts the top ethereal item, an NPC, into the NPC list.
void ItemCache::popToLunch(uint8_t clearMap)
{
	Referent r = popEthereal();
	if (!r)
		return;
	statusArray[r] &= ~ETHEREAL;
	statusArray[r] |= IN_NPC_LIST;
	if (clearMap)
		mapNumArray[r] = 0;
}

int16_t ItemCache::popEthereal(void)
{
	if (etherealCount <= 0)
		return 0;
	etherealCount--;
	Referent r = etherealTop;
	etherealTop = nextArray[r];
	return r;
}

int16_t getEtherealTop(void)
{
	return ItemCache::etherealTop;
}

// A new item, ethereal until popped.
int16_t ItemCache::create(void)
{
	if (freeCount <= 0)
		halt(__FILE__, 1820);	// __LINE__
	statusArray[firstFree] = 0;
	qArray[firstFree] = 0;
	npcNumArray[firstFree] = 0;
	mapNumArray[firstFree] = 0;
	Referent r = firstFree;
	push(r);
	return r;
}

int16_t ItemCache::createNpc(void)
{
	if (!firstFreeNpc)
		return 0;
	statusArray[firstFreeNpc] = 0;
	qArray[firstFreeNpc] = 0;
	npcNumArray[firstFreeNpc] = 0;
	Referent r = firstFreeNpc;
	push(r);
	return r;
}

// A worst-case count of the nodes still free once the loaded area moves:
// the items of every region not yet expanded from globs, plus the largest
// glob load any loaded area could need.

int16_t ItemCache::countFreeNodes(void)
{
	Referent r;
	int16_t rx;
	int16_t ry;
	int16_t x;
	int16_t y;
	int16_t sum;
	int16_t diff;
	int16_t glob;
	int16_t (*globItems)[REGIONS] = new int16_t[REGIONS][REGIONS];
	int16_t free = 0x2200;

	for (ry = 0; ry < REGIONS; ry++)
	{
		for (rx = 0; rx < REGIONS; rx++)
		{
			globItems[rx][ry] = 0;
			for (r = regionStartHash[rx][ry]; r; r = nextArray[r])
			{
				if (!(statusArray[r] & FAST_ONLY))
					free -= Container(r).countItems() + 1;
				if (GlobalTypes.typeFlags[typeArray[r]].family == GLOBEGG_FAMILY)
				{
					glob = qArray[r];
					if (glob)
						globItems[rx][ry] = globExpander->countItems(glob);
				}
			}
		}
	}

	int16_t total;
	int16_t most = 0;
	for (ry = 0; ry < REGIONS; ry++)
	{
		for (rx = 0; rx < REGIONS; rx++)
		{
			total = 0;
			for (diff = -3; diff <= 3; diff++)
			{
				for (sum = -7; sum <= 7; sum++)
				{
					x = rx + ((uint16_t)(sum + diff) >> 1);
					y = ry + ((uint16_t)(sum - diff) >> 1);
					if (x >= 0 && x < REGIONS && y >= 0 && y < REGIONS)
						total += globItems[x][y];
				}
			}
			if (total > most)
				most = total;
		}
	}
	free -= most;
	delete globItems;
	return free;
}

int16_t ItemCache::countFreeNpcs(void)
{
	int16_t i;
	int16_t count = MAX_NPCS - 2;
	for (i = 2; i < MAX_NPCS; i++)
	{
		if (zArray[i] != Z_FREE)
			count--;
	}
	return count;
}

void ItemCache::verifyFailed(int16_t n)
{
	printf("Verify Failed on %d.\n", n);
	if (getch() == 'x')
		exit(-1);
}

// Prints every node of the cache: where it starts a region, which list
// ends it points at, and its position.

void ItemCache::dumpCache(void)
{
	Item item;
	char *freeFlag;
	char *lastFlag;
	char *etherFlag;
	int16_t i;

	printf("\nfreeCount = %d\n", freeCount);
	printf("                         Referent  Next X    Y    Z  \n");
	printf("                         --------  ---- ---- ---- ---\n");
	for (i = 1; i < MAX_ITEMS; i++)
	{
		char region[20];
		char spare[20];
		strcpy(region, "       ");
		strcpy(spare, "       ");
		for (int16_t rx = 0; rx < REGIONS; rx++)
		{
			for (int16_t ry = 0; ry < REGIONS; ry++)
			{
				if (regionStartHash[rx][ry] == i)
					sprintf(region, "S %02d %02d", rx, ry);
			}
		}
		if (i == firstFree)
			freeFlag = "FF";
		else
			freeFlag = "  ";
		if (i == firstFreeNpc)
			freeFlag = "FFn";
		else
			freeFlag = "  ";
		if (i == lastFree)
			lastFlag = "LF";
		else
			lastFlag = "  ";
		if (i == lastFreeNpc)
			lastFlag = "LFn";
		else
			lastFlag = "  ";
		if (i == etherealTop)
			etherFlag = "ET";
		else
			etherFlag = "  ";
		item = Item(i);
		printf("%s %s %s %s %s %4d      %4d %4d %4d", region, spare, freeFlag, lastFlag, etherFlag, i,
			item.getNext(), item.getX(), item.getY());
		if (item.getStatus() & ETHEREAL)
			printf(" ETHEREAL\n");
		else if (item.getZ() == Z_FREE)
			printf(" FREE\n");
		else
			printf(" %3d\n", item.getZ());
	}
}

void initItemCache(void)
{
	ItemCache::init();
}

void setCantSaveGameFlag(void)
{
	avatar.f_3b = 1;
}
