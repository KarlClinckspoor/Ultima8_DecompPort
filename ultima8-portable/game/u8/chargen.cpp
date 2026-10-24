// flags: -P -2 -Z -G -k-
// name: chargen.c

#include "u8port.h"

// #include <dos.h>  (DOS only)
// #include <phapi.h>  (DOS only)
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "error.h"
#include "chargen.h"

extern "C" uint8_t *getRomFontPtr(int16_t);

uint16_t textBufferSeg;
CharacterGenerator charGen;

CharacterGenerator::CharacterGenerator(void)
{
	uint16_t seg;

	smallFont = 0;
	font = getRomFontPtr(2);
	fgColor = 15;
	bgColor = 0;
	DosMapRealSeg(0xb800, 8000L, &textBufferSeg);
	DosMapRealSeg(0xa000, 64000L, &seg);
	setSeg(seg);
}

void CharacterGenerator::setSeg(uint16_t seg)
{
	screen = (uint8_t *)MK_FP(seg, 0);
}

// Glyphs are 8x14; the small font keeps every other column and drops the top and bottom rows.
void CharacterGenerator::writeChar(int16_t x, int16_t y, char c)
{
	int16_t row = 14;
	uint8_t *glyph = font + c * 14;
	uint8_t *dest = screen + x + y * 320;

	while (row--)
	{
		uint8_t bits = *glyph++;
		uint8_t mask = 0x80;
		if (smallFont && (row == 13 || row == 0))
			continue;
		while (mask)
		{
			if (bits & mask)
				*dest++ = fgColor;
			else
				*dest++ = bgColor;
			if (smallFont)
				mask >>= 2;
			else
				mask >>= 1;
		}
		if (smallFont)
			dest += 316;
		else
			dest += 312;
	}
}

void CharacterGenerator::writeString(int16_t x, int16_t y, char *s)
{
	int16_t startX = x;
	int16_t startY = y;
	int16_t line = 0;

	while (*s)
	{
		if (*s == '\n')
		{
			line++;
			x = startX;
			y = startY;
			if (smallFont)
				y += line * 12;
			else
				y += line * 14;
			s++;
		}
		else
		{
			writeChar(x, y, *s++);
			if (smallFont)
				x += 4;
			else
				x += 8;
		}
	}
}

void CharacterGenerator::makeString(int16_t x, int16_t y, char *format, ...)
{
	char buffer[100];
	va_list ap;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	if (strlen(buffer) > 99)
		halt(__FILE__, 138);	// __LINE__
	writeString(x, y, buffer);
}
