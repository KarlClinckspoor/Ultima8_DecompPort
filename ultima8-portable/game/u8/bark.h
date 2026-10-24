#ifndef BARK_H
#define BARK_H

#include "u8port.h"

#include "u8process.h"
#include "cafont.h"
#include "dispatch.h"
#include "irgump.h"

class BarkGump;
class BaseFile;
class Event;
class Rect;

// Debug process: outlines the screen places barks have taken.
class DorkProcess : public Process
{
public:
	virtual void process(void);
};

// Speech or text shown over an item, a page at a time, placed to keep clear of other barks.
class BarkGump : public ItemRelativeGump
{
public:
	FontType fontType;
	char *text;
	int16_t rectIndex;		// slot taken in rects, or -1
	uint16_t offset;	// start of the page shown, or -1 before the first
	uint8_t f_60;
	uint16_t screenX;		// where the item is on screen
	uint16_t screenY;
	uint16_t textLength;
	Rect itemRect;		// the item's own shape on screen

	BarkGump(NewGump *, uint16_t, char *, uint8_t, uint16_t);
	virtual ~BarkGump(void);
	virtual void commandMouseLeft(Event &);
	virtual void commandPrivate(Event &);
	virtual void draw(int16_t, int16_t);
	virtual void killMyself(void);
	virtual void move(MovedState);
	void findPlace(void);
	void removeRect(void);
	void nextPage(void);
	static void load(BaseFile *);
	static void save(BaseFile *);
	static int16_t numBarks;
	static Rect rects[30];
};

// One keyword of an InputGump.
class InputElementGump : public NewGump
{
public:
	char *text;

	InputElementGump(NewGump *, char *, Rect *);
	virtual ~InputElementGump(void);
	virtual void commandMouseLeft(Event &);
	virtual void draw(int16_t, int16_t);
};

// Offers the Avatar the keywords of a usecode list to choose from.
class InputGump : public BarkGump
{
public:
	Rect f_6f;

	InputGump(uint16_t, uint16_t);
	virtual void commandMouseLeft(Event &) {}
	virtual void commandPrivate(Event &);
	virtual void draw(int16_t, int16_t);
	virtual void killMyself(void);
};

void BarkTest(void);
uint16_t ask(uint16_t, uint16_t);
void f6key(void);

extern char StringHeader[3];
extern Process *barkTest;

#endif
