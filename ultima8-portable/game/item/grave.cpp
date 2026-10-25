// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\ITEM\GRAVE.C

#include "u8port.h"

#include <string.h>
#include "newgump.h"
#include "dispatch.h"
#include "cvport.h"
#include "ccurrvp.h"
#include "cglobvp.h"
#include "u8gmpshp.h"
#include "cexit.h"
#include "grave.h"

// Inline in the shared headers when this file was built.
U8_INLINE Point::Point(void) {}
U8_INLINE void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE Rect::Rect(void) {}
U8_INLINE void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
U8_INLINE Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }
U8_INLINE void NewGump::moveto(int16_t x, int16_t y) { rect.moveto(x, y); }
U8_INLINE GumpShape::GumpShape(int16_t shape) { _frame = 0; this->shape = shape; }
U8_INLINE Rect GumpShape::get_rect(void) { return drawer->get_rect(shape, _frame); }
U8_INLINE void GumpShape::draw(int16_t x, int16_t y) { drawer->draw(x, y, shape, _frame); }
U8_INLINE CacheFont::~CacheFont(void) {}
U8_INLINE GraphicYtable::GraphicYtable(void) { f_04 = 2; index = 0; }
U8_INLINE Vport::Vport(void) { mem = 0; f_0f = 2; f_10 = 0; }
U8_INLINE Vport::~Vport(void) { free(); }

#define LINE_GAP	4

GraveGump::GraveGump(NewGump *parent, int16_t shape, FontType font, Referent referent, char *message) :
	ItemRelativeGump((IRGumpType)4, parent, Rect(0, 0, 0, 0), referent, (IRGumpMove)1, 0, 120, 0),
	GumpShape(shape)
{
	f_1d |= 0x22;
	registerHotKeys(-1, 0);
	fontType = font;
	graveShape = shape;
	rect = get_rect();
	moveto((320 - rect.f_04) >> 1, (200 - rect.f_06) >> 1);

	textLength = strlen(message) + 1;
	text = new char[textLength];
	if (text == 0)
		outOfMemory(__FILE__, 81);	// __LINE__
	strcpy(text, message);
	mouseDown = 0;
	this->parent->newGumpList.moveToTail(this);

	lineCount = 0;
	offset = -1;
	while (moreToCome())
	{
		nextLine();
		lineCount++;
	}
	offset = -1;
	setExclusive(0);
	refresh();
}

GraveGump::~GraveGump(void)
{
	clrExclusive();
	if (text)
		delete text;
}

Boolean GraveGump::moreToCome(void)
{
	if (text == 0)
		return FALSE;
	if (offset == 0xFFFF)	// -1, as Borland compared it
		return TRUE;
	if (offset + strlen(text + offset) + 1 >= textLength)
		return FALSE;
	return TRUE;
}

// Moves to the next line and ends it at its line break.
Boolean GraveGump::nextLine(void)
{
	if (text == 0)
		return FALSE;
	if (offset == 0xFFFF)	// -1, as Borland compared it
		offset = 0;
	else
	{
		offset += strlen(text + offset) + 1;
		if (offset >= textLength)
			return FALSE;
	}
	char *p = text + offset;
	while (*p++)
	{
		if (*p == '*' || *p == 0)
		{
			*p = 0;
			break;
		}
	}
	return TRUE;
}

void GraveGump::commandPrivate(Event &event)
{
	if (event.data == -1)
		killMyself();
}

void GraveGump::commandKeyboard(Event &)
{
	killMyself();
}

void GraveGump::commandMouseLeft(Event &event)
{
	if (event.isSingle())
	{
		mouseDown = 1;
		return;
	}
	if (event.isRelease() && mouseDown)
	{
		mouseDown = 0;
		killMyself();
		return;
	}
	if (event.isDouble())
		killMyself();
}

void GraveGump::draw(int16_t x, int16_t y)
{
	int16_t w;
	int16_t h;

	CacheFont font(fontType);
	GumpShape::draw(x, y);
	Rect textRect = U8GumpShapeDrawer::rects[graveShape - 1];
	textRect.moveto(x + textRect.f_00, y + textRect.f_02);
	Vport vport;
	vport = *GlobalVport::global_ptr;
	vport.rect = textRect;
	vport.rect.clip(GlobalVport::global_ptr->rect);

	// Centre the block of lines vertically.
	font.dim('A', w, h);
	int16_t textY = (textRect.f_06 - textRect.f_02 + 1 - (lineCount * h + lineCount * LINE_GAP)) >> 1;
	offset = -1;
	{
		CurrentVport current(&vport);
		while (moreToCome())
		{
			nextLine();

			// Trim the spaces at both ends of the line.
			uint16_t len = strlen(text + offset);
			int16_t start;
			for (start = offset; text[start] == ' ' && start - offset < len; start++)
				;
			int16_t end;
			for (end = offset + len - 1; text[end] == ' ' && end != start; end--)
				;
			if (start < end)
			{
				char saved = 0;
				if (text[end + 1])
				{
					saved = text[end + 1];
					text[end + 1] = 0;
				}
				font.Font::dim(text + start, w, h);
				w = (textRect.f_04 - textRect.f_00 + 1 - w) >> 1;
				font.printf(textRect.f_00 + w, textRect.f_02 + textY + font.getBaseLine(), text + start);
				if (saved)
					text[end + 1] = saved;
				textY += h + LINE_GAP;
			}
		}
	}
}

uint16_t Grave::read(int16_t shape, char *message)
{
	if (message)
	{
		GraveGump *gump = new GraveGump(Dispatcher::base[Dispatcher::baseSP], shape, (FontType)11, referent, message);
		return gump->f_4e;
	}
	return 0;	// fell off the end in DOS (unused there)
}

uint16_t Plaque::read(int16_t shape, char *message)
{
	if (message)
	{
		GraveGump *gump = new GraveGump(Dispatcher::base[Dispatcher::baseSP], shape, (FontType)10, referent, message);
		return gump->f_4e;
	}
	return 0;	// fell off the end in DOS (unused there)
}
