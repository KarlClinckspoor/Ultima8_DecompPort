// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: MESSAGE.C

#include "u8port.h"

#include "newgump.h"
#include "crect.h"
#include "dispatch.h"
#include "font.h"
#include "tbutton.h"
#include "message.h"

MessageGump::MessageGump(NewGumpId id, NewGump *parent, char *message, uint8_t color) :
	PanelGump(id, parent, Rect(0, 0, 4, 4), color)
{
	text = message;
	TextPushButton *button = new TextPushButton(0, this, "Ok", color + 2);
	rect = Rect(0, 0, button->rect.width() + 8, button->rect.height() + 12);
	GlobalFont::global_ptr->Font::dim(text, textWidth, textHeight);
	if (rect.width() < textWidth + 8)
		rect.f_04 = textWidth + 8;
	rect.f_06 += textHeight;
	button->moveto((rect.width() >> 1) - (button->rect.width() >> 1), rect.height() - (button->rect.height() + 4));
	rect.moveto(160 - (rect.width() >> 1), 100 - (rect.height() >> 1));
	if (this->parent)
		rect.moverel(-this->parent->get_dx(), -this->parent->get_dy());
	registerHotKeys(13, 0);
}

void MessageGump::commandPrivate(Event &)
{
	killMyself();
}

void MessageGump::commandKeyboard(Event &)
{
	killMyself();
}

void MessageGump::draw(int16_t x, int16_t y)
{
	PanelGump::draw(x, y);
	int16_t line = 1;
	char *p = text;
	char *start = text;
	char more = 0;
	int16_t height, width;
	do
	{
		if (more)
		{
			more = 0;
			*p = '\n';
			p++;
			line++;
			start = p;
		}
		while (*p && *p != '\n')
			p++;
		if (*p == '\n')
		{
			more = 1;
			*p = 0;
		}
		GlobalFont::global_ptr->Font::dim(start, width, height);
		int16_t left = (rect.width() >> 1) - (width >> 1);
		GlobalFont::global_ptr->printf(x + left, y + height * line + 4, start);
	} while (more);
}
