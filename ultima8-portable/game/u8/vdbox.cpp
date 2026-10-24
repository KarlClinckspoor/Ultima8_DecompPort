// name: VDBOX
// Ported from VDBOX.ASM.
// Fills a rectangle of a viewport with one colour.

#include "u8port.h"

#include "cvport.h"
#include "segmem.h"

// void DrawBox(Vport far *vp, int x1, int y1, int x2, int y2, unsigned char color)
// Corners are inclusive and clipped to the viewport.
// The sizes are not checked after clipping: a box with x2 < x1 or y2 < y1 is filled with 16-bit
// counts, as the assembly did.
void DrawBox(Vport *vp, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
{
	// Clip the corners; give up when the box misses the viewport.
	if (x1 > vp->rect.f_04)
		return;
	if (x1 < vp->rect.f_00)
		x1 = vp->rect.f_00;
	if (x2 < vp->rect.f_00)
		return;
	if (x2 > vp->rect.f_04)
		x2 = vp->rect.f_04;
	if (y1 > vp->rect.f_06)
		return;
	if (y1 < vp->rect.f_02)
		y1 = vp->rect.f_02;
	if (y2 < vp->rect.f_02)
		return;
	if (y2 > vp->rect.f_06)
		y2 = vp->rect.f_06;

	// Each row is stored as words plus an odd byte.
	uint16_t wide = x2 - x1 + 1;
	uint16_t oddByte = wide & 1;
	uint16_t words = wide >> 1;

	// The row pitch is the distance between the first two row offsets.
	uint16_t *ytable = vp->graphicYtable.index;
	uint16_t rowSkip = ytable[1] - ytable[0] - wide;
	uint16_t di = ytable[y1] + x1;
	uint16_t rows = y2 - y1 + 1;
	uint16_t value = color | color << 8;
	do
	{
		SegStos(vp->mem, di, value, words, 2);
		SegStos(vp->mem, di, value, oddByte, 1);
		di += rowSkip;
	} while (--rows != 0);
}
