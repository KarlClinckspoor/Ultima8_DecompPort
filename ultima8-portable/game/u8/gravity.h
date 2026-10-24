#ifndef GRAVITY_H
#define GRAVITY_H

#include "u8port.h"

#include "u8process.h"
#include "animprim.h"
#include "item.h"
#include "type.h"

// ItemSupport::flags: which corners of the footprint rest on something.
#define SUPPORT_EAST_NORTH	0x01
#define SUPPORT_EAST_SOUTH	0x02
#define SUPPORT_WEST_SOUTH	0x04
#define SUPPORT_WEST_NORTH	0x08
#define SUPPORTED			0x10	// two opposite corners: the item stands

// What an item standing at a point would rest on.
class ItemSupport
{
public:
	uint8_t flags;

	ItemSupport(void) {}
	ItemSupport(uint16_t, uint8_t);
	void figureInfo(uint16_t, WorldPoint &, uint8_t, uint16_t);
	void figureInfo(uint16_t a, WorldPoint &&p, uint8_t b, uint16_t c) { figureInfo(a, p, b, c); }
	uint8_t isSupported(void) { return flags & SUPPORTED; }
	Boolean onlyNorth(void) { return (flags & 9) && !(flags & 6); }
	Boolean onlyEast(void) { return (flags & 3) && !(flags & 0xc); }
	Boolean onlySouth(void) { return (flags & 6) && !(flags & 9); }
	Boolean onlyWest(void) { return (flags & 0xc) && !(flags & 3); }
};

// Moves a thrown or falling item each tick until it comes to rest.
class GravityTracker : public Process
{
public:
	int16_t vx;
	int16_t vy;
	int16_t vz;
	int16_t gravity;
	uint8_t sliding;
	int16_t lifetime;			// ticks left, or 0 for no limit
	uint8_t grabbed;	// caught hold of a ledge
	WorldPoint *dest;		// set when only testing the throw

	GravityTracker(uint16_t, int16_t, int16_t, int16_t, int16_t, int16_t, WorldPoint *);
	virtual void process(void);
	virtual ~GravityTracker(void);
	virtual uint8_t move(void) { return 0; }
	virtual void afterMove(uint16_t) {}
	virtual uint8_t impact(void);
	void bump(int16_t, int16_t, int16_t);
	void cancel(void);
};

// The Avatar's fall: holding the right mouse button grabs a ledge in reach.
class GravityTrackerCling : public GravityTracker
{
public:
	uint16_t hitItem;

	GravityTrackerCling(uint16_t npc, int16_t vx, int16_t vy, int16_t vz, int16_t gravity) :
		GravityTracker(npc, vx, vy, vz, gravity, 200, 0) { hitItem = 0; }
	virtual uint8_t move(void);
	virtual void afterMove(uint16_t);
	void setHitItem(uint16_t item) { hitItem = item; }
};

// Climbs onto something 16 to 72 high; height counts in steps of 8 from 1.
class Climb : public AnimPrimitive
{
public:
	Climb(uint16_t npc, int16_t height, char dir, WorldPoint *dest = 0) :
		AnimPrimitive(npc, (AnimSet)(height + 17), dir, dest, 0, 0) {}
};

// Waits on another process, which then leaves its result here.
class ResultProcess : public Process
{
public:
	virtual void process(void) { pop(0); }
};

#endif
