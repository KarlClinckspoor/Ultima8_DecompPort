#ifndef U8YESNO_H
#define U8YESNO_H

#include "u8port.h"

#include "basecam.h"
#include "sbutton.h"

class Event;
class NewGump;
class NewGumpId;

class U8YesNoGump : public PanelGump, public GumpShape
{
public:
	uint8_t drawn;

	U8YesNoGump(NewGumpId, NewGump *, char *);
	virtual void commandMouseLeft(Event &) {}
	virtual void commandPrivate(Event &);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
};

extern char *mouseBark[6];

#endif
