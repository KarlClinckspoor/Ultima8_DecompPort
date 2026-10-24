// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: SKIPDRAW\WRITSKP.C

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "cvport.h"
#include "writskp.h"

SkipMaker::SkipMaker(uint8_t *buffer, int16_t w, int16_t h, int16_t hotX, int16_t hotY, uint16_t flags) :
	FastSkipMaker(buffer, w, h, hotX, hotY)
{
	if (buffer)
	{
		*(int16_t *)start = flags;
	}
	firstRow = rowOffset;
	if (h)
	{
		firstRow--;
	}
	blankRow = 0;
}

// As FastSkipMaker::skip_to, but a finished row that matches an earlier one
// is dropped and shares that row's data, and blank rows share one copy.
int16_t SkipMaker::skip_to(int16_t newX, int16_t newY)
{
	int16_t size = 0;
	uint16_t i;

	if (y < newY)
	{
		if (x < width)
		{
			size += add_skip(width - x);
		}
		x = 0;
		if (y > 0)
		{
			int16_t *row = firstRow;
			uint8_t *last = (uint8_t *)(rowOffset - 1) + rowOffset[-1];
			for (i = 0; i < y - 1; i++)
			{
				uint8_t *data = (uint8_t *)row + *row;
				if (memcmp(data, last, out - last) == 0)
				{
					out = last;
					rowOffset[-1] = data - (uint8_t *)(rowOffset - 1);
					break;
				}
				row++;
			}
		}
		while (y < newY)
		{
			if (y + 1 < height)
			{
				if (newY - y > 1)
				{
					if (blankRow == 0)
					{
						blankRow = out;
						size += add_skip(width);
						x = 0;
					}
					*rowOffset++ = blankRow - (uint8_t *)rowOffset;
				}
				else
				{
					*rowOffset++ = out - (uint8_t *)rowOffset;
				}
			}
			y++;
		}
	}
	if (height > newY)
	{
		size += add_skip(newX - x);
	}
	x = newX;
	return size;
}

// True when RLE coding the run makes it smaller.
uint8_t SkipMaker::segment_can_rle(int16_t len, uint8_t *pixels)
{
	uint16_t savedX = x;
	uint16_t rleSize;
	uint16_t plainSize = measure_run(len);
	rleSize = measure_rle_run(len, pixels);
	x = savedX;
	return rleSize < plainSize;
}

// Run counts are doubled; an odd count marks a repeated pixel.
int16_t SkipMaker::add_non_rle_run(int16_t len, uint8_t *pixels)
{
	int16_t size = 0;
	x += len;
	while (len > 127)
	{
		*out++ = 127 * 2;
		memmove(out, pixels, 127);
		out += 127;
		pixels += 127;
		*out++ = 0;
		size += 129;
		len -= 127;
	}
	*out++ = len * 2;
	memmove(out, pixels, len);
	out += len;
	size += len + 1;
	return size;
}

uint16_t SkipMaker::measure_non_rle_run(int16_t len)
{
	uint16_t size = 0;
	x += len;
	while (len > 127)
	{
		size += 129;
		len -= 127;
	}
	size += len + 1;
	return size;
}

// Four or more equal pixels become a repeat; the rest stay literal.
int16_t SkipMaker::add_rle_run(int16_t len, uint8_t *pixels)
{
	int16_t i;
	int16_t color;
	int16_t pending = 0;
	int16_t repeat = 0;
	int16_t last = -1;
	uint8_t *p = pixels;
	int16_t size = 0;
	uint8_t needSkip = 0;

	for (i = 0; i < len; i++)
	{
		color = *p++;
		pending++;
		if (color == last)
		{
			if (++repeat == 4)
			{
				if (needSkip)
				{
					*out++ = 0;
					size++;
				}
				if (pending - 4 > 0)
				{
					size += add_non_rle_run(pending - 4, pixels);
					pixels += pending - 4;
					*out++ = 0;
					size++;
				}
				while (i < len - 1 && *p == color)
				{
					p++;
					repeat++;
					i++;
				}
				pending = 0;
				pixels += repeat;
				x += repeat;
				while (repeat > 127)
				{
					*out++ = 127 * 2 + 1;
					*out++ = color;
					*out++ = 0;
					size += 3;
					repeat -= 127;
				}
				*out++ = repeat * 2 + 1;
				*out++ = color;
				size += 2;
				repeat = 0;
				needSkip = 1;
			}
		}
		else
		{
			last = color;
			repeat = 1;
		}
	}
	if (pending)
	{
		if (needSkip)
		{
			*out++ = 0;
			size++;
		}
		size += add_non_rle_run(pending, pixels);
	}
	return size;
}

uint16_t SkipMaker::measure_rle_run(int16_t len, uint8_t *pixels)
{
	int16_t i;
	int16_t color;
	int16_t pending = 0;
	int16_t repeat = 0;
	int16_t last = -1;
	uint8_t *p = pixels;
	uint16_t size = 0;
	uint8_t needSkip = 0;

	for (i = 0; i < len; i++)
	{
		color = *p++;
		pending++;
		if (color == last)
		{
			if (++repeat == 4)
			{
				if (needSkip)
				{
					size++;
				}
				if (pending - 4 > 0)
				{
					size += measure_non_rle_run(pending - 4);
					size++;
				}
				pending = 0;
				while (i < len - 1 && *p == color)
				{
					p++;
					repeat++;
					i++;
				}
				x += repeat;
				while (repeat > 127)
				{
					size += 3;
					repeat -= 127;
				}
				size += 2;
				needSkip = 1;
			}
		}
		else
		{
			last = color;
			repeat = 1;
		}
	}
	if (pending)
	{
		if (needSkip)
		{
			size++;
		}
		size += measure_non_rle_run(pending);
	}
	return size;
}

// Turns an unused RLE table back into plain counts and clears its flag.
void SkipMaker::optimize(void)
{
	uint8_t *p;
	uint8_t *lastRow = start;

	*(int16_t *)start &= ~SKIP_RLE;
	for (uint16_t i = 0; i < height; i++)
	{
		p = (uint8_t *)(firstRow + i) + firstRow[i];
		// Shared rows are converted once.
		if (p > lastRow)
		{
			lastRow = p;
			x = 0;
			while (x < width)
			{
				x += *p++;
				if (x < width)
				{
					int16_t n = *p >> 1;
					*p++ = n;
					x += n;
					p += n;
				}
			}
		}
	}
}

uint16_t EstimateWriteSkip(Vport &vport, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
	int16_t skipColor, uint16_t flags)
{
	int16_t width = x2 - x1 + 1;
	int16_t height = y2 - y1 + 1;
	ShapeScanner scanner(vport, x1, y1, x2, y2, skipColor);
	SkipMaker maker(0, width, height, 0, 0, flags);
	int32_t size = height * 2 + 10L;

	while (scanner.find_segment())
	{
		size += maker.measure_skip_to(scanner.x - scanner.left, scanner.y - scanner.top);
		if (flags & SKIP_RLE)
		{
			if (maker.segment_can_rle(scanner.runLength, scanner.pos))
			{
				size += maker.measure_rle_run(scanner.runLength, scanner.pos);
			}
			else
			{
				size += maker.measure_non_rle_run(scanner.runLength);
			}
		}
		else
		{
			size += maker.measure_run(scanner.runLength);
		}
	}
	size += maker.measure_skip_to(0, height);
	if (size > 0xFFFF)
	{
		size = 0xFFFF;
	}
	return size;
}

int16_t WriteSkip(Vport &vport, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t hotX, int16_t hotY,
	uint8_t *buffer, int16_t skipColor, uint16_t flags)
{
	int16_t width = x2 - x1 + 1;
	int16_t height = y2 - y1 + 1;
	uint8_t usedRle = 0;
	ShapeScanner scanner(vport, x1, y1, x2, y2, skipColor);
	SkipMaker maker(buffer, width, height, hotX - x1, hotY - y1, flags);

	while (scanner.find_segment())
	{
		maker.skip_to(scanner.x - scanner.left, scanner.y - scanner.top);
		if (flags & SKIP_RLE)
		{
			if (maker.segment_can_rle(scanner.runLength, scanner.pos))
			{
				maker.add_rle_run(scanner.runLength, scanner.pos);
				usedRle = 1;
			}
			else
			{
				maker.add_non_rle_run(scanner.runLength, scanner.pos);
			}
		}
		else
		{
			maker.add_run(scanner.runLength, scanner.pos);
		}
	}
	maker.skip_to(0, height);
	if ((flags & SKIP_RLE) && !usedRle)
	{
		maker.optimize();
	}
	return maker.out - maker.start;
}
