// name: SCALE
// Ported from SCALE.ASM.
// Hand-written shrinking draw of a shape frame into a viewport rectangle.
// extern "C" void scale(Vport *dest, int x1, int y1, int x2, int y2, void *frame)
//
// Each row of the frame that is drawn is first decoded into lineBuffer, with
// 0xff where nothing is drawn (so a frame pixel of colour 0xff is not drawn
// either), then every xStep-th pixel of it goes to the screen. Target rows
// take every yStep-th row of the frame, target columns every xStep-th column;
// the remainders of the divisions add one more to the first rows and columns.
// A frame smaller than the rectangle is drawn at its own size.
//
// The screen is addressed as es was: by 16-bit offsets from dest->mem (64K
// long, as Vport::alloc makes every buffer). Rows are taken to be the clip
// rectangle's width apart and to start at offset 0, as the assembly took them;
// it does not use the viewport's row table.

#include "u8port.h"

#include "cvport.h"
#include "ferror.h"
#include "segmem.h"
#include "skipdraw.h"

// One decoded line of the frame (DGROUP:2B52h). DOS reserved 324 bytes, which
// hold a frame up to 322 pixels wide (the fill writes srcW / 2 + 1 words, the
// decode up to the end of the row's last run); a wider frame ran over into the
// data after it. The buffer is 64K here, all that the 16-bit positions can
// reach, so such a frame stays inside it. (The widest frame in the game's
// shapes is 210 pixels.)
static uint8_t lineBuffer[0x10000];

void scale(Vport *dest, int16_t x1, int16_t y1, int16_t x2, int16_t y2, const SKIPFRAME *frame)
{
	const uint8_t *fp = (const uint8_t *)frame;
	int16_t vpX1 = dest->rect.f_00;
	int16_t vpY1 = dest->rect.f_02;
	int16_t vpX2 = dest->rect.f_04;
	int16_t vpY2 = dest->rect.f_06;
	uint16_t srcW, srcH;	// frame size
	uint16_t dstW, dstH;	// target size
	uint16_t xStep, yStep;	// source pixels per target pixel
	uint16_t xExtra, yExtra;	// remainders of those divisions
	uint16_t xCount, xExtraLeft, yCount;
	int16_t clipL, clipT;	// target pixels clipped off each side
	int16_t visW, visH;
	int16_t skipL, leftW;
	uint16_t rowStart, pitch;
	uint8_t compressed;
	uint8_t *es;
	uint16_t di, cx;
	const uint8_t *si;
	const uint8_t *src;
	uint16_t pos, end;
	uint8_t al;

	if (x2 < vpX1)
		return;
	if (x1 > vpX2)
		return;
	if (y2 < vpY1)
		return;
	if (y1 > vpY2)
		return;
	pitch = vpX2 - vpX1 + 1;
	clipL = 0;
	clipT = 0;
	visW = x2 - x1 + 1;
	visH = y2 - y1 + 1;
	if (!(x1 > vpX1))
	{
		clipL = vpX1 - x1;
		visW -= clipL;
	}
	if (!(vpX2 > x2))
		visW -= (int16_t)(x2 - vpX2);
	if (!(y1 > vpY1))
	{
		clipT += (int16_t)(vpY1 - y1);
		visH -= (int16_t)(vpY1 - y1);
	}
	if (!(vpY2 > y2))
		visH -= (int16_t)(y2 - vpY2);

	compressed = fp[0];	// the low byte of the compression word
	srcW = frame->sfWidth;
	srcH = frame->sfHeight;
	dstW = x2 - x1 + 1;
	dstH = y2 - y1 + 1;

	// div: unsigned, dx:ax = 0:srcW. A divisor of 0 (x2 = x1 - 1) was a
	// divide error in DOS, which ended the game.
	if (dstW == 0 || dstH == 0)
		Fatal("scale: divide by zero\r\n");
	if (srcW / dstW == 0)
	{
		xStep = 1;
		xExtra = 0;
		dstW = frame->sfWidth;
	}
	else
	{
		xStep = srcW / dstW;
		xExtra = srcW % dstW;
	}
	if (srcH / dstH == 0)
	{
		yStep = 1;
		yExtra = 0;
		dstH = frame->sfHeight;
	}
	else
	{
		yStep = srcH / dstH;
		yExtra = srcH % dstH;
	}

	es = dest->mem;
	di = Mul16(y1, pitch);
	di += x1;
	si = fp + 0x0a;	// the row table
	do
	{
		yCount = yStep;
		skipL = clipL;
		leftW = visW;
		rowStart = di;
		// Pass over all but the last of this target row's source rows.
		while (--yCount != 0)
			si += 2;
		if (yExtra != 0)
		{
			yExtra--;
			si += 2;
		}
		if (clipT != 0)
		{
			clipT--;
			goto nextRow;
		}
		if (visH == 0)
			return;
		visH--;

		// Decode the row into lineBuffer, 0xff where nothing is drawn.
		src = SkipFrameRowData(si);
		pos = 0;
		end = srcW;
		memset(lineBuffer, 0xff, (uint16_t)((srcW >> 1) + 1) * 2);
		for (;;)
		{
			pos += *src++;
			if (!((int16_t)pos < (int16_t)end))
				break;
			cx = *src++;
			if (compressed == 1 && (cx & 1))
			{
				// rcr after cmp found compressed == 1: the carry shifted in is 0.
				cx >>= 1;
				al = *src++;
				while (cx--)
					lineBuffer[pos++] = al;
			}
			else
			{
				if (compressed == 1)
					cx >>= 1;
				while (cx--)
					lineBuffer[pos++] = *src++;
			}
			if (!((int16_t)pos < (int16_t)end))
				break;
		}

		// Take every xStep-th pixel; skipL target pixels are clipped off on
		// the left, and the row ends after leftW drawn ones or dstW in all.
		pos = 0;
		cx = dstW;
		xExtraLeft = xExtra;
		for (;;)
		{
			xCount = xStep;
			do
				al = lineBuffer[pos++];
			while (--xCount);
			if (xExtraLeft != 0)
			{
				xExtraLeft--;
				al = lineBuffer[pos++];
			}
			if (al != 0xff)
			{
				if (skipL != 0)
				{
					skipL--;
					di++;
				}
				else
				{
					es[di++] = al;
					if (--leftW == 0)
						break;
				}
				if (--cx == 0)
					break;
				continue;
			}
			// A pixel with nothing drawn: passed over, and still counted
			// against leftW once clipping on the left is done.
			if (skipL != 0)
			{
				skipL--;
				di++;
				if (--cx == 0)
					break;
				continue;
			}
			di++;
			if (--cx == 0)
				break;
			if (leftW == 0)
				continue;
			if (--leftW == 0)
				break;
		}
	nextRow:
		si += 2;
		di = rowStart + pitch;
	}
	while (--dstH != 0);
}
