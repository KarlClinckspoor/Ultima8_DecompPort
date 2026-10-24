#ifndef MOTRAK_H
#define MOTRAK_H

#include "u8port.h"

#include "item.h"
#include "type.h"

// Mover flags for isClear and tryMove.
#define MOVE_ON_LAND		0x01	// may stand on land
#define MOVE_ON_SEA			0x02	// may stand on water
#define MOVE_NO_SUPPORT		0x04	// needs nothing underneath
#define MOVE_STEP_DOWN		0x08	// may drop up to 9 onto a floor
#define MOVE_STEP_UP		0x10	// may climb a ledge up to 9

// Directions a move was cut short in.
#define BLOCKED_X			0x01
#define BLOCKED_Y			0x02
#define BLOCKED_Z			0x04

// One item hit along a move, and how far the mover got before touching it.
class CollisionNode
{
public:
	Referent item;
	int16_t dx;
	int16_t dy;
	int16_t dz;
	CollisionNode *next;
};

// Scratch state shared by isClear and check while sweeping one move.
class MotionTrackerInternalFlags
{
public:
	int16_t dx;
	int16_t dy;
	int16_t dz;
	WorldPoint from;
	WorldPoint to;
	Boolean needSupport;
	Boolean badSurface;
	Boolean supported;
	Boolean damaging;
	Boolean clear;
};

class MotionTracker
{
public:
	uint16_t volumeMask;		// collision flags that block the mover
	uint16_t surfaceMask;		// surfaces the mover cannot stand on
	int16_t xd;
	int16_t yd;
	int16_t zd;
	uint8_t pad_0a[6];
	int16_t hitDx;				// how far the mover gets before the first block
	int16_t hitDy;
	int16_t hitDz;
	int16_t dx;					// the whole move asked for
	int16_t dy;
	int16_t dz;
	int16_t floorGap;				// drop to the nearest floor below
	WorldPoint point;			// where the mover ends up
	uint16_t blocked;
	Referent item;
	Referent hitItem;
	CollisionNode *collisions;
	Referent target;
	Boolean targetFirst;
	Boolean targetHit;
	Boolean offMap;

	MotionTracker(void);
	~MotionTracker(void);
	void init(void);
	void initCollisionList(void);
	uint8_t can_create(uint16_t, const WorldPoint &, uint8_t);
	void collision(uint16_t, int16_t, int16_t, int16_t, uint8_t);
	uint8_t isClear(uint16_t, const WorldPoint &, int16_t, int16_t, int16_t, uint8_t, uint16_t, int16_t, uint16_t);
	uint8_t isClear(uint16_t type, const WorldPoint &from, const WorldPoint &to, uint8_t flags, uint16_t mover, int16_t volume, uint16_t goal)
	{
		return isClear(type, from, to.x, to.y, to.z, flags, mover, volume, goal);
	}
	uint8_t tryMove(uint16_t, int16_t, int16_t, int16_t, uint8_t, uint16_t);
	uint8_t tryMove(uint16_t, const WorldPoint &, const WorldPoint &, uint8_t, uint16_t, uint16_t);
	uint8_t tryMove(uint16_t mover, const WorldPoint &to, uint8_t flags, uint16_t goal)
	{
		return tryMove(mover, to.x, to.y, to.z, flags, goal);
	}
	void makeItSo(void);
	void check(uint16_t, MotionTrackerInternalFlags &);
};

// A tracker that can also step up onto ledges up to 9 high.
class SuperMotionTracker : public MotionTracker
{
public:
	int16_t stepZ;

	uint8_t tryMove(uint16_t, int16_t, int16_t, int16_t, uint8_t, uint16_t);
	uint8_t tryMove(uint16_t, const WorldPoint &, const WorldPoint &, uint8_t, uint16_t, uint16_t);
	uint8_t tryMove(uint16_t mover, const WorldPoint &to, uint8_t flags, uint16_t goal)
	{
		return tryMove(mover, to.x, to.y, to.z, flags, goal);
	}
};

char canExistAt(uint16_t, uint16_t, uint16_t, uint16_t, uint8_t, uint16_t, int16_t);

#endif
