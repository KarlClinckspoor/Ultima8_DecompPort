#ifndef ITEMCACH_H
#define ITEMCACH_H

#include "u8port.h"

#include "u8process.h"
#include "item.h"
#include "itemdata.h"

class FixedMapFile;
class MapFile;
class WorldPoint;
class OneItem;

// The regions to keep loaded around the camera: a diamond in region
// coordinates, bounded on rx + ry and on rx - ry.
class RegionRectangle
{
public:
	int16_t minSum;
	int16_t maxSum;
	int16_t minDiff;
	int16_t maxDiff;

	RegionRectangle(void) { minSum = 0; maxSum = 0; minDiff = 0; maxDiff = 0; }
	Boolean isNeeded(int16_t, int16_t);
	void calculateNeeded(uint16_t, uint16_t);
};

class Scheduler : public Process
{
public:
	uint16_t npcNum;		// next NPC to schedule
	uint16_t time;

	Scheduler(uint16_t);
	virtual void process(void);
};

class ItemCache : public ItemData
{
public:
	~ItemCache(void);
	static void prependToRegion(uint16_t, uint16_t, char *, int16_t);
	static void loadRegion(int16_t, int16_t);
	static void dumpRegion(int16_t, int16_t);
	static void dumpMapLevel(uint8_t, uint8_t);
	static void loadMapLevel(int16_t);
	static void saveNpcs(void);
	static void loadNpcs(void);
	static void save(char *);
	static Boolean load(void);
	static void goTo(uint16_t, uint16_t, int16_t);
	static void reloadMapLevel(void);
	static void init(void);
	static int16_t findPrevious(uint16_t);
	static void unlinkFromContainer(uint16_t);
	static void unlinkFromRegion(uint16_t);
	static void unlinkFromFree(uint16_t);
	static void linkToFree(uint16_t);
	static void unlinkFromVoid(uint16_t);
	static void destroy(uint16_t);
	static Boolean coordToLoadedCoord(uint16_t, uint16_t, uint16_t &, uint16_t &);
	static void push(uint16_t);
	static void pop(uint16_t, uint16_t, uint8_t);
	static void pop(uint16_t);
	static void pop(void);
	static void pop(WorldPoint &);
	static void pop(WorldPoint &&p) { pop(p); }
	static void popToEnd(uint16_t);
	static void popToLunch(uint8_t);
	static int16_t popEthereal(void);
	static int16_t create(void);
	static int16_t createNpc(void);
	static int16_t countFreeNodes(void);
	static int16_t countFreeNpcs(void);
	static void verifyFailed(int16_t);
	static void dumpCache(void);
	static int16_t mapIn;			// map level loaded, -1 for none
	static uint16_t (*regionStartHash)[64];
	static uint16_t (*itemCount)[64];
	static MapFile *itemFile;
	static FixedMapFile *fixedItemFile;
	static uint8_t expandingGlobs;
	static RegionRectangle regionsIn;
	static uint16_t firstFree;
	static uint16_t firstFreeNpc;
	static uint16_t lastFree;
	static uint16_t lastFreeNpc;
	static int16_t freeCount;
	static uint16_t freeNpcCount;
	static uint16_t etherealTop;
	static int16_t etherealCount;
};

int16_t getEtherealTop(void);
void initItemCache(void);
void setCantSaveGameFlag(void);

extern char U7FileName[9];
extern char ItemCacheFileName[13];

#endif
