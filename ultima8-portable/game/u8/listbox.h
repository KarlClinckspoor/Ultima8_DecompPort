#ifndef LISTBOX_H
#define LISTBOX_H

#include "u8port.h"

#include "dispatch.h"

class Event;
class Rect;

class ListBoxGump : public NewGump
{
public:
	int16_t selected;
	int16_t lineHeight;
	int16_t visibleLines;
	int16_t count;			// -1 until reset() counts a null-terminated list
	uint8_t sorted;
	int16_t stride;			// -1 for an array of string pointers, else the size of one packed string
	char **list;

	ListBoxGump(uint8_t, NewGump *, Rect, char **, uint8_t, int16_t, int16_t);
	virtual void commandNoEvents(Event &);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseMovement(Event &);
	virtual void commandPrivate(Event &);
	virtual void draw(int16_t, int16_t);
	virtual void sort(void);
	int16_t getSelected(void);
	void setSelected(int16_t);
	void setList(char **, uint8_t, int16_t, int16_t);
	void reset(void);
	char *getString(int16_t);
};

#endif
