#ifndef WORLD_H
#define WORLD_H

#include "u8port.h"

#include "u8process.h"
#include "dispatch.h"
#include "hardware.h"
#include "textgump.h"
#include "item.h"
#include "type.h"

class Event;
class InputWindowGump;
class Item;
class TargetGump;

class InputWindowGump : public WindowGump
{
public:
	InputWindowGump(NewGumpId id, Rect r, uint8_t shown) : WindowGump(id, 0, r, 0x1e, shown, 0) {}
	uint8_t getInt(int16_t &);
	uint8_t getString(char *);
	uint8_t getChar(char &);
};

class CheatWindow : public InputWindowGump
{
public:
	Item f_3e;
	uint8_t f_40;
	WorldPoint where;

	CheatWindow(int32_t);
	virtual void commandPrivate(Event &);
	virtual void commandKeyboard(Event &);
	virtual void draw(int16_t, int16_t);
	int16_t resetMenu(void);
	void buildMenu(void);
	void displayItemStats(void);
};

class TestScreen : public InputWindowGump
{
public:
	int16_t language;
	HardwareChecker hardwareChecker;

	TestScreen(void);
	virtual void commandPrivate(Event &);
	virtual void draw(int16_t, int16_t);
};

class BogusProcess : public Process
{
public:
	virtual void process(void);
};

class World : public NewGump
{
public:
	Item f_34;
	Point f_36;
	uint8_t f_3a;
	uint8_t pad_3b[4];
	uint16_t f_3f;
	uint16_t f_41;
	uint8_t f_43;
	WorldPoint dragStart;
	WorldPoint dragLast;
	int16_t f_4e;
	uint32_t savedF1d;
	TargetGump *targetGump;
	BogusProcess *bogus;
	Item *target;
	Referent f_60;
	uint16_t flags;

	World(void);
	virtual void commandNoEvents(Event &);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseRight(Event &);
	virtual void commandMouseMovement(Event &);
	virtual void commandPrivate(Event &);
	virtual void commandKeyboard(Event &);
	virtual void commandMouseBoth(Event &);
	void shutdown(void);
	void saveAndExit(void);
	void prePreDragItem(Event &);
	void preDragItem(Event &);
	void startDragging(Event &);
	void postDragItem(Event &);
	void dragItem(Event &);
	int16_t getTarget(void);
	int16_t getTargetCoords(void);
};

void targetCoords(void);
int16_t target(void);
void drawDiamond(uint16_t, uint16_t, uint8_t, int16_t, int16_t, int16_t);
void drawShadowRect(Item);
void drawFootpadExtents(Item);

extern char *mouseBark[6];
extern char *quit[3];
extern char *toDos[3];
extern char *toPagan[3];
extern char *test[3];
extern char *testMusic[3];
extern char *testSFX[3];
extern char *testFiles[3];
extern char *view[3];
extern char *playing[3];
extern char *file[3];
extern char *passed[3];
extern char *failed[3];
extern char *pressAnyKey[3];
extern char *whatDrive[3];
extern char *invalid[3];
extern char *doesNotExist[3];
extern char *endOfFile[3];
extern char *hardwareStatus[3];
extern char *memory[3];
extern char *bytes[3];
extern char *available[3];
extern char *largestFragment[3];
extern char *hardDrive[3];
extern char *shapeCache[3];
extern char *music[3];
extern char *on[3];
extern char *off[3];
extern char *card[3];
extern char *port[3];
extern char *sfx[3];
extern uint8_t Line25[26];
extern char *TypeFlagDesc[14];
extern char *ItemStatDesc[16];
extern char InvalidString[8];
extern char *FamilyNames[16];
extern char *EquipNames[16];
extern char *AnimNames[16];
extern uint8_t searchBuffer[50];

#endif
