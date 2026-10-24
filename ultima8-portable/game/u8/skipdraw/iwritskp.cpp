// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: SKIPDRAW\IWRITSKP.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
// #include <mem.h>  (DOS only)
#include "cvport.h"
#include "iwritskp.h"

ShapeScanner::ShapeScanner(Vport &vport, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color)
{
	pos = (uint8_t *)MK_FP(vport.seg, ((int16_t *)vport.graphicYtable.index)[y1] + x1);
	lineSkip = (vport.rect.f_04 - vport.rect.f_00 + 1) - (x2 - x1 + 1);
	runLength = 0;
	x = left = x1;
	y = top = y1;
	right = x2;
	bottom = y2;
	skipColor = color;
}

// Moves past the last run to the next one; its length goes in runLength.
uint8_t ShapeScanner::find_segment(void)
{
	x += runLength;
	pos += runLength;
	runLength = 0;
	while (y <= bottom)
	{
		while (x <= right)
		{
			if (*pos != skipColor)
			{
				uint8_t *p = pos;
				int16_t i = x;
				while (i <= right && *p++ != skipColor)
				{
					i++;
					runLength++;
				}
				return 1;
			}
			x++;
			pos++;
		}
		x = left;
		y++;
		pos += lineSkip;
	}
	return 0;
}

FastSkipMaker::FastSkipMaker(uint8_t *buffer, int16_t w, int16_t h, int16_t hotX, int16_t hotY)
{
	if (w < 0 || h < 0)
	{
		w = h = 0;
	}
	width = w;
	height = h;
	rowOffset = (int16_t *)(start = buffer);
	out = buffer + h * 2 + 10;
	if (buffer)
	{
		*rowOffset++ = 0;
		*rowOffset++ = w;
		*rowOffset++ = h;
		*rowOffset++ = hotX;
		*rowOffset++ = hotY;
		if (h)
		{
			*rowOffset++ = out - (uint8_t *)rowOffset;
		}
	}
	x = y = 0;
}

int16_t FastSkipMaker::add_run(int16_t len, uint8_t *pixels)
{
	int16_t size = 0;
	x += len;
	while (len > 255)
	{
		*out++ = 255;
		memmove(out, pixels, 255);
		out += 255;
		pixels += 255;
		*out++ = 0;
		size += 257;
		len -= 255;
	}
	*out++ = len;
	memmove(out, pixels, len);
	out += len;
	size += len + 1;
	return size;
}

uint16_t FastSkipMaker::measure_run(int16_t len)
{
	uint16_t size = 0;
	x += len;
	while (len > 255)
	{
		size += 257;
		len -= 255;
	}
	size += len + 1;
	return size;
}

int16_t FastSkipMaker::add_skip(int16_t len)
{
	int16_t size = 0;
	x += len;
	while (len > 255)
	{
		*out++ = 255;
		*out++ = 0;
		size += 2;
		len -= 255;
	}
	*out++ = len;
	size++;
	return size;
}

uint16_t FastSkipMaker::measure_skip(int16_t len)
{
	uint16_t size = 0;
	x += len;
	while (len > 255)
	{
		size += 2;
		len -= 255;
	}
	size++;
	return size;
}

uint16_t FastSkipMaker::measure_skip_to(int16_t newX, int16_t newY)
{
	uint16_t size = 0;
	if (y < newY)
	{
		if (x < width)
		{
			size += measure_skip(width - x);
		}
		x = 0;
		while (y < newY)
		{
			y++;
			if (newY - y == 1)
			{
				size += measure_skip(width);
				x = 0;
			}
		}
	}
	if (height > newY)
	{
		size += measure_skip(newX - x);
	}
	x = newX;
	return size;
}
