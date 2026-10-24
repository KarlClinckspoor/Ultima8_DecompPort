#ifndef SKIPDRAW_H
#define SKIPDRAW_H

#include "u8port.h"

#include "crect.h"

class Vport;

// Shape frame layout and clip flags shared by the skip drawers (SKIPDRAW.INC).
//
// Each row is a list of skip/run pairs that ends once the frame's width is
// used up:  skip, run, pixels..., skip, run, pixels..., ...
// skip is a count of transparent pixels, run a count of pixels that follow.
// In a compressed frame run holds count*2 + fill; with fill set one pixel
// byte follows and is repeated count times.
//
// A frame is read straight from the shape data as stored (a shape file's frame
// from its compression word on), so the layout is the DOS one: five words, then
// one word per row, packed (u8port.h packs every game struct), little-endian.
struct SKIPFRAME
{
	uint16_t sfFlags;	// SF_COMPRESSED
	int16_t sfWidth;
	int16_t sfHeight;
	int16_t sfXOff;		// hot spot, from the frame's left
	int16_t sfYOff;		// hot spot, from the frame's top
	uint16_t sfRows[1];	// per row, offset of its data from its own entry
};

static_assert(sizeof(SKIPFRAME) == 12, "SKIPFRAME must keep its DOS layout");
static_assert(offsetof(SKIPFRAME, sfRows) == 10, "SKIPFRAME must keep its DOS layout");

#define SF_COMPRESSED	1

// Which end of each row, in stored order, the viewport cuts off.
#define CLIP_HEAD	1
#define CLIP_TAIL	2

// The row table entry of a frame's row, and the row's data (which starts
// that many bytes from the entry). Shape data is byte-aligned, so the entry
// is read a byte at a time.
inline const uint8_t *SkipFrameRowEntry(const SKIPFRAME *frame, uint16_t row)
{
	return (const uint8_t *)frame + offsetof(SKIPFRAME, sfRows) + (uint16_t)(row * 2);
}

inline const uint8_t *SkipFrameRowData(const uint8_t *entry)
{
	return entry + (uint16_t)(entry[0] | entry[1] << 8);
}

// SKIPDRAW.ASM. x, y is where the frame's hot spot lands. Returns 1 when the
// frame misses the viewport, else 0.
int16_t SkipDraw(Vport *vp, int16_t x, int16_t y, const SKIPFRAME *frame);

// FLIPSKIP.ASM. Mirrored left to right: the hot spot is sfXOff in from the
// frame's right edge.
int16_t FlipSkipDraw(Vport *vp, int16_t x, int16_t y, const SKIPFRAME *frame);

// SDXF.ASM. Each covered screen pixel p becomes remap[p].
int16_t SkipTransformDraw(Vport *vp, int16_t x, int16_t y, const SKIPFRAME *frame, const uint8_t *remap);

// SDFXF.ASM. FlipSkipDraw's placement with SkipTransformDraw's colouring.
int16_t FlipSkipTransformDraw(Vport *vp, int16_t x, int16_t y, const SKIPFRAME *frame, const uint8_t *remap);

// SDDXF.ASM. Shape pixel s over screen pixel p draws mix[mix[s] * 256 + p], or
// s itself when mix[s] is 0xff.
int16_t SkipDoubleTransformDraw(Vport *vp, int16_t x, int16_t y, const SKIPFRAME *frame, const uint8_t *mix);

// SDFDXF.ASM. FlipSkipDraw's placement with SkipDoubleTransformDraw's blending.
int16_t FlipSkipDoubleTransformDraw(Vport *vp, int16_t x, int16_t y, const SKIPFRAME *frame, const uint8_t *mix);

// SCALE.ASM. Shrinks a frame into the rectangle x1, y1 - x2, y2 of dest.
void scale(Vport *dest, int16_t x1, int16_t y1, int16_t x2, int16_t y2, const SKIPFRAME *frame);

// COLLIDE.ASM. Returns 1 when at hits a drawn pixel of the frame with its hot
// spot at origin, else 0.
int16_t Collision(const SKIPFRAME *frame, Point origin, Point at);

// The callers pass frames as the char or void pointers the shape handlers
// give out (the assembly took any far pointer); these take those.
inline int16_t SkipDraw(Vport *vp, int16_t x, int16_t y, const void *frame)
{
	return SkipDraw(vp, x, y, (const SKIPFRAME *)frame);
}

inline int16_t FlipSkipDraw(Vport *vp, int16_t x, int16_t y, const void *frame)
{
	return FlipSkipDraw(vp, x, y, (const SKIPFRAME *)frame);
}

inline int16_t SkipTransformDraw(Vport *vp, int16_t x, int16_t y, const void *frame, const uint8_t *remap)
{
	return SkipTransformDraw(vp, x, y, (const SKIPFRAME *)frame, remap);
}

inline int16_t FlipSkipTransformDraw(Vport *vp, int16_t x, int16_t y, const void *frame, const uint8_t *remap)
{
	return FlipSkipTransformDraw(vp, x, y, (const SKIPFRAME *)frame, remap);
}

inline int16_t SkipDoubleTransformDraw(Vport *vp, int16_t x, int16_t y, const void *frame, const uint8_t *mix)
{
	return SkipDoubleTransformDraw(vp, x, y, (const SKIPFRAME *)frame, mix);
}

inline int16_t FlipSkipDoubleTransformDraw(Vport *vp, int16_t x, int16_t y, const void *frame, const uint8_t *mix)
{
	return FlipSkipDoubleTransformDraw(vp, x, y, (const SKIPFRAME *)frame, mix);
}

inline void scale(void *dest, int16_t x1, int16_t y1, int16_t x2, int16_t y2, const void *frame)
{
	scale((Vport *)dest, x1, y1, x2, y2, (const SKIPFRAME *)frame);
}

inline int16_t Collision(const void *frame, Point origin, Point at)
{
	return Collision((const SKIPFRAME *)frame, origin, at);
}

#endif
