// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -vi-
// name: SCROLBAR.C

#include "u8port.h"

#include "newgump.h"
#include "basecam.h"
#include "cglobvp.h"
#include "crect.h"
#include "cvport.h"
#include "dispatch.h"
#include "menu.h"
#include "scrolbar.h"

// The shared headers declare these; each file that uses them defines them.
inline NewGumpId::NewGumpId(uint8_t type, uint8_t id) { instance = (type << 8) | id; f_02 = ~instance; f_04 = 0; }
inline Rect::Rect(void) {}
inline void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
inline void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
inline Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }

// Inline in the shared headers when this file was built.
inline Point::Point(int16_t x, int16_t y) { set(x, y); }

// The arrows are drawn as three lines from the centre of the button face.

void RightActivatorButton::draw(int16_t x, int16_t y)
{
	Panel panel;
	Rect r = rect;
	r.moveto(x, y);
	if (pressed)
		panel.drawdown(r, color);
	else
		panel.drawup(r, color);
	r.f_00 += 2;
	r.f_02 += 2;
	r.f_04 -= 2;
	r.f_06 -= 2;
	r.moverel(pressed, pressed);
	Point centre((r.f_00 + r.f_04) >> 1, (r.f_02 + r.f_06) >> 1);
	DrawLine(GlobalVport::global_ptr, r.f_00, centre.f_02, r.f_04, centre.f_02, GumpColorMap[0]);
	DrawLine(GlobalVport::global_ptr, centre.f_00, r.f_02, r.f_04, centre.f_02, GumpColorMap[0]);
	DrawLine(GlobalVport::global_ptr, centre.f_00, r.f_06, r.f_04, centre.f_02, GumpColorMap[0]);
}

void LeftActivatorButton::draw(int16_t x, int16_t y)
{
	Panel panel;
	Rect r = rect;
	r.moveto(x, y);
	if (pressed)
		panel.drawdown(r, color);
	else
		panel.drawup(r, color);
	r.f_00 += 2;
	r.f_02 += 2;
	r.f_04 -= 2;
	r.f_06 -= 2;
	r.moverel(pressed, pressed);
	Point centre((r.f_00 + r.f_04) >> 1, (r.f_02 + r.f_06) >> 1);
	DrawLine(GlobalVport::global_ptr, r.f_00, centre.f_02, r.f_04, centre.f_02, GumpColorMap[0]);
	DrawLine(GlobalVport::global_ptr, centre.f_00, r.f_02, r.f_00, centre.f_02, GumpColorMap[0]);
	DrawLine(GlobalVport::global_ptr, centre.f_00, r.f_06, r.f_00, centre.f_02, GumpColorMap[0]);
}

void UpActivatorButton::draw(int16_t x, int16_t y)
{
	Panel panel;
	Rect r = rect;
	r.moveto(x, y);
	if (pressed)
		panel.drawdown(r, color);
	else
		panel.drawup(r, color);
	r.f_00 += 2;
	r.f_02 += 2;
	r.f_04 -= 2;
	r.f_06 -= 2;
	r.moverel(pressed, pressed);
	Point centre((r.f_00 + r.f_04) >> 1, (r.f_02 + r.f_06) >> 1);
	DrawLine(GlobalVport::global_ptr, centre.f_00, r.f_02, centre.f_00, r.f_06, GumpColorMap[0]);
	DrawLine(GlobalVport::global_ptr, r.f_00, centre.f_02, centre.f_00, r.f_02, GumpColorMap[0]);
	DrawLine(GlobalVport::global_ptr, r.f_04, centre.f_02, centre.f_00, r.f_02, GumpColorMap[0]);
}

void DownActivatorButton::draw(int16_t x, int16_t y)
{
	Panel panel;
	Rect r = rect;
	r.moveto(x, y);
	if (pressed)
		panel.drawdown(r, color);
	else
		panel.drawup(r, color);
	r.f_00 += 2;
	r.f_02 += 2;
	r.f_04 -= 2;
	r.f_06 -= 2;
	r.moverel(pressed, pressed);
	Point centre((r.f_00 + r.f_04) >> 1, (r.f_02 + r.f_06) >> 1);
	DrawLine(GlobalVport::global_ptr, centre.f_00, r.f_02, centre.f_00, r.f_06, GumpColorMap[0]);
	DrawLine(GlobalVport::global_ptr, r.f_00, centre.f_02, centre.f_00, r.f_06, GumpColorMap[0]);
	DrawLine(GlobalVport::global_ptr, r.f_04, centre.f_02, centre.f_00, r.f_06, GumpColorMap[0]);
}

Thumb::Thumb(NewGump *parent, Rect r, uint8_t vertical, int32_t resolution) :
	NewGump(NewGumpId(0, 3), parent, 2)
{
	rect = r;
	this->vertical = vertical;
	thumb = 0;
	dragging = 0;
	int16_t size = this->vertical ? rect.width() - 1 : rect.height() - 1;
	thumbRect = Rect(0, 0, size, size);
	setResolution(resolution);
}

void Thumb::setResolution(int16_t steps)
{
	int32_t range = vertical ? rect.height() : rect.width();
	int16_t width = vertical ? thumbRect.height() : thumbRect.width();
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

void Thumb::draw(int16_t x, int16_t y)
{
	Rect r;
	r = rect;
	r.moveto(x, y);
	r.f_00++;
	r.f_04--;
	r.f_02++;
	r.f_06--;
	if (vertical)
	{
		DrawLine(GlobalVport::global_ptr, x, y + 1, x, y + rect.height() - 1, GumpColorMap[0]);
		DrawLine(GlobalVport::global_ptr, x + rect.width() - 1, y + 1, x + rect.width() - 1, y + rect.height() - 1, GumpColorMap[0]);
	}
	else
	{
		DrawLine(GlobalVport::global_ptr, x, y, x + rect.width() - 1, y, GumpColorMap[0]);
		DrawLine(GlobalVport::global_ptr, x, y + rect.height() - 1, x + rect.width() - 1, y + rect.height() - 1, GumpColorMap[0]);
	}
	DrawBox(GlobalVport::global_ptr, r.f_00, r.f_02, r.f_04, r.f_06, GumpColorMap[2]);
	if (scale)
	{
		Panel panel;
		Rect t = thumbRect;
		if (vertical)
			t.moveto(x, y + thumbRect.f_02);
		else
			t.moveto(x + thumbRect.f_00, y);
		panel.drawup(t, GumpColorMap[5]);
	}
}

void Thumb::commandMouseMovement(Event &event)
{
	int32_t at;
	if (vertical)
		at = getRelY(event) - (thumbRect.height() >> 1);
	else
		at = getRelX(event) - (thumbRect.width() >> 1);
	if (at < 0)
		at = 0;
	if (scale)
	{
		int32_t t = (at << 16) / scale;
		setThumb((uint16_t)t);
		refresh();
	}
}

void Thumb::commandMouseLeft(Event &event)
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

void Thumb::setThumb(int32_t t)
{
	if (thumb == t)
		return;
	if (!scale)
	{
		thumb = 0;
		return;
	}
	int32_t range = vertical ? rect.height() : rect.width();
	int32_t width = vertical ? thumbRect.height() : thumbRect.width();
	range -= width;
	if (t > resolution)
		t = resolution;
	else if (t < 0)
		t = 0;
	int16_t at = (t * scale) >> 16;
	if (at < 0)
		at = 0;
	if (at > range)
		at = range;
	thumb = t;
	if (vertical)
		thumbRect.moveto(rect.f_00, at);
	else
		thumbRect.moveto(at, rect.f_02);
}

ScrollBar::ScrollBar(uint8_t id, NewGump *parent, int16_t length, int16_t steps, int16_t x, int16_t y, uint8_t vertical, int16_t width) :
	NewGump(NewGumpId(4, id), parent, EVENT_PRIVATE)
{
	rect = vertical ? Rect(0, 0, width - 1, length) : Rect(0, 0, length, width - 1);
	DrawMyselfActivatorButton *inc;
	DrawMyselfActivatorButton *dec;
	Rect buttonRect;
	if (vertical)
	{
		buttonRect.set(0, 0, rect.width() - 1, rect.width() - 1);
		dec = new UpActivatorButton(1, this, buttonRect, GumpColorMap[5]);
		inc = new DownActivatorButton(2, this, buttonRect, GumpColorMap[5]);
	}
	else
	{
		buttonRect.set(0, 0, rect.height() - 1, rect.height() - 1);
		dec = new LeftActivatorButton(1, this, buttonRect, GumpColorMap[5]);
		inc = new RightActivatorButton(2, this, buttonRect, GumpColorMap[5]);
	}
	dec->moveto(0, 0);
	inc->moveto(rect.f_04 - buttonRect.width() + 1, rect.f_06 - buttonRect.height() + 1);
	length -= vertical ? buttonRect.height() - 1 : buttonRect.width() - 1;
	pos = new Thumb(this, vertical ? Rect(0, buttonRect.height() - 1, rect.width() - 1, length) :
		Rect(buttonRect.width() - 1, 0, length, rect.height() - 1), vertical, steps);
	rect.moveto(x, y);
}

void ScrollBar::commandPrivate(Event &event)
{
	switch (event.from->newGumpId.getInstance())
	{
	case 1:
		pos->decThumb(1);
		pos->refresh();
		break;
	case 2:
		pos->incThumb(1);
		pos->refresh();
		break;
	}
	send(Event(EVENT_PRIVATE, pos->getThumb(), 0, 1, this, parent, 0));
}

void ScrollBar::setPos(int16_t p)
{
	pos->setThumb(p);
}

int16_t ScrollBar::getPos(void)
{
	return pos->getThumb();
}

void ScrollBar::setRange(int16_t range)
{
	pos->setResolution(range);
}
