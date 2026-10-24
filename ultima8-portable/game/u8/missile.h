#ifndef MISSILE_H
#define MISSILE_H

#include "u8port.h"

class WorldPoint;

// Works out how to throw an item from one point to another.
class MissileTracker
{
public:
	int16_t gravity;
	int16_t range;
	int16_t maxRange;
	uint16_t speedX;
	uint16_t speedY;
	int16_t speedZ;
	uint8_t inRange;

	MissileTracker(WorldPoint, WorldPoint, int16_t, int16_t);
	void fire(uint16_t);
	uint8_t isPathClear(uint16_t, WorldPoint);
};

#endif
