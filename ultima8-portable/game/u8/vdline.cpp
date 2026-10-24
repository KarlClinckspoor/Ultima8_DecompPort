// name: VDLINE
// Ported from VDLINE.ASM.
// Draws a clipped straight line in a viewport.

#include "u8port.h"

#include "cvport.h"
#include "segmem.h"

// The similar-triangles step of the clipping: a * b / c in 16-bit signed arithmetic (imul
// then idiv: the 32-bit product divided, the quotient truncated toward zero). Every operand
// is a 16-bit difference. Where idiv's quotient would not fit 16 bits the PC raised a divide
// error; that cannot happen for the ends being clipped, and here it would keep the low word.
static int16_t ClipStep(int16_t a, int16_t b, int16_t c)
{
	return (int16_t)(((int32_t)a * b) / c);
}

// void DrawLine(Vport far *vp, int x1, int y1, int x2, int y2, unsigned char color)
// Both ends are drawn. The line is clipped to the viewport by similar triangles, then
// drawn with Bresenham's stepping from its left end.
void DrawLine(Vport *vp, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
{
	uint16_t *ytable = vp->graphicYtable.index;
	uint16_t pitch = ytable[1] - ytable[0];
	uint16_t rowStep;
	uint8_t upward;
	int16_t t;

	// Make x1 the left end.
	if (x2 < x1)
	{
		t = x1, x1 = x2, x2 = t;
		t = y1, y1 = y2, y2 = t;
	}

	// A line rising to the right steps rows upward.
	int16_t top = y1;
	int16_t bottom = y2;
	if (y2 < y1)
	{
		top = y2;
		bottom = y1;
		rowStep = -pitch;
		upward = 1;
	}
	else
	{
		rowStep = pitch;
		upward = 0;
	}

	// Give up when the bounding box misses the viewport, then clip x.
	if (x2 < vp->rect.f_00 || x1 > vp->rect.f_04 || bottom < vp->rect.f_02 || top > vp->rect.f_06)
		return;
	if (x2 > vp->rect.f_04)
	{
		y2 = ClipStep(vp->rect.f_04 - x1, y2 - y1, x2 - x1) + y1;
		x2 = vp->rect.f_04;
	}
	if (x1 < vp->rect.f_00)
	{
		y1 = ClipStep(vp->rect.f_00 - x1, y2 - y1, x2 - x1) + y1;
		x1 = vp->rect.f_00;
	}

	// Clip y with the ends ordered top first.
	if (upward)
	{
		t = x1, x1 = x2, x2 = t;
		t = y1, y1 = y2, y2 = t;
	}
	if (y2 < vp->rect.f_02 || y1 > vp->rect.f_06)
		return;
	if (y2 > vp->rect.f_06)
	{
		x2 = ClipStep(vp->rect.f_06 - y1, x2 - x1, y2 - y1) + x1;
		y2 = vp->rect.f_06;
	}
	if (y1 < vp->rect.f_02)
	{
		x1 = ClipStep(vp->rect.f_02 - y1, x2 - x1, y2 - y1) + x1;
		y1 = vp->rect.f_02;
	}
	int16_t yLen = y2 - y1;

	// Back to left end first.
	if (upward)
	{
		t = x1, x1 = x2, x2 = t;
		t = y1, y1 = y2, y2 = t;
	}
	int16_t xLen = x2 - x1;

	// di at the left end.
	uint8_t *mem = vp->mem;
	uint16_t di = ytable[y1] + x1;
	uint16_t cx;
	uint16_t si;

	if (x1 == x2)
	{
		// Vertical: one pixel per row.
		cx = yLen + 1;
		uint16_t step = rowStep - 1;
		do
		{
			mem[di++] = color;
			di += step;
		} while (--cx != 0);
	}
	else if (y1 == y2)
	{
		// Horizontal: words, then the odd byte.
		cx = xLen + 1;
		uint16_t odd = cx & 1;
		SegStos(mem, di, color | color << 8, cx >> 1, 2);
		SegStos(mem, di, color, odd, 1);
	}
	else if (yLen <= xLen)
	{
		// Mostly sideways: one pixel per column, cx counts toward a row change.
		cx = (uint16_t)xLen >> 1;
		si = xLen + 1;
		for (;;)
		{
			mem[di++] = color;
			if (--si == 0)
				break;
			cx += yLen;
			if ((int16_t)cx < xLen)
				continue;
			di += rowStep;
			cx -= xLen;
		}
	}
	else
	{
		// Mostly up or down: one pixel per row, cx counts toward a column change.
		cx = (uint16_t)yLen >> 1;
		si = yLen + 1;
		rowStep--;	// the store has already moved one right
		for (;;)
		{
			mem[di++] = color;
			di += rowStep;
			if (--si == 0)
				break;
			cx += xLen;
			if ((int16_t)cx < yLen)
				continue;
			di++;
			cx -= yLen;
		}
	}
}
