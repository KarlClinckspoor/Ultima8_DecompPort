// name: SDFXF
// Ported from SDFXF.ASM.
// Recolours the screen under a mirrored run-length shape frame through a
// table, clipped.
//
// The screen is addressed as the assembly addressed es: by 16-bit offsets from
// vp->mem (Vport::alloc gives every buffer 64K, so a wrapped offset stays in
// it). Frame bytes are read through a plain pointer.

#include "u8port.h"

#include "cvport.h"
#include "skipdraw.h"

// The remap loop (mov bl, es:[di] / mov bl, [bx+si] / mov es:[di], bl / dec di /
// dec cx / jne) over n screen pixels, right to left. As in the assembly, n = 0
// goes round 65536 times.
static inline void remapRunLeft(uint8_t *mem, uint16_t &di, const uint8_t *remap, uint16_t n)
{
	do
	{
		mem[di] = remap[mem[di]];
		di--;
	}
	while (--n);
}

// A run of n pixels of its own: passed over in the frame, remapped on screen.
static inline void remapCopyLeft(uint8_t *mem, uint16_t &di, const uint8_t *&si, const uint8_t *remap, uint16_t n)
{
	si += n;
	remapRunLeft(mem, di, remap, n);
}

// A run of one colour: its fill byte is passed over, n pixels remapped.
static inline void remapFillLeft(uint8_t *mem, uint16_t &di, const uint8_t *&si, const uint8_t *remap, uint16_t n)
{
	si++;
	remapRunLeft(mem, di, remap, n);
}

// int FlipSkipTransformDraw(Vport far *vp, int x, int y, SKIPFRAME far *frame,
//                           unsigned char far *remap)
// FlipSkipDraw's placement with SkipTransformDraw's colouring: each covered
// screen pixel p becomes remap[p]. Returns 1 when the frame misses the
// viewport, else 0.
int16_t FlipSkipTransformDraw(Vport *vp, int16_t x, int16_t y, const SKIPFRAME *frame, const uint8_t *remap)
{
	int16_t x1 = vp->rect.f_00;
	int16_t y1 = vp->rect.f_02;
	int16_t x2 = vp->rect.f_04;
	int16_t y2 = vp->rect.f_06;
	uint8_t *mem = vp->mem;
	uint16_t *ytable = vp->graphicYtable.index;
	uint16_t pitch, skipRows, rows, cols, skipLeft, clip, skipRight;
	int16_t ax, old, hidden, flipOff;
	uint16_t cx, n, di, d, run;
	const uint8_t *bx;
	const uint8_t *si;

	// The frame's left edge is width - 1 - sfXOff left of x.
	flipOff = frame->sfWidth - 1 - frame->sfXOff;
	ax = x - flipOff;
	if (ax > x2)
		return 1;
	ax = ax + frame->sfWidth - 1;
	if (ax < x1)
		return 1;
	ax = y - frame->sfYOff;
	if (ax > y2)
		return 1;
	ax = ax + frame->sfHeight - 1;
	if (ax < y1)
		return 1;

	// Trim the frame to the clip rectangle. A row starts at the right, so the
	// left edge cuts the tail of the stored row and the right edge its head.
	rows = frame->sfHeight;
	cols = frame->sfWidth;
	skipRows = 0;
	skipLeft = 0;
	clip = 0;
	skipRight = 0;	// left unset by the assembly; only read when CLIP_HEAD is set
	ax = y - frame->sfYOff;
	if (ax < y1)
	{
		skipRows = y1 - ax;
		rows -= skipRows;
	}
	ax = ax + frame->sfHeight - 1;
	if (ax > y2)
		rows -= (uint16_t)(ax - y2);
	ax = x - flipOff;
	if (ax < x1)
	{
		skipLeft = x1 - ax;	// kept as the assembly kept it; nothing reads it
		cols -= skipLeft;
		clip |= CLIP_TAIL;
	}
	ax = ax + frame->sfWidth - 1;
	if (ax > x2)
	{
		ax -= x2;
		cols -= ax;
		skipRight = ax;
		clip |= CLIP_HEAD;
	}

	// di = screen offset of the first drawn row's left edge; each path moves
	// it to the right edge, where the row starts.
	pitch = ytable[1] - ytable[0];
	di = ytable[(uint16_t)(y - frame->sfYOff + skipRows)];
	di = di - flipOff + x;
	bx = SkipFrameRowEntry(frame, skipRows);
	di = di + frame->sfWidth - 1;

	if (clip == 0)
	{
		// Whole rows.
		if (!(frame->sfFlags & SF_COMPRESSED))
		{
			do
			{
				ax = cols;
				si = SkipFrameRowData(bx);
				bx += 2;
				d = di;
				for (;;)
				{
					cx = *si++;
					ax -= cx;
					if (ax == 0)
						break;
					d -= cx;
					n = *si++;
					remapCopyLeft(mem, d, si, remap, n);
					ax -= n;
					if (ax == 0)
						break;
				}
				di += pitch;
			}
			while (--rows);
			return 0;
		}
		do
		{
			ax = cols;
			si = SkipFrameRowData(bx);
			bx += 2;
			d = di;
			for (;;)
			{
				cx = *si++;
				ax -= cx;
				if (ax == 0)
					break;
				d -= cx;
				cx = *si++;
				n = cx >> 1;
				if (cx & 1)
					remapFillLeft(mem, d, si, remap, n);
				else
					remapCopyLeft(mem, d, si, remap, n);
				ax -= n;
				if (ax == 0)
					break;
			}
			di += pitch;
		}
		while (--rows);
		return 0;
	}

	// Clipped rows: runs are trimmed to cols pixels, starting skipRight in.
	if (!(frame->sfFlags & SF_COMPRESSED))
	{
		if (clip != CLIP_TAIL)
		{
			// Raw rows cut at the start (and maybe the end).
			do
			{
				ax = cols;
				si = SkipFrameRowData(bx);
				bx += 2;
				hidden = skipRight;
				d = di - skipRight;
				// Walk the hidden part; hidden counts the pixels still to pass.
				for (;;)
				{
					cx = *si++;
					old = hidden;
					hidden -= cx;
					if (hidden == 0)
						goto rawHeadRun;
					if (old < (int16_t)cx)
					{
						// The edge is inside this skip.
						cx = -hidden;
						goto rawHeadGap;
					}
					cx = *si++;
					si += cx;
					old = hidden;
					hidden -= cx;
					if (hidden == 0)
						goto rawHeadSkip;
					if ((uint16_t)old < cx)
					{
						// The edge is inside this run: back up to its first
						// visible pixel.
						si -= cx;
						hidden += cx;
						cx -= hidden;
						si += hidden;
						goto rawHeadCopy;
					}
				}
			rawHeadSkip:
				cx = *si++;
			rawHeadGap:
				if (!(ax > (int16_t)cx))
					cx = ax;
				ax -= cx;
				if (ax == 0)
					goto rawHeadRowDone;
				d -= cx;
			rawHeadRun:
				cx = *si++;
			rawHeadCopy:
				if (!(ax > (int16_t)cx))
					cx = ax;
				n = cx;
				remapCopyLeft(mem, d, si, remap, n);
				ax -= n;
				if (ax != 0)
					goto rawHeadSkip;
			rawHeadRowDone:
				di += pitch;
			}
			while (--rows);
			return 0;
		}

		// Raw rows cut at the end only.
		do
		{
			ax = cols;
			si = SkipFrameRowData(bx);
			bx += 2;
			d = di;
			for (;;)
			{
				cx = *si++;
				old = ax;
				ax -= cx;
				if (old <= (int16_t)cx)
					break;
				d -= cx;
				cx = *si++;
				if (!(ax > (int16_t)cx))
					cx = ax;
				n = cx;
				remapCopyLeft(mem, d, si, remap, n);
				ax -= n;
				if (ax == 0)
					break;
			}
			di += pitch;
		}
		while (--rows);
		return 0;
	}

	if (clip != CLIP_TAIL)
	{
		// Compressed rows cut at the start (and maybe the end).
		do
		{
			ax = cols;
			si = SkipFrameRowData(bx);
			bx += 2;
			hidden = skipRight;
			d = di - skipRight;
			// Walk the hidden part; run keeps the run byte for its fill bit.
			for (;;)
			{
				cx = *si++;
				old = hidden;
				hidden -= cx;
				if (hidden == 0)
					goto packHeadRun;
				if (old < (int16_t)cx)
				{
					cx = -hidden;
					goto packHeadGap;
				}
				cx = *si;
				run = cx;
				si += 2;	// past the run byte and, for a fill, its colour
				cx >>= 1;
				if (!(run & 1))
				{
					si--;
					si += cx;
				}
				old = hidden;
				hidden -= cx;
				if (hidden == 0)
					goto packHeadSkip;
				if ((uint16_t)old < cx)
				{
					// The edge is inside this run.
					si--;
					if (run & 1)
					{
						// si is on the fill byte.
						hidden += cx;
						cx -= hidden;
						goto packHeadFill;
					}
					si++;
					si -= cx;
					hidden += cx;
					cx -= hidden;
					si += hidden;
					goto packHeadCopy;
				}
			}
		packHeadSkip:
			cx = *si++;
		packHeadGap:
			if (!(ax > (int16_t)cx))
				cx = ax;
			ax -= cx;
			if (ax == 0)
				goto packHeadRowDone;
			d -= cx;
		packHeadRun:
			cx = *si++;
			run = cx;
			cx >>= 1;
			if (!(run & 1))
				goto packHeadCopy;
		packHeadFill:
			if (!(ax > (int16_t)cx))
				cx = ax;
			n = cx;
			remapFillLeft(mem, d, si, remap, n);
			goto packHeadNext;
		packHeadCopy:
			if (!(ax > (int16_t)cx))
				cx = ax;
			n = cx;
			remapCopyLeft(mem, d, si, remap, n);
		packHeadNext:
			ax -= n;
			if (ax != 0)
				goto packHeadSkip;
		packHeadRowDone:
			di += pitch;
		}
		while (--rows);
		return 0;
	}

	// Compressed rows cut at the end only.
	do
	{
		ax = cols;
		si = SkipFrameRowData(bx);
		bx += 2;
		d = di;
		for (;;)
		{
			cx = *si++;
			old = ax;
			ax -= cx;
			if (old <= (int16_t)cx)
				break;
			d -= cx;
			cx = *si++;
			run = cx;
			cx >>= 1;
			if (!(ax > (int16_t)cx))
				cx = ax;
			n = cx;
			if (run & 1)
				remapFillLeft(mem, d, si, remap, n);
			else
				remapCopyLeft(mem, d, si, remap, n);
			ax -= n;
			if (ax == 0)
				break;
		}
		di += pitch;
	}
	while (--rows);
	return 0;
}
