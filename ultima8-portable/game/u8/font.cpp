// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: FONT.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
#include <stdarg.h>
#include <stdio.h>
#include "crect.h"
#include "cvport.h"
#include "cglobvp.h"
#include "system.h"
#include "font.h"

Font *GlobalFont::main_font = 0;
Font *GlobalFont::global_ptr = 0;

Font::~Font(void)
{
}

Font::Font(void)
{
	gotoXY(0, 0);
	setXSpacing(0);
	setYSpacing(0);
}

void Font::print(uint8_t c)
{
	int16_t width, height;
	dim(c, width, height);
	drawChar(cursorX, cursorY, c);
	if (c == '\n')
	{
		cursorY += height;
		cursorX = GlobalVport::global_ptr->getRect()->f_00;
	}
	else if (c == '\r')
		cursorX = GlobalVport::global_ptr->getRect()->f_00;
	else
		cursorX += width;
}

void Font::print(char *text)
{
	while (*text)
	{
		print(*text);
		text++;
	}
}

// Prints as much of text as fits in maxWidth; returns the count printed.
int16_t Font::print(int16_t maxWidth, char *text)
{
	int16_t startX = cursorX;
	int16_t count = 0;
	int16_t width, height;
	while (*text)
	{
		dim(*text, width, height);
		if (cursorX + width > startX + maxWidth)
			break;
		print(*text);
		text++;
		count++;
	}
	return count;
}

void Font::dim(char *text, int16_t &width, int16_t &height)
{
	int16_t lineWidth = 0;
	int16_t maxWidth = 0;
	int16_t totalHeight = 0;
	int16_t charHeight = 0;
	while (*text)
	{
		dim(*text, width, charHeight);
		if (*text == '\n')
		{
			totalHeight += charHeight;
			if (lineWidth > maxWidth)
				maxWidth = lineWidth;
			lineWidth = 0;
		}
		lineWidth += width;
		text++;
	}
	totalHeight += charHeight;
	if (lineWidth > maxWidth)
		maxWidth = lineWidth;
	width = maxWidth;
	height = totalHeight;
}

int16_t Font::height(char *text)
{
	int16_t height, width;
	dim(text, width, height);
	return height;
}

int16_t Font::width(char *text)
{
	int16_t height, width;
	dim(text, width, height);
	return width;
}

// The DOS build found the argument list by hand ((va_list)(&format + 1)), to keep
// later functions in the file from getting stack frames; va_start does it on the host.

// align: 'c' centres on the screen, 'l' starts at the left edge.
void Font::printf(int16_t x, int16_t y, char align, char *format, ...)
{
	va_list args;
	if (format && *format && format != (char *)WorkString)
	{
		va_start(args, format);
		vsprintf(WorkString, format, args);
		va_end(args);
	}
	else
		*(char *)WorkString = 0;
	int16_t left;
	if (align == 'c')
		left = (320 - width(WorkString)) >> 1;
	else if (align == 'l')
		left = 0;
	else
		left = x;
	gotoXY(left, y);
	print(WorkString);
}

int16_t Font::printf(int16_t x, int16_t y, char *format, ...)
{
	va_list args;
	if (format && *format && format != (char *)WorkString)
	{
		va_start(args, format);
		vsprintf(WorkString, format, args);
		va_end(args);
	}
	else
		*(char *)WorkString = 0;
	gotoXY(x, y);
	print(WorkString);
	// Borland returned whatever print left in AX; no caller uses it.
	return 0;
}

void Font::printf(int16_t x, int16_t y, int16_t maxWidth, char *format, ...)
{
	va_list args;
	if (format && *format && format != (char *)WorkString)
	{
		va_start(args, format);
		vsprintf(WorkString, format, args);
		va_end(args);
	}
	else
		*(char *)WorkString = 0;
	gotoXY(x, y);
	print(maxWidth, WorkString);
}

void Font::printf(char *format, ...)
{
	va_list args;
	if (format && *format && format != (char *)WorkString)
	{
		va_start(args, format);
		vsprintf(WorkString, format, args);
		va_end(args);
	}
	else
		*(char *)WorkString = 0;
	print(WorkString);
}

void Font::getRect(Rect &rect, int16_t x, int16_t y, char *text)
{
	rect.f_00 = rect.f_02 = 0;
	dim(text, rect.f_04, rect.f_06);
	rect.moveto(x, y);
	rect.moverel(0, -getBaseLine());
}

void GlobalFont::setMainFont(Font *font)
{
	global_ptr = main_font = font;
}

CurrentFont::CurrentFont(Font *font)
{
	oldFont = GlobalFont::global_ptr;
	GlobalFont::global_ptr = font;
}

CurrentFont::~CurrentFont(void)
{
	GlobalFont::global_ptr = oldFont;
}

void Font::gotoXY(int16_t x, int16_t y)
{
	cursorX = x;
	cursorY = y;
}

void Font::setXSpacing(int16_t spacing)
{
	xSpacing = spacing;
}

// Writes xSpacing, as the shipped code does.
void Font::setYSpacing(int16_t spacing)
{
	xSpacing = spacing;
}

// The address of the VGA BIOS ROM font (DOS: the caller passed the font number, read
// straight off the stack); see game/port/romfont.cpp.
void *getRomFontPtr(void)
{
	return getRomFontPtr((int16_t)2);
}
