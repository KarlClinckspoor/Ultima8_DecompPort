// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: OCCLUDE.C

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "item.h"
#include "itemcach.h"
#include "type.h"
#include "camera.h"
#include "error.h"
#include "occlude.h"

class ZItem : public Item
{
public:
	ZItem(Referent r) { referent = r; }
};

inline Item itemOf(Referent r) { return ZItem(r); }

// The world corner of region (rx, ry).
inline void regionOrigin(int16_t rx, int16_t ry, int16_t &x, int16_t &y)
{
	x = rx << 9;
	y = ry << 9;
}

// How far a height shifts an item along x and y on screen.
inline int16_t zToXY(uint8_t z) { return z * 4; }

// A shape's size in world units.
inline void footprint(TypeFlag &type, int16_t &xd, int16_t &yd, int16_t &zd)
{
	xd = type.xSize * 32;
	yd = type.ySize * 32;
	zd = type.zSize * 8;
	if (zd == 0 && !type.fixed)
		zd = 1;
}

inline Zbuffer::Zbuffer(void) { buffer = 0; }

Zbuffer theZbuffer;

Zbuffer::~Zbuffer(void)
{
	if (buffer)
	{
		delete buffer;
		buffer = 0;
	}
}

void Zbuffer::alloc(void)
{
	if (!buffer)
	{
		buffer = new uint8_t[ZBUFFER_SIZE][ZBUFFER_SIZE];
		if (!buffer)
			outOfMemory(__FILE__, 66);	// __LINE__
	}
}

void Zbuffer::init(void)
{
	memset(buffer, 0, sizeof(*buffer) * ZBUFFER_SIZE);
}

void Zbuffer::make(void)
{
	int16_t i, j, rx, ry;
	Referent r;
	int16_t xd, yd, zd, left, top, right, bottom;
	int16_t originX, originY;
	uint8_t z, roof;
	Item item;
	TypeFlag type;
	uint16_t h = ItemCache::regionsIn.minSum + 7;
	uint16_t v = ItemCache::regionsIn.minDiff + 3;
	// Unsigned 16-bit sums and differences, shifted logically, as Borland computed them.
	int16_t rx0 = ((uint16_t)(h + v) >> 1) - 4;
	int16_t ry0 = ((uint16_t)(h - v) >> 1) - 4;
	init();
	roof = Camera::roof();
	regionOrigin(rx0, ry0, originX, originY);
	for (i = 0; i < 9; i++)
	{
		rx = rx0 + i;
		for (j = 0; j < 9; j++)
		{
			ry = ry0 + j;
			if (ItemCache::regionsIn.isNeeded(rx, ry))
			{
				for (r = ItemCache::regionStartHash[rx][ry]; r; r = ItemData::nextArray[item.referent])
				{
					item = itemOf(r);
					type = GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
					if (type.fixed && type.occl && ((!Camera::disableDrawBit && type.draw) || ItemData::zArray[item.referent] < roof))
					{
						footprint(type, xd, yd, zd);
						int16_t lift = zToXY(ItemData::zArray[item.referent]);
						uint16_t x = ItemData::xArray[item.referent] - originX - lift;
						uint16_t y = ItemData::yArray[item.referent] - originY - lift;
						z = ItemData::zArray[item.referent];
						int16_t x1 = x - zd * 4;
						int16_t y1 = y - zd * 4;
						left = ((x1 - xd) >> 5) + 1;
						if (left < 0)
							left = 0;
						top = ((y1 - yd) >> 5) + 1;
						if (top < 0)
							top = 0;
						right = ((x1 + 1) >> 5) - 1;
						if (right >= ZBUFFER_SIZE)
							right = ZBUFFER_SIZE - 1;
						bottom = ((y1 + 1) >> 5) - 1;
						if (bottom >= ZBUFFER_SIZE)
							bottom = ZBUFFER_SIZE - 1;
						for (int16_t cx = left; cx <= right; cx++)
							for (int16_t cy = top; cy <= bottom; cy++)
								if (buffer[cx][cy] < z)
									buffer[cx][cy] = z;
					}
				}
			}
		}
	}
}

uint8_t Zbuffer::isHidden(Referent r)
{
	int16_t xd, yd, zd, left, top, right, bottom;
	TypeFlag type;
	int16_t originX, originY;
	uint16_t x, y;
	int16_t x1, y1;
	uint8_t z, hidden;
	uint16_t h = ItemCache::regionsIn.minSum + 7;
	uint16_t v = ItemCache::regionsIn.minDiff + 3;
	// Unsigned 16-bit sums and differences, shifted logically, as Borland computed them.
	int16_t rx0 = ((uint16_t)(h + v) >> 1) - 4;
	int16_t ry0 = ((uint16_t)(h - v) >> 1) - 4;
	ZItem item(r);
	regionOrigin(rx0, ry0, originX, originY);
	type = GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
	footprint(type, xd, yd, zd);
	int16_t lift = zToXY(ItemData::zArray[item.referent]);
	x = ItemData::xArray[item.referent] - originX - lift;
	y = ItemData::yArray[item.referent] - originY - lift;
	z = ItemData::zArray[item.referent];
	x1 = x - zd * 4;
	y1 = y - zd * 4;
	left = (x1 - xd + 1) >> 5;
	if (left < 0)
		left = 0;
	top = (y1 - yd + 1) >> 5;
	if (top < 0)
		top = 0;
	right = x >> 5;
	if (right >= ZBUFFER_SIZE)
		right = ZBUFFER_SIZE - 1;
	bottom = y >> 5;
	if (bottom >= ZBUFFER_SIZE)
		bottom = ZBUFFER_SIZE - 1;
	hidden = left <= right && top <= bottom;
	for (int16_t cx = left; cx <= right; cx++)
	{
		for (int16_t cy = top; cy <= bottom; cy++)
		{
			if (buffer[cx][cy] <= z)
			{
				hidden = 0;
				break;
			}
		}
	}
	return hidden;
}
