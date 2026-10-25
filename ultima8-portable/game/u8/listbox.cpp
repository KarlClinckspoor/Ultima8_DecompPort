// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -vi-
// name: LISTBOX.C

#include "u8port.h"

#include <string.h>
#include "newgump.h"
#include "cglobvp.h"
#include "crect.h"
#include "cvport.h"
#include "dispatch.h"
#include "font.h"
#include "listbox.h"
#include "menu.h"
#include "scrolbar.h"

// The shared headers declare these; each file that uses them defines them.
inline NewGumpId::NewGumpId(uint8_t type, uint8_t id) { instance = (type << 8) | id; f_02 = ~instance; f_04 = 0; }
inline void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
inline void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
inline Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }

inline NewGump *NewGumpList::getHead(void) { return (NewGump *)head; }

ListBoxGump::ListBoxGump(uint8_t id, NewGump *parent, Rect r, char **list, uint8_t sorted, int16_t count, int16_t stride) :
	NewGump(NewGumpId(5, id), parent, 0x12)
{
	rect = r;
	this->list = list;
	selected = -1;
	this->sorted = sorted;
	this->count = count;
	this->stride = stride;
	new ScrollBar(0, this, rect.height() - 1, 0, rect.width() - 9, 0, 1, 9);
	reset();
}

void ListBoxGump::draw(int16_t x, int16_t y)
{
	DrawRectangle(GlobalVport::global_ptr, x, y, x + rect.width() - 1, y + rect.height() - 1, GumpColorMap[0]);
	DrawBox(GlobalVport::global_ptr, x + 1, y + 1, x + rect.width() - 2, y + rect.height() - 2, GumpColorMap[5]);
	int16_t top = ((ScrollBar *)newGumpList.getHead())->getPos();
	if (selected >= top && selected <= top + visibleLines - 1)
	{
		int16_t offset = lineHeight * (selected - top);
		Rect bar(x + 1, y + offset, x + rect.width() - 9, y + offset + lineHeight);
		DrawBox(GlobalVport::global_ptr, bar.f_00, bar.f_02, bar.f_04, bar.f_06, GumpColorMap[8]);
	}
	int16_t lineY = y + lineHeight;
	int16_t line = 0;
	for (int16_t i = top; list && line < visibleLines && i < count; i++, line++)
	{
		GlobalFont::global_ptr->printf(x + 3, lineY, (int16_t)(rect.width() - 12), getString(i));
		lineY += lineHeight;
	}
}

void ListBoxGump::commandMouseLeft(Event &event)
{
	if (event.isSingle())
	{
		f_1d |= 9;
		setExclusive(this);
		commandMouseMovement(event);
	}
	else if (event.isRelease())
	{
		f_1d &= ~9;
		clrExclusive();
		send(Event(EVENT_PRIVATE, selected, 0, 1, this, parent, 0));
	}
	else if (event.isDouble())
	{
		commandMouseMovement(event);
		send(Event(EVENT_PRIVATE, selected, 0, 2, this, parent, 0));
	}
}

void ListBoxGump::commandMouseMovement(Event &event)
{
	int16_t top = ((ScrollBar *)newGumpList.getHead())->getPos();
	if (!contains(event))
	{
		selected = -1;
		if (event.y < rect.f_02 && top > 0)
			((ScrollBar *)newGumpList.getHead())->setPos(top - 1);
		else if (event.y > rect.f_06 && getString(top + visibleLines - 1))
			((ScrollBar *)newGumpList.getHead())->setPos(top + 1);
	}
	else
	{
		if (!list)
			return;
		int16_t line = top + getRelY(event) / lineHeight;
		if (top + visibleLines - 1 >= line)
		{
			int16_t i;
			for (i = top; i < line; i++)
			{
				if (count == i + 1)
					break;
			}
			selected = i;
		}
	}
	refresh();
}

void ListBoxGump::commandNoEvents(Event &event)
{
	commandMouseMovement(event);
}

void ListBoxGump::commandPrivate(Event &)
{
	refresh();
}

void ListBoxGump::setSelected(int16_t index)
{
	if (!getString(index))
		selected = -1;
	selected = index;
	((ScrollBar *)newGumpList.getHead())->setPos(index);
	refresh();
}

void ListBoxGump::setList(char **list, uint8_t sorted, int16_t count, int16_t stride)
{
	this->list = list;
	selected = -1;
	this->sorted = sorted;
	this->count = count;
	this->stride = stride;
	reset();
}

void ListBoxGump::reset(void)
{
	if (list && count == -1)
	{
		int16_t i;
		// getString() reads only below count, so count runs one ahead.
		for (i = 0, count = 1; getString(i); i++, count++)
			;
		count--;
	}
	lineHeight = GlobalFont::global_ptr->height(' ');
	visibleLines = rect.height() / lineHeight;
	int16_t maxTop = count - visibleLines + 1;
	if (maxTop < 0)
		maxTop = 0;
	((ScrollBar *)newGumpList.getHead())->setRange(maxTop);
	sort();
}

void ListBoxGump::sort(void)
{
	if (!sorted)
		return;
	if (stride > -1)
		return;
	if (!list)
		return;
	int16_t i;
	int16_t j;
	char *swap;
	int16_t least;
	for (i = 0; i < count - 1; i++)
	{
		least = i;
		for (j = i + 1; j < count; j++)
		{
			if (strcmp(getString(j), getString(least)) < 0)
				least = j;
		}
		swap = list[least];
		list[least] = list[i];
		list[i] = swap;
	}
}

char *ListBoxGump::getString(int16_t index)
{
	if (index >= count || index < 0)
		return 0;
	if (stride == -1)
		return list[index];
	return (char *)list + index * stride;
}
