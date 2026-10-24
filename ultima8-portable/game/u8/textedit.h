#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include "u8port.h"

#include "dispatch.h"

class Event;

// A one-line text field; Tab and Enter pass the focus on.
class TextEditGump : public NewGump
{
public:
	uint8_t lastChar;
	uint8_t len;
	uint8_t maxLen;
	uint8_t selStart;
	uint8_t selEnd;
	uint8_t scroll;		// first character shown
	uint8_t tabNext;		// Tab moves to the next field
	uint8_t cursor;
	char *text;

	TextEditGump(uint8_t, NewGump *, char *, uint8_t, uint8_t, char *, uint8_t);
	virtual ~TextEditGump(void);
	virtual void commandNoEvents(Event &);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseMovement(Event &);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
	virtual void reset(uint8_t, char *);
	char *getData(void);
	int16_t findLoc(int16_t);
	int16_t findChar(int16_t);
	void findScrollPos(void);
	void getFocus(void);
	void loseFocus(void);
	static TextEditGump *focus;
};

#endif
