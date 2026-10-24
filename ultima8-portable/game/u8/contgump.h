#ifndef CONTGUMP_H
#define CONTGUMP_H

#include "u8port.h"

#include "newgump.h"
#include "dispatch.h"
#include "crect.h"
#include "contain.h"
#include "sbutton.h"
#include "irgump.h"

class AvatarStatusGump;
class BaseFile;
class EditorItem;

class ContainerGumpBox : public NewGump, public Container
{
public:
	uint8_t dragging;
	Item dragItem;
	Rect backpack;			// where the paperdoll shows the backpack
	Referent pressed;		// item under a held button, dragged if the mouse moves
	Event event;			// the press that started it

	ContainerGumpBox(NewGump *, Rect, uint16_t);
	virtual void commandNoEvents(Event &);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseMovement(Event &);
	virtual void commandBroadCast(Event &);
	virtual void commandMouseEscape(Event &);
	virtual void commandMouseBoth(Event &);
	virtual void draw(int16_t, int16_t);
	void drag(uint16_t);
	Boolean findEquipSlot(uint16_t);
	int16_t findTarget(int16_t, int16_t);
	static uint8_t targetModeHack;
};

class ContainerGump : public ItemRelativeGump, public Container, public GumpShape
{
public:
	Point dragOffset;
	Point dragStart;
	uint8_t pad_64[2];
	int16_t index;				// this gump's entry in rects
	Rect itemRect;
	Item containerItem;

	ContainerGump(NewGump *, uint16_t, int16_t);
	virtual ~ContainerGump(void);
	virtual void commandMouseLeft(Event &);
	virtual void commandPrivate(Event &);
	virtual void commandMouseBoth(Event &);
	virtual void draw(int16_t, int16_t);
	virtual uint8_t collide(Event &);
	static uint8_t findTarget(int16_t, int16_t, uint16_t &);
	void positionGump(int16_t, int16_t, int16_t);
	static void load(BaseFile *);
	static void save(BaseFile *);
	static int16_t gumpCount;
	static Rect rects[16];	// the open gumps' screen rectangles
};

void closeAllGumps(void);
void ctrlF6Key(void);

extern AvatarStatusGump *theStatGump;
extern char *cheatmsg[7];
extern uint16_t cheats1[7];
extern uint32_t cheats2[7];
extern uint32_t cheats3[7];
extern char *statsText[21];
extern uint8_t containerGameCheat[1];
extern char contgumpNotice[32];
extern char dummyPtr1[16];
extern char dummyPtr2[16];
extern char dummyPtr3[16];
extern char *txtref;
extern char *txtrect;
extern char *txtshape;
extern char *txtframe;
// The editor item lives in raw storage; its constructor never runs.
extern uint8_t theEditorItem[32];
#define editorItem (*(EditorItem *)theEditorItem)

#endif
