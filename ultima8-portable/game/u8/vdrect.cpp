// name: VDRECT
// Ported from VDRECT.ASM.
// Draws a one-pixel rectangle outline in a viewport.

#include "u8port.h"

#include "cvport.h"
#include "segmem.h"

// void DrawRectangle(Vport far *vp, int x1, int y1, int x2, int y2, unsigned char color)
// Corners are inclusive. Each side clipped off by the viewport is left out.
// The sizes are not checked after clipping: the counts are 16-bit, as in the assembly, where
// a side of 0 pixels (y2 < y1) loops through 65536.
void DrawRectangle(Vport *vp, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
{
	uint8_t drawLeft, drawRight, drawTop, drawBottom;

	// Clip the corners, noting which sides are still visible.
	drawLeft = 1;
	if (x1 > vp->rect.f_04)
		return;
	if (x1 < vp->rect.f_00)
	{
		drawLeft = 0;
		x1 = vp->rect.f_00;
	}
	drawRight = 1;
	if (x2 < vp->rect.f_00)
		return;
	if (x2 > vp->rect.f_04)
	{
		drawRight = 0;
		x2 = vp->rect.f_04;
	}
	drawTop = 1;
	if (y1 > vp->rect.f_06)
		return;
	if (y1 < vp->rect.f_02)
	{
		drawTop = 0;
		y1 = vp->rect.f_02;
	}
	drawBottom = 1;
	if (y2 < vp->rect.f_02)
		return;
	if (y2 > vp->rect.f_06)
	{
		drawBottom = 0;
		y2 = vp->rect.f_06;
	}

	// Horizontal sides are stored as words plus an odd byte.
	uint16_t wide = x2 - x1 + 1;
	uint16_t oddByte = wide & 1;
	uint16_t words = wide >> 1;

	// Screen offsets of three corners; the row pitch comes from the first two row offsets.
	uint16_t *ytable = vp->graphicYtable.index;
	uint16_t pitch = ytable[1] - ytable[0];
	uint16_t topLeft = x1 + ytable[y1];
	uint16_t topRight = topLeft + wide - 1;
	uint16_t bottomLeft = x1 + ytable[y2];
	uint16_t high = y2 - y1 + 1;
	uint16_t value = color | color << 8;
	uint16_t di;
	uint16_t cx;

	if (drawTop)
	{
		di = topLeft;
		SegStos(vp->mem, di, value, words, 2);
		SegStos(vp->mem, di, value, oddByte, 1);
	}
	if (drawBottom)
	{
		di = bottomLeft;
		SegStos(vp->mem, di, value, words, 2);
		SegStos(vp->mem, di, value, oddByte, 1);
	}
	if (drawLeft)
	{
		di = topLeft;
		cx = high;
		do
		{
			vp->mem[di] = color;
			di += pitch;
		} while (--cx != 0);
	}
	if (drawRight)
	{
		di = topRight;
		cx = high;
		do
		{
			vp->mem[di] = color;
			di += pitch;
		} while (--cx != 0);
	}
}
