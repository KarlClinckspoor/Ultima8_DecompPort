#ifndef U8SBAR_H
#define U8SBAR_H

#include "u8port.h"

#include "dispatch.h"
#include "sbutton.h"
#include "scrolbar.h"

class Event;
class Rect;

class U8RightActivatorButton : public DrawMyselfActivatorButton, public GumpShape
{
public:
	U8RightActivatorButton(uint8_t id, NewGump *parent, Rect r, uint8_t color) :
		DrawMyselfActivatorButton(id, parent, r, color), GumpShape(44) { rect = get_rect(); }
	virtual void draw(int16_t, int16_t);
};

class U8LeftActivatorButton : public DrawMyselfActivatorButton, public GumpShape
{
public:
	U8LeftActivatorButton(uint8_t id, NewGump *parent, Rect r, uint8_t color) :
		DrawMyselfActivatorButton(id, parent, r, color), GumpShape(43) { rect = get_rect(); }
	virtual void draw(int16_t, int16_t);
};

// Thumb positions run 0..resolution; scale is pixels per step in 16.16 fixed point.
class U8Thumb : public NewGump, public GumpShape
{
public:
	int32_t resolution;
	uint8_t dragging;
	Rect thumbRect;
	int32_t thumb;
	int32_t scale;

	U8Thumb(NewGump *, Rect, int32_t);
	virtual void commandMouseLeft(Event &);
	virtual void commandMouseMovement(Event &);
	virtual void draw(int16_t, int16_t);
	virtual void setThumb(int32_t);
	virtual int32_t getThumb(void) { return thumb; }
	virtual void setResolution(int16_t);
	virtual void incThumb(int16_t step) { setThumb(thumb + step); }
	virtual void decThumb(int16_t step) { setThumb(thumb - step); }
};

class U8ScrollBar : public NewGump
{
public:
	U8Thumb *pos;

	U8ScrollBar(uint8_t, NewGump *, int16_t, int16_t, int16_t, int16_t);
	virtual void commandPrivate(Event &);
	virtual void setPos(int16_t);
	virtual int16_t getPos(void);
	virtual void setRange(int16_t);
};

#endif
