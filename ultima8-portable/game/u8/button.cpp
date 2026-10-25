// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -vi-
// name: BUTTON.C

#include "u8port.h"

#include "newgump.h"
#include "button.h"
#include "dispatch.h"

// Bits of selecting.
#define SEL_PRESSED	1
#define SEL_LIT		2
#define SEL_RIGHT	4

uint16_t RawButton::selecting = 0;

void RawButton::commandMouseRight(Event &event)
{
	commandMouseLeft(event);
}

void RawButton::commandMouseLeft(Event &event)
{
	if (event.isSingle() && !selecting)
	{
		if (!enabled)
			return;
		setExclusive(0);
		f_1d |= 8;
		selecting = SEL_PRESSED | SEL_LIT;
		if (event.type == 4)
			selecting |= SEL_RIGHT;
		select();
		parent->refresh();
	}
	else if (event.isRelease() && selecting)
	{
		int32_t data = (selecting & SEL_RIGHT) ? 2L : 1L;
		selecting = 0;
		clrExclusive();
		f_1d &= ~8;
		if (contains(event))
			send(Event(EVENT_PRIVATE, 0, 0, data, this, parent, 0));
	}
}

void RawButton::commandMouseMovement(Event &event)
{
	if (contains(event))
	{
		if (~selecting & SEL_LIT)
		{
			select();
			parent->refresh();
			selecting |= SEL_LIT;
		}
	}
	else if (selecting & SEL_LIT)
	{
		unselect();
		parent->refresh();
		selecting &= ~SEL_LIT;
	}
}

PushButton::PushButton(uint8_t id, NewGump *parent) :
	RawButton(id, parent, 1)
{
}

void PushButton::commandMouseLeft(Event &event)
{
	RawButton::commandMouseLeft(event);
	if (event.isRelease() && !selecting)
	{
		unselect();
		refresh();
	}
}

CycleButton::CycleButton(uint8_t id, NewGump *parent) :
	RawButton(id, parent, 1)
{
}

ActivatorButton::ActivatorButton(uint8_t id, NewGump *parent) :
	PushButton(id, parent)
{
}

void ActivatorButton::commandMouseLeft(Event &event)
{
	if (event.isSingle() && !selecting)
	{
		if (!enabled)
			return;
		setExclusive(0);
		f_1d |= 9;
		selecting |= SEL_PRESSED | SEL_LIT;
		if (event.type == 4)
			selecting |= SEL_RIGHT;
		select();
		refresh();
		commandNoEvents(event);
	}
	else if (event.isRelease() && selecting)
	{
		selecting = 0;
		clrExclusive();
		f_1d &= ~9;
		unselect();
		refresh();
	}
}

void ActivatorButton::commandMouseMovement(Event &event)
{
	RawButton::commandMouseMovement(event);
	commandNoEvents(event);
}

// Repeats while the button is held down over it.
void ActivatorButton::commandNoEvents(Event &)
{
	if (selecting & SEL_LIT)
	{
		int32_t data = (selecting & SEL_RIGHT) ? 2L : 1L;
		send(Event(EVENT_PRIVATE, 0, 0, data, this, parent, 0));
	}
}

U8_INLINE RawButton::RawButton(uint8_t id, NewGump *parent, uint8_t enabled) :
	NewGump(NewGumpId(2, id), parent, 2)
{
	selecting = 0;
	this->enabled = enabled;
}

U8_INLINE NewGumpId::NewGumpId(uint8_t type, uint8_t id)
{
	instance = (type << 8) | id;
	f_02 = ~instance;
	f_04 = 0;
}
