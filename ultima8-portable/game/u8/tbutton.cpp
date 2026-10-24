// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: TBUTTON.C

#include "u8port.h"

#include "dispatch.h"
#include "basecam.h"
#include "font.h"
#include "tbutton.h"

TextPushButton::TextPushButton(uint8_t id, NewGump *parent, char *text, uint8_t color) :
	PushButton(id, parent)
{
	this->text = text;
	int16_t width, height;
	GlobalFont::global_ptr->Font::dim(this->text, width, height);
	rect = Rect(0, 0, width + 8, height + 4);
	_frame = 0;
	this->color = color;
}

void TextPushButton::draw(int16_t x, int16_t y)
{
	Panel panel;
	Rect r(x, y, x + rect.width(), y + rect.height());
	uint8_t c = enabled ? color : color + 2;
	if (_frame == 0)
		panel.drawup(r, c);
	else
		panel.drawdown(r, c);
	int16_t height = GlobalFont::global_ptr->Font::height(text);
	GlobalFont::global_ptr->printf(x + _frame + 4, y + height + _frame + 2, text);
}

void TextPushButton::select(void)
{
	set_frame(1);
}

void TextPushButton::unselect(void)
{
	set_frame(0);
}

TextActivatorButton::TextActivatorButton(uint8_t id, NewGump *parent, char *text, uint8_t color) :
	ActivatorButton(id, parent)
{
	this->text = text;
	int16_t width, height;
	GlobalFont::global_ptr->Font::dim(this->text, width, height);
	rect = Rect(0, 0, width + 8, height + 4);
	_frame = 0;
	this->color = color;
}

void TextActivatorButton::draw(int16_t x, int16_t y)
{
	Panel panel;
	Rect r(x, y, x + rect.width(), y + rect.height());
	uint8_t c = enabled ? color : color + 2;
	if (_frame == 0)
		panel.drawup(r, c);
	else
		panel.drawdown(r, c);
	int16_t height = GlobalFont::global_ptr->Font::height(text);
	GlobalFont::global_ptr->printf(x + _frame + 4, y + height + _frame + 2, text);
}

void TextActivatorButton::select(void)
{
	set_frame(1);
}

void TextActivatorButton::unselect(void)
{
	set_frame(0);
}

inline int16_t TextPushButton::set_frame(int16_t frame)
{
	_frame = frame;
	refresh();
}

inline int16_t TextActivatorButton::set_frame(int16_t frame)
{
	_frame = frame;
	refresh();
}
