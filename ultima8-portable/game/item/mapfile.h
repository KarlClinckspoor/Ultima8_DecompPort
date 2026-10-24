#ifndef MAPFILE_H
#define MAPFILE_H

#include "u8port.h"

#include "fastflex.h"
#include "stepflex.h"

class RegionFile : public FlexFile
{
public:
	int16_t numItems(uint16_t);
};

// Movable map items, one record per map; the working copy lives in GAMEDAT.
class MapFile : public StepFlexFile
{
public:
	MapFile(void);
	void restart(void);
	void resetChecksum(void);
};

// Fixed map items, one record per map, read from STATIC.
class FixedMapFile : public FastFlex
{
public:
	FixedMapFile(void);
};

extern uint32_t mapNames;

#endif
