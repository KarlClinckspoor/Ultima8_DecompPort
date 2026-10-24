#ifndef MAINMENU_H
#define MAINMENU_H

#include "u8port.h"

#include "dispatch.h"
#include "crect.h"
#include "sbutton.h"

class Event;

// The main menu: intro, load, save, options, credits, quit, quotes and end game.
class MainMenu : public NewGump, public GumpShape
{
public:
	int16_t saving;
	uint8_t endGameSeen;
	uint8_t quotesSeen;
	uint8_t resume;		// go back to the game when closed
	int16_t mode;					// 0 menu, 1 and 2 save pages, 3 options
	uint16_t f_3f;
	int16_t slot;
	int16_t highlight;
	int16_t named;					// the avatar has a name
	char saveNames[12][104];	// four lines of 26 per save

	MainMenu(NewGump *, uint8_t);
	virtual ~MainMenu(void);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseRight(Event &);
	virtual void commandMouseMovement(Event &);
	virtual void commandPrivate(Event &);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
	void setOptions(void);
	void doMenuCommand(int16_t);
	static GumpShape menuOptions[8];
	static GumpShape pagan;
	static GumpShape pagebutton;
	static GumpShape nums;
	static GumpShape entry;
	static Rect slots[6];
	static Rect options[8];
};

class EnterName : public NewGump
{
public:
	int16_t x;
	int16_t y;
	int16_t len;
	char *text;

	EnterName(NewGump *, char *);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
};

int32_t DispatchMainMenu(void);

extern char *HeaderFileName;
extern char *optionNames[27];
extern char *langs[3];
extern char *beginningText[3];
extern char *enterNameText[3];

#endif
