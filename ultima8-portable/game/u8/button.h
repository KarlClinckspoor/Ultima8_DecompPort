#ifndef BUTTON_H
#define BUTTON_H

#include "u8port.h"

#include "dispatch.h"

class Event;
class PushButton;
class RawButton;

class RawButton : public NewGump
{
public:
	uint8_t enabled;

	inline RawButton(uint8_t, NewGump *, uint8_t);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseRight(Event &);
	virtual void commandMouseMovement(Event &);
	virtual void draw(int16_t, int16_t) = 0;
	virtual void select(void) = 0;
	virtual void unselect(void) = 0;
	static uint16_t selecting;
};

class PushButton : public RawButton
{
public:
	PushButton(uint8_t, NewGump *);
	virtual void commandMouseLeft(Event &);
};

class CycleButton : public RawButton
{
public:
	int16_t _frame;
	int16_t f_37;

	CycleButton(uint8_t, NewGump *);
};

class ActivatorButton : public PushButton
{
public:
	ActivatorButton(uint8_t, NewGump *);
	virtual void commandNoEvents(Event &);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseMovement(Event &);
};

#endif
