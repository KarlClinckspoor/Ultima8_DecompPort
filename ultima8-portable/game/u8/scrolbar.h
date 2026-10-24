#ifndef SCROLBAR_H
#define SCROLBAR_H

#include "u8port.h"

#include "button.h"
#include "crect.h"
#include "dispatch.h"

class DrawMyselfActivatorButton;
class Event;

class Thumb : public NewGump
{
public:
	int32_t resolution;
	uint8_t vertical;
	uint8_t dragging;
	Rect thumbRect;
	int32_t thumb;
	int32_t scale;			// pixels per step, 16.16 fixed point

	Thumb(NewGump *, Rect, uint8_t, int32_t);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseMovement(Event &);
	virtual void draw(int16_t, int16_t);
	virtual void setThumb(int32_t);
	virtual int32_t getThumb(void) { return thumb; }
	virtual void setResolution(int16_t);
	virtual void incThumb(int16_t step) { setThumb(thumb + step); }
	virtual void decThumb(int16_t step) { setThumb(thumb - step); }
};

class ScrollBar : public NewGump
{
public:
	Thumb *pos;

	ScrollBar(uint8_t, NewGump *, int16_t, int16_t, int16_t, int16_t, uint8_t, int16_t);
	virtual void commandPrivate(Event &);
	virtual void setPos(int16_t);
	virtual int16_t getPos(void);
	virtual void setRange(int16_t);
};

class DrawMyselfActivatorButton : public ActivatorButton
{
public:
	uint16_t pressed;
	uint8_t color;

	DrawMyselfActivatorButton(uint8_t id, NewGump *parent, Rect r, uint8_t color) :
		ActivatorButton(id, parent) { rect = r; this->color = color; pressed = 0; }
	virtual void select(void) { pressed = 1; refresh(); }
	virtual void unselect(void) { pressed = 0; refresh(); }
};

class RightActivatorButton : public DrawMyselfActivatorButton
{
public:
	RightActivatorButton(uint8_t id, NewGump *parent, Rect r, uint8_t color) :
		DrawMyselfActivatorButton(id, parent, r, color) {}
	virtual void draw(int16_t, int16_t);
};

class LeftActivatorButton : public DrawMyselfActivatorButton
{
public:
	LeftActivatorButton(uint8_t id, NewGump *parent, Rect r, uint8_t color) :
		DrawMyselfActivatorButton(id, parent, r, color) {}
	virtual void draw(int16_t, int16_t);
};

class UpActivatorButton : public DrawMyselfActivatorButton
{
public:
	UpActivatorButton(uint8_t id, NewGump *parent, Rect r, uint8_t color) :
		DrawMyselfActivatorButton(id, parent, r, color) {}
	virtual void draw(int16_t, int16_t);
};

class DownActivatorButton : public DrawMyselfActivatorButton
{
public:
	DownActivatorButton(uint8_t id, NewGump *parent, Rect r, uint8_t color) :
		DrawMyselfActivatorButton(id, parent, r, color) {}
	virtual void draw(int16_t, int16_t);
};

#endif
