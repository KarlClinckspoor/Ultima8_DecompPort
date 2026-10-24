#ifndef GRAVE_H
#define GRAVE_H

#include "u8port.h"

#include "item.h"
#include "irgump.h"
#include "sbutton.h"
#include "cafont.h"
#include "crect.h"

class Event;
class NewGump;

// Shows the inscription of a gravestone or plaque, centred line by line.
class GraveGump : public ItemRelativeGump, public GumpShape
{
public:
	FontType fontType;
	char *text;
	uint16_t offset;		// start of the line on show
	int16_t lineCount;
	uint16_t textLength;
	int16_t graveShape;
	Rect f_68;
	uint8_t mouseDown;

	GraveGump(NewGump *, int16_t, FontType, Referent, char *);
	virtual ~GraveGump(void);
	virtual void commandMouseLeft(Event &);
	virtual void commandPrivate(Event &);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
	Boolean moreToCome(void);
	Boolean nextLine(void);
};

class Grave : public Item
{
public:
	uint16_t read(int16_t, char *);
};

class Plaque : public Item
{
public:
	uint16_t read(int16_t, char *);
};

#endif
