#ifndef NEWGUMP_H
#define NEWGUMP_H

#include "u8port.h"

#include "cdlist.h"
#include "crect.h"

class NewGump;

enum EventType : int16_t
{
	EventType_0,
	EVENT_NONE,
	EVENT_PRIVATE = 0x10
};

class NewGumpList : public DoubleLinkedList
{
public:
	NewGumpList(void);
	virtual ~NewGumpList(void);
	NewGump *hasType(uint8_t);
	NewGump *getHead(void);
	void attach(NewGump *);
	void moveToTail(NewGump *);
	uint8_t detach(NewGump *);
	uint8_t remove(NewGump *);
	NewGump *operator[](int16_t);
	NewGump *hasInstance(uint8_t);
	NewGump *hasType(uint8_t, NewGump *&);
	NewGump *hasGump(uint8_t, uint8_t);
};

class Event
{
public:
	int16_t type;
	int16_t x;
	int16_t y;
	int32_t data;
	void *extra;
	uint16_t f_0e;
	NewGump *to;
	NewGump *from;

	Event(EventType = EVENT_NONE, int16_t = 0, int16_t = 0, int32_t = 0, NewGump * = 0, NewGump * = 0, void * = 0);
	uint8_t isSingle(void);
	uint8_t isDouble(void);
	uint8_t isRelease(void);
	char isShift(void);
	char isCtrl(void);
	char isAlt(void);
};

// Type in the high byte, instance in the low; f_02 holds the complement as a check.
class NewGumpId
{
public:
	uint16_t instance;
	uint16_t f_02;
	uint8_t f_04;

	inline NewGumpId(uint8_t, uint8_t);
	NewGumpId(uint16_t = 0);
	uint8_t getType(void);
	int16_t operator==(NewGumpId);
	uint8_t getInstance(void);
	int16_t get(void);
	uint8_t check(void);
};

class NewGump : public DoubleLink
{
public:
	NewGumpId newGumpId;
	NewGumpList newGumpList;
	NewGump *parent;
	uint32_t f_1d;
	Rect rect;
	int32_t visibilityStack;	// two bits per level, top in the low bits
	uint8_t f_2d;
	uint8_t f_2e;
	uint8_t f_2f;
	int16_t *hotKeys;

	NewGump(NewGumpId, NewGump *, int32_t);
	virtual ~NewGump(void);
	virtual void commandNoEvents(Event &) {}
	virtual void commandMouseLeft(Event &) {}
	virtual void commandMouseRight(Event &) {}
	virtual void commandMouseMovement(Event &) {}
	virtual void commandPrivate(Event &) {}
	virtual void commandKeyboard(Event &);
	virtual void commandBroadCast(Event &) {}
	virtual void commandMouseEscape(Event &) {}
	virtual void commandMouseBoth(Event &) {}
	virtual void commandJoystick(Event &) {}
	virtual uint8_t commandAsk(Event &) { return 0; }
	virtual void draw(int16_t, int16_t) { drew = 0; }
	virtual void command(Event &);
	virtual uint8_t collide(Event &event) { return contains(event); }
	virtual void killMyself(void);
	void moveto(int16_t, int16_t);
	void popVisibility(void);
	void attach(NewGump *);
	static void *operator new(size_t);
	static void operator delete(void *);
	uint8_t isInvisible(void);
	void makeInvisible(void);
	void refresh(int16_t, int16_t, Rect);
	void refresh(void);
	NewGump *findTargetedGump(Event);
	uint8_t caresAbout(Event &, NewGump *&, int16_t);
	uint8_t contains(Event &);
	void pushVisibility(int16_t);
	uint8_t kidnap(NewGump *, NewGump *);
	int16_t get_dx(void);
	int16_t get_dy(void);
	int16_t getRelX(Event &);
	int16_t getRelY(Event &);
	void registerHotKeys(int, ...);	// int: va_start needs a promoted type
	uint8_t searchHotKeys(Event &);
	void restore(Rect);
	void restore(void);
	void setExclusive(NewGump *);
	void clrExclusive(void);
	void send(Event &);
	// Borland let a temporary bind to Event &; callers pass them so.
	void send(Event &&e) { send(e); }
	void sendInFuture(Event &);
	void badEvent(NewGump *, Event &);
	void check(char *, int16_t);
	uint8_t brattsApprove(Event &);
	int16_t visibility(void);
	void detach(NewGump *);
	static NewGump *prevMouseMoveGump;
	static uint8_t drew;
	static int16_t gumpCount;
	static int16_t maxGumpCount;
};

// Stands in for "no gump" where a null pointer means "not decided yet".
extern NewGump nullGump;

#endif
