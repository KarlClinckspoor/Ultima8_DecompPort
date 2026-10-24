#ifndef MENU_H
#define MENU_H

#include "u8port.h"

#include "dispatch.h"

class Event;
class MenuBarGump;
class MenuGump;
class MenuItemGump;
class MenuLineGump;
class NewGumpId;
class PullDownGump;
class PureMenuItemGump;

// One entry of a menu bar or pull-down. A pull-down attached to an item is its only kid.
class PureMenuItemGump : public NewGump
{
public:
	char *text;
	uint16_t f_38;			// right margin
	uint16_t f_3a;			// left margin
	uint8_t f_3c;		// enabled
	uint8_t f_3d;		// selected

	PureMenuItemGump(char *, int16_t, uint8_t);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseMovement(Event &);
	virtual void commandPrivate(Event &);
	virtual void draw(int16_t, int16_t) = 0;
	virtual void getDim(int16_t &, int16_t &) = 0;
	virtual void addPullDown(PullDownGump *);
	char *getText(void);
	void setText(char *);
};

class MenuItemGump : public PureMenuItemGump
{
public:
	MenuItemGump(char *, int16_t, uint8_t);
	virtual void draw(int16_t, int16_t);
	virtual void getDim(int16_t &, int16_t &);
};

class CheckMenuItemGump : public MenuItemGump
{
public:
	uint8_t *checked;

	virtual void commandMouseLeft(Event &);
	virtual void draw(int16_t, int16_t);
};

// Takes the text of the item picked from its pull-down.
class SelectMenuItemGump : public MenuItemGump
{
public:
	virtual void commandPrivate(Event &);
};

class MenuLineGump : public PureMenuItemGump
{
public:
	virtual void draw(int16_t, int16_t);
	virtual void getDim(int16_t &, int16_t &);
};

class MenuGump : public NewGump
{
public:
	int16_t rightMargin;
	int16_t leftMargin;
	uint8_t color;

	MenuGump(NewGump *, uint8_t);
	virtual void commandPrivate(Event &) = 0;
	virtual void draw(int16_t, int16_t);
	virtual void clearSelect(void);
	virtual void setMovement(uint8_t);
	virtual void addMenuItem(PureMenuItemGump *) = 0;
	virtual void restore(void);
	NewGump *findMenuItem(NewGumpId);
	void setColor(uint8_t);
	char getColor(void);
};

class PullDownGump : public MenuGump
{
public:
	PullDownGump(void);
	virtual void commandPrivate(Event &);
	virtual void addMenuItem(PureMenuItemGump *);
};

class MenuBarGump : public MenuGump
{
public:
	MenuBarGump(uint8_t, NewGump *);
	virtual void commandPrivate(Event &);
	virtual void addMenuItem(PureMenuItemGump *);
};

MenuItemGump &operator+(MenuItemGump &, PullDownGump &);
PullDownGump &operator+(MenuItemGump &, MenuItemGump &);
PullDownGump &operator+(PullDownGump &, MenuItemGump &);
MenuBarGump &operator+(MenuBarGump &, MenuItemGump &);
PullDownGump &operator+(PullDownGump &, MenuLineGump &);
PullDownGump &operator+(MenuItemGump &, MenuLineGump &);

extern uint8_t defaultGumpColorMap[32];
extern uint8_t *GumpColorMap;

#endif
