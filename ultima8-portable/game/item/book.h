#ifndef BOOK_H
#define BOOK_H

#include "u8port.h"

#include "item.h"
#include "irgump.h"
#include "sbutton.h"
#include "cafont.h"
#include "crect.h"

class Event;
class NewGump;

// Shows a book's text two pages at a time, or a scroll's one page at a time.
class BookGump : public ItemRelativeGump, public GumpShape
{
public:
	FontType fontType;
	char *text;
	uint16_t offset;		// start of the page on show
	uint8_t f_62;
	uint16_t textLength;
	int16_t isScroll;
	Rect f_67;
	uint8_t mouseDown;

	BookGump(NewGump *, char, int16_t, Referent, char *);
	virtual ~BookGump(void);
	virtual void commandMouseLeft(Event &);
	virtual void commandPrivate(Event &);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
	Boolean moreToCome(void);
	Boolean nextPage(void);
};

class Book : public Item
{
public:
	uint16_t read(char *);
};

class Scroll : public Item
{
public:
	uint16_t read(char *);
};

extern Rect bookrect;
extern Rect scrollrect;

#endif
