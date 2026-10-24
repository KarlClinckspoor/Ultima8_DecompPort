// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: KTEXTED.C

#include "u8port.h"

#include <string.h>
// #include <mem.h>  (DOS only)
#include <ctype.h>
#include "newgump.h"
#include "crect.h"
#include "cvport.h"
#include "cglobvp.h"
#include "cafont.h"
#include "dispatch.h"
#include "menu.h"
#include "ktexted.h"

// Inline in the shared headers when this file was built.
inline Point::Point(void) {}
inline Rect::Rect(void) {}
inline NewGumpId::NewGumpId(uint8_t type, uint8_t id) { instance = (type << 8) | id; f_02 = ~instance; f_04 = 0; }

// Set while a save description is being edited and nothing has been typed.
uint8_t IDidNotTypeInAnything;

// A four-line, 25-column editor over a caller's 104-byte buffer.
KillerTextEditGump::KillerTextEditGump(uint8_t id, NewGump *owner, char *buffer, Rect r) :
	NewGump(NewGumpId(6, id), owner, 0x22)
{
	IDidNotTypeInAnything = 1;
	setExclusive(0);
	text = buffer;
	backup = new char[104];
	memcpy(backup, text, 104);
	rect = r;
	font = new CacheFont((FontType)4);
	line = col = 0;
	for (int16_t i = 0; i < 4; i++)
		if (backup[i * 26])
		{
			line = i;
			col = strlen(backup + i * 26);
		}
	if (line || col)
		IDidNotTypeInAnything = 0;
	registerHotKeys(0xffff, 0);
	parent->restore();
}

KillerTextEditGump::~KillerTextEditGump(void)
{
	if (backup)
		delete backup;
	if (font)
		delete font;
}

void KillerTextEditGump::clearText(void)
{
	for (int16_t i = 0; i < 104; i++)
		text[i] = 0;
}

void KillerTextEditGump::nullLine(int16_t l)
{
	nullToEnd(l, 0);
}

int16_t KillerTextEditGump::getLen(int16_t l)
{
	int16_t len = 0;
	while (text[l * 26 + len])
		len++;
	return len;
}

// Pads line `l` with spaces out to column `c`.
void KillerTextEditGump::insertTo(int16_t l, int16_t c)
{
	int16_t len = getLen(l);
	while (len < c)
	{
		text[l * 26 + len] = ' ';
		len++;
	}
	text[l * 26 + len] = 0;
}

int16_t KillerTextEditGump::findLastSpace(int16_t l)
{
	int16_t len = getLen(l);
	if (!len)
		return 0;
	for (; len; len--)
		if (text[l * 26 + len] == ' ')
		{
			for (len--; len; len--)
				if (text[l * 26 + len] != ' ')
					return ++len;
			return 0;
		}
	return 0;
}

int16_t KillerTextEditGump::findFirstSpace(int16_t l)
{
	int16_t i = 0;
	int16_t len = getLen(line);
	for (; i < len; i++)
		if (text[l * 26 + i] == ' ')
			for (i++; i; i++)
				if (text[l * 26 + i] != ' ')
					return --i;
	return 0;
}

// Adds a character at the cursor, wrapping the last word onto the next line.
void KillerTextEditGump::output(char c)
{
	if (col >= 25)
	{
		int16_t space = findLastSpace(line);
		int16_t j = 0;
		if (space)
		{
			if (line == 3)
				return;
			int16_t wrap = space;
			space++;
			if (c == ' ')
			{
				nullToEnd(line, col);
				line++;
				col = 0;
				return;
			}
			for (; space != 25; space++, j++)
				text[line * 26 + j + 26] = text[line * 26 + space];
			nullToEnd(line, wrap);
		}
		else if (line == 3)
			return;
		text[line * 26 + col] = 0;
		line++;
		col = j;
	}
	text[line * 26 + col++] = c;
	IDidNotTypeInAnything = 0;
}

void KillerTextEditGump::deleteChar(int16_t l, int16_t c)
{
	if (c < 25)
	{
		int16_t i;	// Borland kept a for loop's variable after the loop
		for (i = col; i < 25; i++)
			text[l * 26 + i] = text[l * 26 + i + 1];
		text[l * 26 + i] = 0;
		printLine(l);
	}
}

void KillerTextEditGump::printLine(int16_t)
{
}

void KillerTextEditGump::printText(void)
{
	for (int16_t i = 0; i < 4; i++)
		printLine(i);
}

void KillerTextEditGump::nullToEnd(int16_t l, int16_t c)
{
	for (int16_t i = c; i < 26; i++)
		text[l * 26 + i] = 0;
}

void KillerTextEditGump::commandMouseLeft(Event &event)
{
	if (event.isDouble())
	{
		clrExclusive();
		send(Event(EVENT_PRIVATE, 0, 0, 32, this, parent, 0));
		killMyself();
	}
}

void KillerTextEditGump::commandKeyboard(Event &event)
{
	int16_t key = event.data;
	switch (key)
	{
	case 27:
		restore();
		clrExclusive();
		send(Event(EVENT_PRIVATE, 0, 0, 31, this, parent, 0));
		memcpy(text, backup, 104);
		killMyself();
		return;
	case 13:
		restore();
		clrExclusive();
		send(Event(EVENT_PRIVATE, 0, 0, 32, this, parent, 0));
		killMyself();
		return;
	case 8:
		if (--col < 0)
		{
			if (--line < 0)
			{
				line++;
				col = 0;
			}
			else
			{
				col = getLen(line);
				deleteChar(line, col);
			}
		}
		else
			deleteChar(line, col);
		break;
	case 0x14d:		// Right
		if (++col >= 26)
		{
			col = 0;
			if (++line == 4)
				line--;
		}
		break;
	case 0x14b:		// Left
		if (--col < 0)
		{
			if (--line < 0)
			{
				line++;
				col = 0;
			}
			else
				col = 25;
		}
		break;
	case 0x148:		// Up
		if (--line < 0)
			line++;
		break;
	case 0x150:		// Down
		if (++line >= 4)
			line--;
		break;
	case 0x147:		// Home
		col = 0;
		break;
	case 0x14f:		// End
		col = getLen(line);
		break;
	case 9:
		col = col / 4 * 4;
		col += 4;
		if (col >= 26)
			col = 26;
		break;
	default:
		if (isalnum(key) || ispunct(key) || key == ' ')
		{
			if (getLen(line) < col)
				;
			insertTo(line, col);
			output(key);
		}
	}
	parent->restore();
}

void KillerTextEditGump::draw(int16_t x, int16_t y)
{
	for (int16_t i = 0; i < 4; i++)
		font->printf(x, y + 17 + i * 6, text + i * 26);
	DrawRectangle(GlobalVport::global_ptr, x + col * 4, y + line * 6 + 16, x + col * 4 + 3, y + line * 6 + 20, GumpColorMap[0]);
}

UnkSymbolEditor::UnkSymbolEditor(uint8_t id, NewGump *parent, char *buffer, uint8_t size, uint8_t width) :
	TextEditGump(id, parent, buffer, size, width, 0, 0)
{
}

void UnkSymbolEditor::commandKeyboard(Event &event)
{
	if (isalnum(event.data) || event.data == 8)
		TextEditGump::commandKeyboard(event);
}

InputInt::InputInt(uint8_t id, NewGump *parent, char *buffer) :
	TextEditGump(id, parent, buffer, 5, 40, 0, 0)
{
}

void InputInt::commandKeyboard(Event &event)
{
	if (event.data < '0' || event.data > '9')
		return;
	TextEditGump::commandKeyboard(event);
}
