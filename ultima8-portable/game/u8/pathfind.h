#ifndef PATHFIND_H
#define PATHFIND_H

#include "u8port.h"

#include "process.h"
#include "type.h"

// Walks an NPC toward a point or an item one step at a time, trying the
// direct direction first and sweeping around obstacles.
class PathFinder : public ProcessInterrupt
{
public:
	WorldPoint dest;
	char dir;
	char lastDir;
	char tryDir;
	uint8_t sweeping;
	uint8_t sweepLeft;
	uint8_t targetSolid;
	int16_t bestDist;
	int16_t closeEnough;
	uint16_t maxSteps;
	uint16_t steps;
	uint8_t testOnly;	// plan the path without animating
	uint8_t arrived;
	uint16_t target;

	PathFinder(uint16_t, uint16_t, WorldPoint, uint16_t, uint8_t);
	PathFinder(uint16_t, uint16_t, uint16_t, uint8_t);
	virtual void process(void);
	virtual void pop(int32_t);
	virtual void fail(int32_t);
	void end(void);
	char getLegalDir(WorldPoint *);
};

char canGetThere(uint16_t, uint16_t, uint16_t);
char canGetThere(uint16_t, uint16_t, WorldPoint, uint16_t);

#endif
