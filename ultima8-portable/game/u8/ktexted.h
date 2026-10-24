#ifndef KTEXTED_H
#define KTEXTED_H

#include "u8port.h"

#include "dispatch.h"
#include "textedit.h"
#include "crect.h"

class Event;
class Rect;

class CacheFont;

// The save description editor.
class KillerTextEditGump : public NewGump
{
public:
	Rect f_34;
	CacheFont *font;
	char *text;
	char *backup;		// restored by Escape
	int16_t line;
	int16_t col;

	KillerTextEditGump(uint8_t, NewGump *, char *, Rect);
	virtual ~KillerTextEditGump(void);
	virtual void commandMouseLeft(Event &);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
	void clearText(void);
	void nullLine(int16_t);
	int16_t getLen(int16_t);
	void insertTo(int16_t, int16_t);
	int16_t findLastSpace(int16_t);
	int16_t findFirstSpace(int16_t);
	void output(char);
	void deleteChar(int16_t, int16_t);
	void printLine(int16_t);
	void printText(void);
	void nullToEnd(int16_t, int16_t);
};

class UnkSymbolEditor : public TextEditGump
{
public:
	UnkSymbolEditor(uint8_t, NewGump *, char *, uint8_t, uint8_t);
	virtual void commandKeyboard(Event &);
};

class InputInt : public TextEditGump
{
public:
	InputInt(uint8_t, NewGump *, char *);
	virtual void commandKeyboard(Event &);
};

extern uint8_t IDidNotTypeInAnything;

#endif
