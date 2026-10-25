// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -vi-
// name: COMBOBOX.C

#include "u8port.h"

#include <string.h>
#include "newgump.h"
#include "combobox.h"
#include "crect.h"
#include "dispatch.h"
#include "error.h"
#include "listbox.h"
#include "textedit.h"

// The shared headers declare these; each file that uses them defines them.
U8_INLINE NewGumpId::NewGumpId(uint8_t type, uint8_t id) { instance = (type << 8) | id; f_02 = ~instance; f_04 = 0; }
U8_INLINE void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
U8_INLINE Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }

U8_INLINE int16_t Rect::height(void) { return f_06 - f_02 + 1; }
U8_INLINE void NewGump::moveto(int16_t x, int16_t y) { rect.moveto(x, y); }
U8_INLINE uint8_t NewGumpId::getType(void) { return instance >> 8; }
U8_INLINE int16_t ListBoxGump::getSelected(void) { return selected; }

ComboBox::ComboBox(uint8_t id, NewGump *parent, Rect r, int16_t len) :
	NewGump(NewGumpId(8, id), parent, EVENT_PRIVATE)
{
	rect = r;
	text = new char[len];
	*text = 0;
	lastText = new char[len];
	*lastText = 0;
	if (!text || !lastText)
		outOfMemory(__FILE__, 72);	// __LINE__
	list = 0;
	textEdit = new TextEditGump(0, this, text, len, rect.width(), 0, 0);
	textEdit->f_1d |= 0x20;
	Rect listRect(0, 0, rect.width(), rect.height() - textEdit->rect.height());
	listBox = new ListBoxGump(0, this, listRect, list, 1, -1, -1);
	listBox->moveto(0, textEdit->rect.f_06);
}

ComboBox::~ComboBox(void)
{
	delete text;
	delete lastText;
}

void ComboBox::commandPrivate(Event &event)
{
	switch (event.from->newGumpId.getType())
	{
	case 6:
		if (event.data == 2 || event.data == 3)
		{
			if (!*list)
				return;
			int16_t index = 0;
			int16_t cmp;
			while ((cmp = strnicmp(text, list[index], strlen(text))) != 0 && list[index + 1])
				index++;
			if (cmp)
				strcpy(text, lastText);
			else
			{
				strcpy(lastText, text);
				listBox->setSelected(index);
			}
			textEdit->reset(0, 0);
			refresh();
			send(Event(EVENT_PRIVATE, listBox->getSelected(), 0, 1, this, parent, 0));
		}
		else
		{
			int16_t selected = listBox->getSelected();
			strcpy(text, selected < 0 ? "" : list[selected]);
			refresh();
			send(Event(EVENT_PRIVATE, listBox->getSelected(), 0, 2, this, parent, 0));
		}
		break;
	case 5:
		if (event.data == 2)
		{
			int16_t selected = listBox->getSelected();
			strcpy(text, selected < 0 ? "" : list[selected]);
			refresh();
			send(Event(EVENT_PRIVATE, listBox->getSelected(), 0, 2, this, parent, 0));
		}
		else
		{
			int16_t selected = listBox->getSelected();
			strcpy(text, selected < 0 ? "" : list[selected]);
			textEdit->reset(1, 0);
			refresh();
			send(Event(EVENT_PRIVATE, listBox->getSelected(), 0, 1, this, parent, 0));
		}
		break;
	}
}

char *ComboBox::getSelected(void)
{
	int16_t selected = listBox->getSelected();
	char *s = 0;
	if (selected != -1)
		s = list[listBox->getSelected()];
	return s;
}
