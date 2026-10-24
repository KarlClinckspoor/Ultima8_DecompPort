#ifndef TARGGUMP_H
#define TARGGUMP_H

#include "u8port.h"

#include "dispatch.h"

class Event;
class Item;
class WorldPoint;

class TargetGump : public NewGump
{
public:
	Item *item;
	uint8_t clicks;
	uint8_t findItem;		// ask the item to find what was clicked
	uint8_t *flag;
	uint8_t toScreen;		// just report the world x and y
	uint16_t *xp;
	uint16_t *yp;
	WorldPoint *point;

	TargetGump(Item *, NewGump *);
	TargetGump(WorldPoint *, Item *, uint8_t *, NewGump *);
	TargetGump(uint16_t *, uint16_t *, NewGump *);
	virtual ~TargetGump(void);
	virtual void commandMouseLeft(Event &);
	virtual void commandKeyboard(Event &);
	void init(void);
};

extern uint8_t U8GumpColorMap[32];

#endif
