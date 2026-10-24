#ifndef MESSAGE_H
#define MESSAGE_H

#include "u8port.h"

#include "basecam.h"

class Event;
class NewGump;
class NewGumpId;

class MessageGump : public PanelGump
{
public:
	char *text;
	int16_t textHeight;
	int16_t textWidth;

	MessageGump(NewGumpId, NewGump *, char *, uint8_t);
	virtual void commandPrivate(Event &);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
};

#endif
