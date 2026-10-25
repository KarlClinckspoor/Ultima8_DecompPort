// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: MISC\FVINFO.C

#include "u8port.h"

#include "crect.h"
#include "cvport.h"
#include "fvinfo.h"

U8_INLINE Point::Point(void) {}
U8_INLINE Point::Point(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE Vport::~Vport(void) { free(); }

// A shape starts with its largest frame width and depth and its frame count,
// then six bytes per frame of which the first word is the frame's offset.
struct ShapeHeader
{
	int16_t maxWidth;
	int16_t maxDepth;
	int16_t numFrames;
};

// The frame's offset is three bytes, of which only the low word is read (an
// int added to a far pointer, which wraps its 16-bit offset part at 64K). It is
// unsigned here, so an offset of 32K or more still counts forwards, and the sum
// wraps at 64K from the shape's start, as it did for a shape at offset 0 of its
// segment. (The fonts' and gumps' frames are all within 32K.)
struct FrameEntry
{
	uint16_t offset;
	int16_t unused[2];
};

static_assert(sizeof(ShapeHeader) == 6 && sizeof(FrameEntry) == 6, "shape headers keep their DOS layout");

// Each frame starts with eight bytes before its own header.
#define FRAME_ADDRESS(shape, frame) \
	((char *)(shape) + (uint16_t)(((FrameEntry *)((char *)(shape) + sizeof(ShapeHeader)))[frame].offset + 8))

int16_t GetMaxShapeWidth(void *shape)
{
	return ((ShapeHeader *)shape)->maxWidth;
}

int16_t GetMaxShapeDepth(void *shape)
{
	return ((ShapeHeader *)shape)->maxDepth;
}

int16_t GetNumShapes(void *shape)
{
	return ((ShapeHeader *)shape)->numFrames;
}

void *GetShapeAddress(void *shape, int16_t frame)
{
	return FRAME_ADDRESS(shape, frame);
}

void GetShapeInfo(char *shape, int16_t frame, int16_t &width, int16_t &height, int16_t &xOffset, int16_t &yOffset)
{
	int16_t *p = (int16_t *)FRAME_ADDRESS(shape, frame);

	width = p[1];
	height = p[2];
	xOffset = p[3];
	yOffset = p[4];
}

void TransformViewport(Vport)
{
}

void Scale(void *)
{
}

// Works out the corners of an isometric diamond from two opposite ones.
// Returns 0 when the points are too close together.
uint8_t figureDiamond(Point p1, Point p2, Point &c1, Point &c2, Point &c3, Point &c4)
{
	int16_t dx = p2.f_00 - p1.f_00;
	int16_t dy = p2.f_02 - p1.f_02;
	int16_t rise = dy << 4;
	int16_t run = dx;
	int16_t slope = 0;
	int16_t a, b;

	if (rise < 48 && rise > -48 && run < 6 && run > -6)
		return 0;
	if (run)
		slope = rise / run;
	if (slope > 8 || slope < -8 || run == 0)
	{
		if (dx & 1)
			p2.f_00--;
		if (p1.f_02 < p2.f_02)
		{
			c1 = p1;
			c3 = p2;
		}
		else
		{
			c1 = p2;
			c3 = p1;
		}
		a = c3.f_02 - (c3.f_00 >> 1);
		b = c1.f_02 + (c1.f_00 >> 1);
		c4.f_00 = b - a;
		c4.f_02 = -(c4.f_00 >> 1) + b;
		c2.f_00 = c1.f_00 + (c3.f_00 - c4.f_00);
		c2.f_02 = c1.f_02 + (c3.f_02 - c4.f_02);
	}
	else
	{
		if (!dx & 1)
			p2.f_00--;
		if (p1.f_00 < p2.f_00)
		{
			c4 = p1;
			c2 = p2;
		}
		else
		{
			c4 = p2;
			c2 = p1;
		}
		a = c2.f_02 - (c2.f_00 >> 1);
		b = c4.f_02 + (c4.f_00 >> 1);
		c1.f_00 = b - a;
		c1.f_02 = -(c1.f_00 >> 1) + b;
		c3.f_00 = c4.f_00 + (c2.f_00 - c1.f_00);
		c3.f_02 = c4.f_02 + (c2.f_02 - c1.f_02);
	}
	return 1;
}

void DrawDiamond(Vport *vport, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
{
	Point c1, c2, c3, c4;

	if (figureDiamond(Point(x1, y1), Point(x2, y2), c1, c2, c3, c4))
	{
		DrawLine(vport, c1.f_00, c1.f_02, c4.f_00, c4.f_02, color);
		DrawLine(vport, c4.f_00, c4.f_02, c3.f_00, c3.f_02, color);
		DrawLine(vport, c3.f_00, c3.f_02, c2.f_00, c2.f_02, color);
		DrawLine(vport, c2.f_00, c2.f_02, c1.f_00, c1.f_02, color);
	}
}

void DrawFilledDiamond(Vport *vport, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
{
	Point c1, c2, c3, c4;

	if (figureDiamond(Point(x1, y1), Point(x2, y2), c1, c2, c3, c4))
	{
		int16_t y, left, right, leftStep, rightStep;

		for (y = c1.f_02, left = c1.f_00, right = c1.f_00 + 1, leftStep = -2, rightStep = 2;
		     y <= c3.f_02; y++, left += leftStep, right += rightStep)
		{
			DrawLine(vport, left, y, right, y, color);
			if (y == c4.f_02)
				leftStep = 2;
			if (y == c2.f_02)
				rightStep = -2;
		}
	}
}
