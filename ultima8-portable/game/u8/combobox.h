#ifndef COMBOBOX_H
#define COMBOBOX_H

#include "u8port.h"

#include "dispatch.h"

class Event;
class ListBoxGump;
class TextEditGump;
class Rect;

class ComboBox : public NewGump
{
public:
	TextEditGump *textEdit;
	ListBoxGump *listBox;
	char *text;
	char *lastText;		// the text before editing, restored when no entry matches
	char **list;

	ComboBox(uint8_t, NewGump *, Rect, int16_t);
	virtual ~ComboBox(void);
	virtual void commandPrivate(Event &);
	virtual void init(void) = 0;
	virtual void unInit(void) = 0;
	char *getSelected(void);
};

#endif
