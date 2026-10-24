#ifndef TEXTGUMP_H
#define TEXTGUMP_H

#include "u8port.h"

#include "dispatch.h"
#include "menu.h"

class Event;
class NewGumpId;
class PullDownGump;
class Rect;
class TextMenuBarGump;
class TextMenuGump;
class TextMenuItemGump;
class TextMenuLineGump;
class TextPullDownGump;
class WindowFrame;
class WindowGump;

class WindowGump : public NewGump
{
public:
	int16_t cursorX;
	int16_t cursorY;
	char color;
	WindowFrame *frame;
	uint8_t shown;	// text mode switched on

	WindowGump(NewGumpId, NewGump *, Rect, char, uint8_t, WindowFrame *);
	virtual void draw(int16_t, int16_t);
	void clear(void);
	void gotoxy(int16_t, int16_t);
	void print(char *, ...);
	void print(char);
	void deactivate(void);
	void activate(void);
	void moveCursor(int16_t, int16_t);
	void setColor(char);
};

class WindowFrame : public WindowGump
{
public:
	char *frameChars;		// corners, then horizontal and vertical edges

	WindowFrame(NewGumpId, NewGump *, Rect, char *, char);
	virtual void draw(int16_t, int16_t);
	void setFrameChars(char *chars) { frameChars = chars; }
};

class Scroller : public WindowGump
{
public:
	int16_t scrollX;
	int16_t scrollY;
	int16_t maxX;
	int16_t maxY;

	Scroller(NewGumpId, NewGump *, Rect, char, WindowFrame *);
	int16_t isVis(int16_t, int16_t);
	virtual void draw(int16_t, int16_t) = 0;
};

class TextMenuLineGump : public PureMenuItemGump
{
public:
	TextMenuLineGump(void);
	virtual void draw(int16_t, int16_t);
	virtual void getDim(int16_t &, int16_t &);
};

class TextMenuItemGump : public PureMenuItemGump
{
public:
	TextMenuItemGump(char *, int16_t, uint8_t);
	virtual void draw(int16_t, int16_t);
	virtual void getDim(int16_t &, int16_t &);
};

class TextCheckMenuItemGump : public TextMenuItemGump
{
public:
	uint8_t *checked;

	TextCheckMenuItemGump(void) : TextMenuItemGump(0, 0, 0) {}
	TextCheckMenuItemGump(char *text, int16_t id, uint8_t enabled, uint8_t *flag) : TextMenuItemGump(text, id, enabled) { checked = flag; }
	virtual void commandMouseLeft(Event &);
	virtual void draw(int16_t, int16_t);
	virtual void addPullDown(PullDownGump *, uint8_t);
};

class TextMenuGump : public MenuGump
{
public:
	TextMenuGump(NewGump *parent) : MenuGump(parent, GumpColorMap[5]) {}
};

class TextPullDownGump : public TextMenuGump
{
public:
	TextPullDownGump(void);
	virtual void commandPrivate(Event &);
	virtual void draw(int16_t, int16_t);
	virtual void addMenuItem(PureMenuItemGump *);
};

class TextMenuBarGump : public TextMenuGump
{
public:
	TextMenuBarGump(uint8_t, NewGump *);
	virtual void commandPrivate(Event &);
	virtual void draw(int16_t, int16_t);
	virtual void addMenuItem(PureMenuItemGump *);
};

TextMenuItemGump &operator+(TextMenuItemGump &, TextPullDownGump &);
TextPullDownGump &operator+(TextMenuItemGump &, TextMenuItemGump &);
TextPullDownGump &operator+(TextPullDownGump &, TextMenuItemGump &);
TextMenuBarGump &operator+(TextMenuBarGump &, TextMenuItemGump &);
TextPullDownGump &operator+(TextPullDownGump &, TextMenuLineGump &);
TextPullDownGump &operator+(TextMenuItemGump &, TextMenuLineGump &);

extern char *doubleFrame;
extern char *singleFrame;

#endif
