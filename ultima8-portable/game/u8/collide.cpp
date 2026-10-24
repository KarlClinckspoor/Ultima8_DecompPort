// name: COLLIDE
// Ported from COLLIDE.ASM.
// Tests whether a point lands on a solid pixel of a shape frame.

#include "u8port.h"

#include "crect.h"
#include "skipdraw.h"

// The assembly's SHAPEFRAME (a shape frame from its compression word on, each
// line offset counting from its own slot) is SKIPFRAME.

// int Collision(ShapeFrame far *frame, int originX, int originY, int x, int y)
// Returns 1 when (x, y) hits a drawn pixel of the frame placed at the origin.
// The C++ callers pass the origin and the point as two Points, which went on
// the stack as the four words.
int16_t Collision(const SKIPFRAME *frame, Point origin, Point at)
{
	int16_t x = at.f_00 - origin.f_00;
	int16_t y = at.f_02 - origin.f_02;
	uint16_t compressed = frame->sfFlags;
	int16_t wide, row, col, cx, bx, dx;
	const uint8_t *si;

	// Turn the point into a line and column of the frame. jl after the add
	// tests the true sum (the sign corrected for overflow); the compare after
	// it takes the 16-bit sum.
	if ((int32_t)frame->sfYOff + y < 0)
		return 0;
	row = frame->sfYOff + y;
	if (row >= frame->sfHeight)
		return 0;
	if ((int32_t)frame->sfXOff + x < 0)
		return 0;
	col = frame->sfXOff + x;
	if (col >= frame->sfWidth)
		return 0;
	wide = frame->sfWidth;
	si = SkipFrameRowData(SkipFrameRowEntry(frame, row));

	// Walk the line: a skip count, then a run of pixels, until the column is passed.
	cx = 0;
	for (;;)
	{
		cx += *si++;
		if (cx > col)
			return 0;
		if (cx >= wide)
			return 0;
		bx = *si++;
		if (compressed & 1)
		{
			// The count is shifted out whether or not the run is a fill.
			uint16_t fill = bx & 1;
			bx = (uint16_t)bx >> 1;
			if (fill)
				dx = 1;	// a repeated run stores one colour
			else
				dx = bx;
		}
		else
			dx = bx;
		cx += bx;
		if (cx > col)
			return 1;
		si += (uint16_t)dx;
	}
}
