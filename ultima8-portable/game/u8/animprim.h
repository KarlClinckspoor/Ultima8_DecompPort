#ifndef ANIMPRIM_H
#define ANIMPRIM_H

#include "u8port.h"

#include "u8process.h"
#include "animcach.h"
#include "anode.h"
#include "mlist.h"
#include "type.h"

// The value an AnimPrimitive ends with, read back by the process it notifies.
struct AnimResult
{
	uint16_t hit : 1;			// the attack hit something
	uint16_t reachedTarget : 1;	// the move touched its target
	uint16_t done : 1;			// the animation ran to its end
	uint16_t hitTarget : 1;		// the attack hit the target
	uint16_t fell : 1;			// the move would leave the NPC unsupported
	uint16_t flags : 11;
	uint16_t high;

	AnimResult(void) {}
	AnimResult(int32_t value) { *(int32_t *)this = value; }
	AnimResult &operator=(int32_t value) { *(int32_t *)this = value; return *this; }
	void clear(void) { *(int32_t *)this = 0; }
};

// One animation of an NPC, played a frame per tick; moving ones also move the NPC.
class AnimPrimitive : public ProcessInterrupt
{
public:
	AnimSet anim;
	char dir;
	uint8_t frames;
	uint8_t started;
	uint8_t stopMoving;
	uint8_t collided;
	uint8_t blocked;
	uint8_t standAfter;
	uint8_t trySideways;
	uint8_t failNext;
	AnimResult result;
	uint16_t f_3d;
	uint16_t target;
	WorldPoint *dest;			// set when only testing a move
	uint8_t savedFirstStep;
	int16_t savedLastAnimSet;
	char savedDir;
	uint8_t savedFlipped;
	uint8_t savedFlags2F_5;
	uint8_t savedAnimFrame;

	AnimPrimitive(uint16_t, AnimSet, char, WorldPoint *, uint16_t, uint16_t);
	virtual void process(void);
	virtual void pop(int32_t);
	virtual void fail(int32_t);
	virtual void interruptHandler(uint32_t, uint32_t);
	virtual WorldPoint getPoint(WorldPoint, char, AnimNode);
	virtual void failFunction(void);
	void initialize(void);
	void figureStartingPhase(void);
	void legalStore(void);
	void legalRevert(void);
	void cleanUp(void);
	void weaponCheck(AnimStreamHeader *, WorldPoint *, AnimNode *);
	void collideWithItem(uint16_t);
	uint8_t checkGravity(WorldPoint, WorldPoint, uint16_t);
};

#endif
