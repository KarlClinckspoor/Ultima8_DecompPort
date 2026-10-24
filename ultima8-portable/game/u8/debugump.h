#ifndef DEBUGUMP_H
#define DEBUGUMP_H

#include "u8port.h"

#include "cfile.h"
#include "debugger.h"
#include "textgump.h"

class Event;
class NewGump;
class NewGumpId;
class Rect;
class WindowFrame;

// A usecode source file split into lines.
class ViewFile : public BaseFile
{
public:
	uint16_t numLines;
	uint32_t lines[4000];
	char name[9];
	char *text;

	ViewFile(void);
	int16_t parse(uint32_t *, char *, uint32_t, uint32_t);
	uint8_t setFile(char *);
	void loadFile(void);
	char *getLine(uint16_t);
	uint16_t getNumLines(void) { return numLines; }
};

class SourceDebugger : public Debugger
{
public:
	void atBreakPoint(void);
	void takeControl(void);
};

class DebugScroller : public Scroller
{
public:
	int16_t cursorX;
	int16_t cursorY;
	Debugger *owner;

	DebugScroller(NewGumpId, NewGump *, Rect, Debugger *, WindowFrame *);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
	void cursorGotoOrigin(void);
	void scrollTo(int16_t);
	int16_t parseLine(char *, char *, int16_t);
	void trackCursor(void);
	uint8_t setFile(char *);
	char *getSourceFile(void) { return viewFile->name; }
	static ViewFile *viewFile;
};

class DebugGump : public WindowGump
{
public:
	WindowFrame *sourceFrame;
	DebugScroller *scroller;
	WindowFrame *inputFrame;
	WindowGump *input;

	DebugGump(NewGumpId, NewGump *, Rect, Debugger *, uint8_t);
	virtual ~DebugGump(void);
	virtual void commandPrivate(Event &);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
	void getInput(char *, char *);
	void message(char *);
	void dumpSymbol(char *, char *, char *, char *, int16_t);
	void doMenuCommand(Event &);
	void buildMenu(void);
	static uint8_t init;
	static char *watches[10];
	static uint8_t fromInterpreter;
};

void initDebugger(void);

#endif
