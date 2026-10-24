#ifndef IRGUMP_H
#define IRGUMP_H

#include "u8port.h"

#include "basecam.h"
#include "kernel.h"
#include "wait.h"
#include "item.h"
#include "type.h"

class Event;
class NewGump;
class Rect;

enum IRGumpType : int16_t
{
	IRGumpType_0
};

// What the gump follows: 0 and 1 its item, 2 the avatar, 3 the camera.
// The name is a guess that reproduces the constructor's hashed symbol.
enum IRGumpMove : int16_t
{
	IRGumpMove_0
};

enum MovedState : int16_t
{
	MovedState_0
};

class GumpNotifier : public Wait
{
public:
	NewGumpId path[10];		// ids from the base gump down to the target
	uint16_t depth;
	uint8_t f_70;
	uint8_t active;

	GumpNotifier(uint16_t, NewGump *, uint16_t, ProcessType, uint8_t);
	virtual ~GumpNotifier(void);
	virtual void restart(void);
	virtual void timerDone(void);
};

class ItemRelativeGump : public PanelGump
{
public:
	int16_t dx;			// offset from the followed point on screen
	int16_t dy;
	WorldPoint loc;
	IRGumpMove moveType;
	uint16_t f_4e;		// pid of the GumpNotifier closing the gump
	uint16_t delay;
	uint16_t f_52;		// IRGumpType
	Referent item;

	ItemRelativeGump(IRGumpType, NewGump *, Rect, uint16_t, IRGumpMove, int16_t, int16_t, uint16_t);
	virtual ~ItemRelativeGump(void);
	virtual void commandMouseRight(Event &);
	virtual void commandMouseBoth(Event &);
	virtual void killMyself(void);
	virtual void move(MovedState);
	static void notifyMoved(uint16_t, MovedState);
	void sort(void);
	static void notifyDestroyed(uint16_t);
};

void loadGumps(void);
void saveGumps(void);

extern char *GumpSaveFile;

#endif
