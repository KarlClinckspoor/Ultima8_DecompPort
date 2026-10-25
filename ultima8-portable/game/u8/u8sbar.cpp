// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: U8SBAR.C

#include "u8port.h"

#include "newgump.h"
#include "crect.h"
#include "dispatch.h"
#include "menu.h"
#include "sbutton.h"
#include "u8gmpshp.h"
#include "u8sbar.h"

// Inline in the shared headers when this file was built.
U8_INLINE Point::Point(void) {}
U8_INLINE void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE Rect::Rect(void) {}
U8_INLINE void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
U8_INLINE Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }
U8_INLINE int16_t Rect::width(void) { return f_04 - f_00 + 1; }
U8_INLINE int16_t Rect::height(void) { return f_06 - f_02 + 1; }
U8_INLINE NewGumpId::NewGumpId(uint16_t i) { instance = i; f_02 = ~instance; f_04 = 0; }
U8_INLINE NewGumpId::NewGumpId(uint8_t type, uint8_t id) { instance = (type << 8) | id; f_02 = ~instance; f_04 = 0; }
U8_INLINE uint8_t NewGumpId::getInstance(void) { return instance & 0xff; }
U8_INLINE void NewGump::moveto(int16_t x, int16_t y) { rect.moveto(x, y); }
U8_INLINE GumpShape::GumpShape(int16_t shape) { _frame = 0; this->shape = shape; }
U8_INLINE Rect GumpShape::get_rect(void) { return drawer->get_rect(shape, _frame); }
U8_INLINE void GumpShape::draw(int16_t x, int16_t y) { drawer->draw(x, y, shape, _frame); }

void U8RightActivatorButton::draw(int16_t x, int16_t y)
{
	GumpShape::draw(x, y);
}

void U8LeftActivatorButton::draw(int16_t x, int16_t y)
{
	GumpShape::draw(x, y);
}

U8Thumb::U8Thumb(NewGump *parent, Rect r, int32_t resolution) :
	NewGump(NewGumpId(3), parent, 2), GumpShape(45)
{
	rect = r;
	thumb = 0;
	dragging = 0;
	int16_t size = rect.height() - 1;
	thumbRect = Rect(0, 0, size, size);
	setResolution(resolution);
}

void U8Thumb::setResolution(int16_t steps)
{
	int32_t range = rect.width();
	int16_t width = thumbRect.width();
	range -= width;
	if (steps)
		steps--;
	if (steps > 0)
	{
		resolution = steps;
		scale = (range << 16) / resolution;
	}
	else
	{
		scale = 0;
		thumb = 0;
	}
}

void U8Thumb::draw(int16_t x, int16_t y)
{
	if (scale)
		GumpShape::draw(x + thumbRect.f_00, y);
}

void U8Thumb::commandMouseMovement(Event &event)
{
	int32_t x = getRelX(event) - (thumbRect.width() >> 1);
	if (x < 0)
		x = 0;
	if (scale)
	{
		int32_t t = (x << 16) / scale;
		setThumb((uint16_t)t);
		send(Event(EVENT_PRIVATE, 0, 0, 1, this, parent, 0));
	}
}

void U8Thumb::commandMouseLeft(Event &event)
{
	if (event.isSingle() && !dragging)
	{
		setExclusive(this);
		f_1d |= 8;
		dragging = 1;
		commandMouseMovement(event);
	}
	else if (event.isRelease() && dragging)
	{
		dragging = 0;
		clrExclusive();
		f_1d &= ~8;
		send(Event(EVENT_PRIVATE, 0, 0, 1, this, parent, 0));
	}
}

void U8Thumb::setThumb(int32_t t)
{
	if (thumb == t)
		return;
	if (!scale)
	{
		thumb = 0;
		return;
	}
	int32_t range = rect.width();
	int32_t width = thumbRect.width();
	range -= width;
	if (t > resolution)
		t = resolution;
	else if (t < 0)
		t = 0;
	int16_t x = (t * scale) >> 16;
	if (x < 0)
		x = 0;
	if (x > range)
		x = range;
	thumb = t;
	thumbRect.moveto(x, rect.f_02);
}

U8ScrollBar::U8ScrollBar(uint8_t id, NewGump *parent, int16_t length, int16_t steps, int16_t x, int16_t y) :
	NewGump(NewGumpId(4, id), parent, EVENT_PRIVATE)
{
	int16_t right = length;
	rect = Rect(0, 0, length, 9);
	U8RightActivatorButton *rightButton;
	U8LeftActivatorButton *left;
	Rect buttonRect(0, 0, rect.height() - 1, rect.height() - 1);
	left = new U8LeftActivatorButton(1, this, buttonRect, GumpColorMap[5]);
	rightButton = new U8RightActivatorButton(2, this, buttonRect, GumpColorMap[5]);
	buttonRect = rightButton->get_rect();
	left->moveto(0, 0);
	rightButton->moveto(rect.f_04 - buttonRect.width() + 1, 0);
	right -= buttonRect.width() - 1;
	pos = new U8Thumb(this, Rect(buttonRect.width() + 1, 0, right, rect.height() - 1), steps);
	rect.moveto(x, y);
}

void U8ScrollBar::commandPrivate(Event &event)
{
	switch (event.from->newGumpId.getInstance())
	{
	case 1:
		pos->decThumb(1);
		refresh();
		break;
	case 2:
		pos->incThumb(1);
		pos->refresh();
		refresh();
		break;
	}
	send(Event(EVENT_PRIVATE, pos->getThumb(), 0, 1, this, parent, 0));
}

void U8ScrollBar::setPos(int16_t p)
{
	pos->setThumb(p);
}

int16_t U8ScrollBar::getPos(void)
{
	return pos->getThumb();
}

void U8ScrollBar::setRange(int16_t range)
{
	pos->setResolution(range);
}
