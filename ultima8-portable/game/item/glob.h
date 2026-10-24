#ifndef GLOB_H
#define GLOB_H

#include "u8port.h"

#include "flex.h"
#include "handler.h"
#include "type.h"

class Item;

#define MAX_GLOBS_IN	512

class GlobFile : public FlexFile
{
public:
	GlobFile(void);
};

// An item as stored in a glob or the map file.
class OneItem : public WorldPoint
{
public:
	uint16_t type;
	uint8_t frame;
	uint16_t flags;
	uint16_t quality;
	uint8_t npcNum;
	uint8_t mapNum;
	uint16_t next_object;
};

// Loads globs (prefabricated groups of items) into the cache and turns
// them into real items in the map.
class GlobExpander : public CacheHandler
{
public:
	GlobExpander(void);
	~GlobExpander(void);
	virtual void cacheOut(uint32_t, uint16_t);
	int32_t getSizeFromDisk(uint16_t);
	void *get(uint16_t);
	void expand(Item, uint8_t);
	int16_t expand(uint16_t, uint16_t, uint16_t, uint8_t, uint8_t);
	void preload(int16_t);
	void flushGlob(uint16_t);
	int16_t countItems(int16_t);
	static GlobFile *globFile;
	static uint16_t startHandle;
	static uint16_t globsIn[MAX_GLOBS_IN];	// glob loaded in each handle
};

void initGlobExpander(void);
void uninitGlobExpander(void);

extern GlobExpander *globExpander;

#endif
