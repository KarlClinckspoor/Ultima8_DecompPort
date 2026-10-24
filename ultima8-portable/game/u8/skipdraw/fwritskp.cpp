// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: SKIPDRAW\FWRITSKP.C

#include "u8port.h"

#include "cvport.h"
#include "fwritskp.h"
#include "iwritskp.h"

// Skips to (newX, newY), writing the skips and row offsets on the way.
// y, width and height are unsigned: Borland compared them with the int arguments unsigned,
// and y + 1 and newY - y were unsigned 16-bit results. Row offsets count from the entry itself.
int16_t FastSkipMaker::skip_to(int16_t newX, int16_t newY)
{
	int16_t size = 0;
	if (y < (uint16_t)newY)
	{
		if (x < width)
		{
			size += add_skip(width - x);
		}
		x = 0;
		while (y < (uint16_t)newY)
		{
			if ((uint16_t)(y + 1) < height)
			{
				int16_t *entry = rowOffset++;
				*entry = out - (uint8_t *)entry;
			}
			y++;
			if ((uint16_t)(newY - y) == 1)
			{
				size += add_skip(width);
				x = 0;
			}
		}
	}
	if (height > (uint16_t)newY)
	{
		size += add_skip(newX - x);
	}
	x = newX;
	return size;
}

// Size of the skip table FastWriteSkip would build, at most 0xFFFF.
uint16_t EstimateFastWriteSkip(Vport &vport, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t skipColor)
{
	int16_t width = x2 - x1 + 1;
	int16_t height = y2 - y1 + 1;
	ShapeScanner scanner(vport, x1, y1, x2, y2, skipColor);
	FastSkipMaker maker(0, width, height, 0, 0);
	int32_t size = (int16_t)(height * 2) + 10L;	// height * 2 in 16 bits

	while (scanner.find_segment())
	{
		size += maker.measure_skip_to(scanner.x - scanner.left, scanner.y - scanner.top);
		size += maker.measure_run(scanner.runLength);
	}
	size += maker.measure_skip_to(0, height);
	if (size > 0xFFFF)
	{
		size = 0xFFFF;
	}
	return size;
}

// Builds the skip table of a Vport rectangle into buffer; returns its size.
int16_t FastWriteSkip(Vport &vport, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
	int16_t hotX, int16_t hotY, uint8_t *buffer, int16_t skipColor)
{
	int16_t width = x2 - x1 + 1;
	int16_t height = y2 - y1 + 1;
	ShapeScanner scanner(vport, x1, y1, x2, y2, skipColor);
	FastSkipMaker maker(buffer, width, height, hotX - x1, hotY - y1);

	while (scanner.find_segment())
	{
		maker.skip_to(scanner.x - scanner.left, scanner.y - scanner.top);
		maker.add_run(scanner.runLength, scanner.pos);
	}
	maker.skip_to(0, height);
	return maker.out - maker.start;
}
