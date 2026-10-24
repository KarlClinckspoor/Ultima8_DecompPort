// name: MOTCHECK
// Ported from MOTCHECK.ASM.
// MotionTracker::check, written in assembly (.386: ENTER frame, near Jcc).
// Tests the item's footprint along a move against the collision buffer.
//
// The assembly used the tracker and the move's flags by offset; the members are used by name
// now (the offsets are checked below). Its 32-bit locals were 16-bit values sign-extended
// with cwd, and its products 16 x 16 imul, so they are int16_t values and int32_t products
// here; the quotients fit 16 bits (the distance divided is always shorter than the move).

#include "u8port.h"

#include <stddef.h>
#include "item.h"
#include "itemdata.h"
#include "type.h"
#include "colbuf.h"
#include "motrak.h"

static_assert(sizeof(TypeFlag) == 8, "TypeFlag is one 8-byte typeflag.dat record");

// The offsets the assembly used.
static_assert(offsetof(MotionTracker, volumeMask) == 0x00, "MotionTracker layout");
static_assert(offsetof(MotionTracker, surfaceMask) == 0x02, "MotionTracker layout");
static_assert(offsetof(MotionTracker, xd) == 0x04, "MotionTracker layout");
static_assert(offsetof(MotionTracker, yd) == 0x06, "MotionTracker layout");
static_assert(offsetof(MotionTracker, zd) == 0x08, "MotionTracker layout");
static_assert(offsetof(MotionTracker, floorGap) == 0x1c, "MotionTracker layout");
static_assert(offsetof(MotionTracker, blocked) == 0x23, "MotionTracker layout");
static_assert(offsetof(MotionTrackerInternalFlags, from) == 0x06, "MotionTrackerInternalFlags layout");
static_assert(offsetof(MotionTrackerInternalFlags, to) == 0x0b, "MotionTrackerInternalFlags layout");
static_assert(offsetof(MotionTrackerInternalFlags, needSupport) == 0x10, "MotionTrackerInternalFlags layout");
static_assert(offsetof(MotionTrackerInternalFlags, badSurface) == 0x11, "MotionTrackerInternalFlags layout");
static_assert(offsetof(MotionTrackerInternalFlags, supported) == 0x12, "MotionTrackerInternalFlags layout");
static_assert(offsetof(MotionTrackerInternalFlags, damaging) == 0x13, "MotionTrackerInternalFlags layout");
static_assert(offsetof(MotionTrackerInternalFlags, clear) == 0x14, "MotionTrackerInternalFlags layout");

// void MotionTracker::check(unsigned short item, MotionTrackerInternalFlags &mflags)
// One item found near a move (isClear calls this for each). First, if the mover needs
// support and would end up standing on top of the item (no more than floorGap above it, the
// footprints overlapping), the item's surface is checked: one the mover may not stand on
// sets badSurface; a solid one it may stand on sets supported and becomes the new floorGap,
// and standing right on it counts as a collision at the start of the move. Then, if the item
// blocks the mover (or either is damaging), the move is tested against the item's six faces,
// low x, low y, low z, then high x, high y, high z. The first face the move passes through is
// a collision at the last point before it; a blocking one also clears mflags.clear and sets
// the direction in blocked.
void MotionTracker::check(uint16_t other, MotionTrackerInternalFlags &mflags)
{
	int16_t moveX = mflags.dx;
	int16_t moveY = mflags.dy;
	int16_t moveZ = mflags.dz;

	// The assembly copied only the first four bytes of the record onto its stack and passed
	// that to surfaceFlags and volumeFlags, which read only the first word.
	TypeFlag typeFlag = GlobalTypes.typeFlags[ItemData::typeArray[other]];

	int16_t xFoot = typeFlag.xSize * 32;
	int16_t yFoot = typeFlag.ySize * 32;
	int16_t zFoot = typeFlag.zSize * 8;
	if (zFoot == 0 && !typeFlag.fixed)
		zFoot = 1;

	if (ItemData::statusArray[other] & FLIPPED)
	{
		int16_t t = xFoot;
		xFoot = yFoot;
		yFoot = t;
	}

	if (mflags.needSupport)
	{
		// How far the mover would stand above the top of the item.
		int16_t gap = mflags.to.z - (ItemData::zArray[other] + zFoot);
		if (gap >= 0 && floorGap >= gap)
		{
			// The item covers x - xFoot + 1 .. x (taken as signed), the mover at its
			// destination to.x - xd + 1 .. to.x (taken as unsigned); compared as longs.
			int16_t x = ItemData::xArray[other];
			int16_t y = ItemData::yArray[other];
			int32_t itemLeft = (int16_t)(x - xFoot + 1);
			int32_t itemTop = (int16_t)(y - yFoot + 1);
			if ((int32_t)(uint16_t)(mflags.to.x - xd + 1) <= x &&
				(int32_t)(uint16_t)(mflags.to.y - yd + 1) <= y &&
				(int32_t)mflags.to.x >= itemLeft &&
				(int32_t)mflags.to.y >= itemTop)
			{
				int16_t surface = CollisionBuffer::surfaceFlags(typeFlag);
				if ((uint16_t)(surfaceMask & surface))
					mflags.badSurface = 1;
				else if (typeFlag.solid && (uint16_t)(surface & ~surfaceMask))
				{
					mflags.supported = 1;
					floorGap = gap;
					if (gap == 0)
						collision(other, 0, 0, 0, 0);
				}
			}
		}
	}

	// Only the low bytes are compared: COLLIDE_ANY never counts as a hit.
	uint8_t hit = (uint8_t)CollisionBuffer::volumeFlags(typeFlag) & (uint8_t)volumeMask;
	uint8_t damaging = typeFlag.damaging || mflags.damaging ? 1 : 0;
	if (hit == 0 && !damaging)
		return;

	// The offsets from the start of the move at which the mover overlaps the item: x from
	// xLo to xHi, and so on.
	int16_t x = ItemData::xArray[other];
	int16_t y = ItemData::yArray[other];
	int16_t z = ItemData::zArray[other];
	int16_t fromZ = mflags.from.z;
	int16_t xLo = x - xFoot + 1 - mflags.from.x;
	int16_t yLo = y - yFoot + 1 - mflags.from.y;
	int16_t xHi = x + xd - 1 - mflags.from.x;
	int16_t yHi = y + yd - 1 - mflags.from.y;
	int16_t zHi = z + zFoot - 1 - fromZ;
	int16_t zLo = z - zd + 1 - fromZ;
	int16_t t;

	// Into the low x face: the move reaches xLo, and at that fraction of it (xLo / moveX)
	// y and z are inside the item.
	if (moveX != 0 && moveX >= xLo && xLo > 0 &&
		(int32_t)moveX * yHi >= (int32_t)moveY * xLo &&
		(int32_t)moveX * yLo <= (int32_t)moveY * xLo &&
		(int32_t)moveX * zHi >= (int32_t)moveZ * xLo &&
		(int32_t)moveX * zLo <= (int32_t)moveZ * xLo)
	{
		if (hit)
		{
			mflags.clear = 0;
			blocked |= BLOCKED_X;
		}
		t = xLo - 1;
		collision(other, t, (int16_t)((int32_t)moveY * t / moveX), (int16_t)((int32_t)moveZ * t / moveX), hit);
		return;
	}

	// Into the low y face.
	if (moveY != 0 && moveY >= yLo && yLo > 0 &&
		(int32_t)moveY * xHi >= (int32_t)moveX * yLo &&
		(int32_t)moveY * xLo <= (int32_t)moveX * yLo &&
		(int32_t)moveY * zHi >= (int32_t)moveZ * yLo &&
		(int32_t)moveY * zLo <= (int32_t)moveZ * yLo)
	{
		if (hit)
		{
			mflags.clear = 0;
			blocked |= BLOCKED_Y;
		}
		t = yLo - 1;
		collision(other, (int16_t)((int32_t)moveX * t / moveY), t, (int16_t)((int32_t)moveZ * t / moveY), hit);
		return;
	}

	// Into the low z face (the bottom).
	if (moveZ != 0 && moveZ >= zLo && zLo > 0 &&
		(int32_t)moveZ * xHi >= (int32_t)moveX * zLo &&
		(int32_t)moveZ * xLo <= (int32_t)moveX * zLo &&
		(int32_t)moveZ * yHi >= (int32_t)moveY * zLo &&
		(int32_t)moveZ * yLo <= (int32_t)moveY * zLo)
	{
		if (hit)
		{
			mflags.clear = 0;
			blocked |= BLOCKED_Z;
		}
		t = zLo - 1;
		collision(other, (int16_t)((int32_t)moveX * t / moveZ), (int16_t)((int32_t)moveY * t / moveZ), t, hit);
		return;
	}

	// Into the high x face, moving towards -x (so the inequalities turn round).
	if (moveX != 0 && moveX <= xHi && xHi < 0 &&
		(int32_t)moveX * yHi <= (int32_t)moveY * xHi &&
		(int32_t)moveX * yLo >= (int32_t)moveY * xHi &&
		(int32_t)moveX * zHi <= (int32_t)moveZ * xHi &&
		(int32_t)moveX * zLo >= (int32_t)moveZ * xHi)
	{
		if (hit)
		{
			mflags.clear = 0;
			blocked |= BLOCKED_X;
		}
		t = xHi + 1;
		collision(other, t, (int16_t)((int32_t)moveY * t / moveX), (int16_t)((int32_t)moveZ * t / moveX), hit);
		return;
	}

	// Into the high y face.
	if (moveY != 0 && moveY <= yHi && yHi < 0 &&
		(int32_t)moveY * xHi <= (int32_t)moveX * yHi &&
		(int32_t)moveY * xLo >= (int32_t)moveX * yHi &&
		(int32_t)moveY * zHi <= (int32_t)moveZ * yHi &&
		(int32_t)moveY * zLo >= (int32_t)moveZ * yHi)
	{
		if (hit)
		{
			mflags.clear = 0;
			blocked |= BLOCKED_Y;
		}
		t = yHi + 1;
		collision(other, (int16_t)((int32_t)moveX * t / moveY), t, (int16_t)((int32_t)moveZ * t / moveY), hit);
		return;
	}

	// Into the high z face (the top).
	if (moveZ != 0 && moveZ <= zHi && zHi < 0 &&
		(int32_t)moveZ * xHi <= (int32_t)moveX * zHi &&
		(int32_t)moveZ * xLo >= (int32_t)moveX * zHi &&
		(int32_t)moveZ * yHi <= (int32_t)moveY * zHi &&
		(int32_t)moveZ * yLo >= (int32_t)moveY * zHi)
	{
		if (hit)
		{
			mflags.clear = 0;
			blocked |= BLOCKED_Z;
		}
		t = zHi + 1;
		collision(other, (int16_t)((int32_t)moveX * t / moveZ), (int16_t)((int32_t)moveY * t / moveZ), t, hit);
	}
}
