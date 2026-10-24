// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r- -y
// name: COLBUF.C

#include "u8port.h"

#include "item.h"
#include "itemcach.h"
#include "itemfind.h"
#include "type.h"
#include "colbuf.h"

// A shape's size in world units: 32 per x/y cell, 8 per z step.
// Flat shapes that are not fixed still count as one unit tall.
inline void footprint(TypeFlag &type, int16_t &xd, int16_t &yd, int16_t &zd)
{
	xd = type.xSize * 32;
	yd = type.ySize * 32;
	zd = type.zSize * 8;
	if (zd == 0 && !type.fixed)
		zd = 1;
}

int16_t CollisionBuffer::volumeFlags(TypeFlag &type)
{
	int16_t flags = COLLIDE_ANY;
	if (type.solid) flags |= COLLIDE_SOLID;
	if (type.damaging) flags |= COLLIDE_DAMAGING;
	if (type.roof) flags |= COLLIDE_ROOF;
	return flags;
}

int16_t CollisionBuffer::surfaceFlags(TypeFlag &type)
{
	int16_t flags = 0;
	if (type.sea) flags |= COLLIDE_SEA;
	if (type.land) flags |= COLLIDE_LAND;
	return flags;
}

int16_t CollisionBuffer::get(uint16_t x, uint16_t y, uint8_t z)
{
	// World coordinates to cells.

	x >>= 5;
	y >>= 5;
	z >>= 3;

	return scanAreaCell(x, y, x, y, z, z);
}

int16_t CollisionBuffer::getCell(int16_t x, int16_t y, int16_t z)
{
	return scanAreaCell(x, y, x, y, z, z);
}

int16_t CollisionBuffer::scanAreaCell(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t z1, int16_t z2)
{
	if (x1 < 0 || y1 < 0 || x2 < 0 || y2 < 0)
		return -1;

	uint16_t worldX1 = x1 << 5;
	uint16_t worldY1 = y1 << 5;
	uint16_t worldX2 = (x2 << 5) + 31;
	uint16_t worldY2 = (y2 << 5) + 31;
	uint8_t worldZ1 = z1 << 3;
	uint8_t worldZ2 = z2 << (3 + 7);	// the original's "z2 << 3 + 7": its low byte is always 0

	return scanArea(worldX1, worldY1, worldX2, worldY2, worldZ1, worldZ2);
}

int16_t CollisionBuffer::scanArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t z1, uint8_t z2)
{
	int16_t flags = 0;

	if ((int16_t)x1 < 0 || (int16_t)y1 < 0 || (int16_t)x2 < 0 || (int16_t)y2 < 0)
		return -1;

	if (z1 == 0 && z2 == 255)
		// The whole height of the map.
		return 0;

	int16_t xd, yd, zd;

	CollisionItem item;
	TypeFlag type;
	AreaItemFinder finder(x1, y1, x2 + 256, y2 + 256);
	while (finder.found())
	{
		item = finder.referent;
		if (ItemData::xArray[item.referent] >= x1 && ItemData::yArray[item.referent] >= y1 && ItemData::zArray[item.referent] <= z2)
		{
			type = GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
			footprint(type, xd, yd, zd);
			// Unsigned 16-bit in Borland (unsigned - int is unsigned).
			if ((uint16_t)(ItemData::xArray[item.referent] - xd + 1) <= x2 && (uint16_t)(ItemData::yArray[item.referent] - yd + 1) <= y2 && ItemData::zArray[item.referent] + zd - 1 >= z1)
			{
				flags |= volumeFlags(type) & ~COLLIDE_ANY;
			}
		}
		finder.findNext();
	}
	return flags;
}

Boolean CollisionBuffer::isSupported(Referent referent)
{
	CollisionItem item(referent);
	uint8_t z = ItemData::zArray[item.referent];
	if (z == 0 || ItemData::statusArray[item.referent] & 8)
		// On the ground, or inside a container.
		return TRUE;

	int16_t supportFlags = 0;
	uint16_t x = ItemData::xArray[item.referent];
	uint16_t y = ItemData::yArray[item.referent];
	TypeFlag type = GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
	int16_t needFlags = volumeFlags(type);
	int16_t xd, yd, zd;
	footprint(type, xd, yd, zd);
	uint16_t left = x - xd + 1;
	uint16_t top = y - yd + 1;
	AreaItemFinder finder(left, top, x + 256, y + 256);
	while (finder.found())
	{
		item = finder.referent;
		if (item.referent != referent && ItemData::xArray[item.referent] >= left && ItemData::yArray[item.referent] >= top && ItemData::zArray[item.referent] <= z)
		{
			type = GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
			footprint(type, xd, yd, zd);
			if ((uint16_t)(ItemData::xArray[item.referent] - xd + 1) <= x && (uint16_t)(ItemData::yArray[item.referent] - yd + 1) <= y && ItemData::zArray[item.referent] + zd == z)
			{
				supportFlags |= volumeFlags(type) & ~COLLIDE_ANY;
			}
		}
		finder.findNext();
	}
	return (supportFlags & needFlags) != 0;
}
