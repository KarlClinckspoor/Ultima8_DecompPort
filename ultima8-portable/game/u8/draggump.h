#ifndef DRAGGUMP_H
#define DRAGGUMP_H

#include "u8port.h"

#include "dispatch.h"

class Event;
class NewGumpId;
class Shape;

// A gump the mouse can drag around the screen.
class DrgNewGump : public NewGump
{
public:
	Shape *dragShape;	// the gump's image while it is dragged
	uint16_t dragging;
	int16_t lastX;
	int16_t lastY;

	DrgNewGump(NewGumpId, NewGump *);
	virtual ~DrgNewGump(void);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseMovement(Event &);
	virtual void draw(int16_t, int16_t) {}
};

#endif
