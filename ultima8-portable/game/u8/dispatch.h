#ifndef DISPATCH_H
#define DISPATCH_H

#include "u8port.h"

#include "newgump.h"
#include "cmpoint.h"
#include "cmselect.h"
#include "crect.h"
#include "cdlist.h"

class AccurateTimer;
class BaseFile;
class Event;
class MouseButtonStatus;
class MouseEvent;
class MouseSelectChronicler;
class MouseSelectEvent;
class NewGump;
class NewGumpId;
class Tracker;
class VGAMouseSelectEvent;
class VGAMouseSelectInspector;

// One event as kept in the record/playback file.
struct RecordedEvent
{
	int16_t type;
	int16_t x;
	int16_t y;
	int32_t data;
	uint16_t buttons;
	uint32_t gameloop;
	uint32_t ticks;
};

class Dispatcher
{
public:
	Dispatcher(int16_t);
	~Dispatcher(void);
	void uninit(void);
	void poll(void);
	void write(Event &);
	// Borland let a temporary bind to Event &; callers pass them so.
	void write(Event &&e) { write(e); }
	void translateMouse(VGAMouseSelectEvent &, Event *);
	void process(void);
	uint8_t read(void);
	uint8_t readingMyFuture(Event &);
	void send(Event &);
	void send(Event &&e) { send(e); }
	void sendInFuture(Event &);
	void badEvent(NewGump *, Event &);
	void refresh(Rect);
	void restore(Rect);
	void setExclusive(NewGump *);
	void clrExclusive(void);
	void pushBase(NewGump *);
	void popBase(void);
	NewGump *findGump(NewGumpId *, int16_t);
	void suspend(void);
	void resume(void);
	uint8_t getNextRecordedEvent(Event &);
	void recordEvent(Event &);
	uint8_t isMouseRightHanded(void);
	void switchHandedness(void);
	NewGump *getBase(void);
	static uint8_t active;
	static uint32_t gameloop;
	static int16_t reader;
	static int16_t writer;
	static int16_t futureReader;
	static int16_t futureWriter;
	static int16_t baseSP;
	static int16_t mode;
	static MouseSelectChronicler *theChronicler;
	static VGAMouseSelectInspector *theInspector;
	static Tracker *theTracker;
	static VGAMouseSelectEvent *mouseEventBuffer;
	static BaseFile *eventFile;
	static Event *eventBuffer;
	static Event *futureEventBuffer;
	static Event *event;
	static RecordedEvent *dispatcherMemory;
	static int32_t eventCounter;
	static NewGump *exclusive;
	static NewGump *suspended;
	static NewGump **base;
};

// The base gump of a modal loop; its private events hand back the result.
class UncleErnie : public NewGump
{
public:
	int32_t *result;

	UncleErnie(int32_t *);
	virtual void commandPrivate(Event &);
};

// Runs a gump modally on its own event, restoring the dispatcher afterwards.
class TightDispatcher
{
public:
	NewGump *oldExclusive;
	Event *oldEvent;
	uint8_t pad_08[4];

	TightDispatcher(NewGump *, int32_t *);
	~TightDispatcher(void);
	void run(void);
};

class VGAMouseSelectInspector : public MouseSelectInspector
{
public:
	static VGAMouseSelectEvent *get_last_event(void);
};

class MouseButtonStatus
{
public:
	char status;

	MouseButtonStatus(void);
	MouseButtonStatus(int16_t);
};

class MouseEvent
{
public:
	uint16_t _action;
	uint16_t _button;
	int16_t _x;
	uint16_t _y;
	MouseButtonStatus mouseButtonStatus;
	uint16_t extraStatus;

	MouseEvent(void);
	int16_t get_y(void);
	MouseButtonStatus get_button_status(void);
	uint16_t getExtraStatus(void);
	int16_t get_button(void);
};

class MouseSelectEvent : public MouseEvent
{
public:
	uint16_t _type;
	uint32_t _time;

	MouseSelectEvent(MouseSelectCommand, MouseButton, int16_t, int16_t);
	MouseSelectEvent(void);
	uint8_t is_valid(void);
	int16_t get_type(void);
};

// Mouse coordinates arrive doubled horizontally; get_x halves them for 320 wide.
class VGAMouseSelectEvent : public MouseSelectEvent
{
public:
	VGAMouseSelectEvent(void);
	int16_t get_x(void);
};

int16_t keyWaiting(void);
int32_t Dispatch(NewGump *);

extern Dispatcher *dispatcher;
extern AccurateTimer *atimer;
extern char *eventFileDesc;

#endif
