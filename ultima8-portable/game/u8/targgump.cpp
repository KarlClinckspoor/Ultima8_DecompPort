// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: TARGGUMP.C

#include "u8port.h"

#include "newgump.h"
#include "crect.h"
#include "dispatch.h"
#include "item.h"
#include "type.h"
#include "camera.h"
#include "u8point.h"
#include "targgump.h"

// Inline in the shared headers when this file was built.
U8_INLINE void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
U8_INLINE NewGumpId::NewGumpId(uint16_t i) { instance = i; f_02 = ~instance; f_04 = 0; }
U8_INLINE void NewGump::moveto(int16_t x, int16_t y) { rect.moveto(x, y); }

// A full-screen gump that waits for a click on the world.
void TargetGump::init(void)
{
	rect.set(0, 0, 319, 199);
	moveto(0, 0);
	U8MousePointer::pushMode((PointerModes)0x22, 1);
	clicks = 0;
	registerHotKeys(27, 0);
}

TargetGump::TargetGump(Item *target, NewGump *parent) :
	NewGump(NewGumpId(0), parent, 3)
{
	init();
	item = target;
	*item = 0;
	toScreen = 0;
	findItem = 1;
}

TargetGump::TargetGump(WorldPoint *where, Item *target, uint8_t *found, NewGump *parent) :
	NewGump(NewGumpId(0), parent, 3)
{
	init();
	item = target;
	*item = 0;
	toScreen = 0;
	point = where;
	flag = found;
	findItem = 0;
}

TargetGump::TargetGump(uint16_t *x, uint16_t *y, NewGump *parent) :
	NewGump(NewGumpId(0), parent, 3)
{
	init();
	toScreen = 1;
	xp = x;
	yp = y;
	findItem = 0;
}

TargetGump::~TargetGump(void)
{
	U8MousePointer::popMode();
	if (parent)
		send(Event(EVENT_PRIVATE, 0, 0, clicks, this, parent, 0));
}

void TargetGump::commandMouseLeft(Event &event)
{
	if (event.isSingle())
		clicks++;
	if (event.isRelease() && clicks)
	{
		if (findItem)
			item->findTarget(getRelX(event), getRelY(event));
		else if (toScreen)
		{
			FastItem found;
			uint8_t z;
			uint8_t hit;
			ScreenToWorldCoords(event.x, event.y, *xp, *yp, z, &found, &hit);
		}
		else
			ScreenToWorldCoords(event.x, event.y, (uint16_t &)point->x, (uint16_t &)point->y, point->z, (FastItem *)item, flag);
		killMyself();
	}
}

void TargetGump::commandKeyboard(Event &event)
{
	if (event.data == 27)
		killMyself();
}
