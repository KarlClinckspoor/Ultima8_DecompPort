#ifndef COLBUF_H
#define COLBUF_H

#include "u8port.h"

#include "item.h"
#include "itemdata.h"

class TypeFlag;

// Collision flags. COLLIDE_ANY marks a shape that takes up space at all;
// scanArea drops it and returns -1 for an area off the map.
#define COLLIDE_SOLID		0x0001
#define COLLIDE_SEA			0x0002
#define COLLIDE_LAND		0x0004
#define COLLIDE_ROOF		0x0008
#define COLLIDE_DAMAGING	0x0020
#define COLLIDE_ANY			0x8000

// An item handle that can be pointed at another item.
class CollisionItem : public Item
{
public:
	CollisionItem(void) {}
	CollisionItem(Referent r) { referent = r; }
	CollisionItem &operator=(Referent r) { referent = r; return *this; }
	uint16_t getType(void) { return ItemData::typeArray[referent]; }
	uint16_t getStatus(void) { return ItemData::statusArray[referent]; }
};

class CollisionBuffer
{
public:
	static int16_t volumeFlags(TypeFlag &);
	static int16_t surfaceFlags(TypeFlag &);
	static int16_t get(uint16_t, uint16_t, uint8_t);
	static int16_t getCell(int16_t, int16_t, int16_t);
	static int16_t scanAreaCell(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t);
	static int16_t scanArea(uint16_t, uint16_t, uint16_t, uint16_t, uint8_t, uint8_t);
	static Boolean isSupported(Referent);
};

#endif
