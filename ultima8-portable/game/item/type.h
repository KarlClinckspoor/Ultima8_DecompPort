#ifndef TYPE_H
#define TYPE_H

#include "u8port.h"

class TypeFlag;

#define NUM_TYPES		2048
#define NUM_FRAMES		256
#define TYPE_NAME_LEN	32

// One bit per frame of a shape.
class FrameSet
{
public:
	uint8_t bits[NUM_FRAMES / 8];

	FrameSet(void) { init(); }
	void init(void);
	void add(uint16_t);
	char contains(uint16_t);
};

// Marks every shape and frame placed on the map, so they can be loaded in one pass.
class ShapePreloader
{
public:
	static char init(void);
	static char uninit(void);
	static FrameSet *getFrameSet(uint16_t);
	static void add(uint16_t, uint16_t);
	static char contains(uint16_t, uint16_t);
	static char make(void);
	static void preload(uint8_t);
	static FrameSet *type[2];
};

// Shape families, from the high nibble of a type's second flag byte.
#define GENERIC_FAMILY		0
#define QUAL_FAMILY			1
#define QUAN_FAMILY			2
#define GLOBEGG_FAMILY		3
#define UNKEGG_FAMILY		4
#define BREAKABLE_FAMILY	5
#define CONTAINER_FAMILY	6
#define MONSTEREGG_FAMILY	7
#define TELEPORTEGG_FAMILY	8
#define REAGENT_FAMILY		9

// One record of typeflag.dat.
class TypeFlag
{
public:
	uint16_t fixed : 1;
	uint16_t solid : 1;
	uint16_t sea : 1;
	uint16_t land : 1;
	uint16_t occl : 1;
	uint16_t bag : 1;
	uint16_t damaging : 1;
	uint16_t noisy : 1;
	uint16_t draw : 1;
	uint16_t ignore : 1;
	uint16_t roof : 1;
	uint16_t trans : 1;
	uint16_t family : 4;
	uint16_t equipType : 4;
	uint16_t xSize : 4;
	uint16_t ySize : 4;
	uint16_t zSize : 4;
	uint16_t animType : 4;
	uint16_t animData : 4;
	uint16_t animSpeed : 4;
	uint16_t editor : 1;
	uint16_t explode : 1;
	uint16_t unknown1 : 1;
	uint16_t unknown2 : 1;
	uint8_t weight;
	uint8_t volume;

	void init(void);

	// Footpad size in world units; a non-fixed shape is never flat.
	void getWorldSize(int16_t &x, int16_t &y, int16_t &z)
	{
		x = xSize * 32;
		y = ySize * 32;
		z = zSize * 8;
		z == 0 && !fixed ? z = 1 : 0;
	}

	// The same in display list units, a quarter of world x and y.
	void getFootpad(int16_t &x, int16_t &y, int16_t &z)
	{
		x = xSize * 8;
		y = ySize * 8;
		z = zSize * 8;
		z == 0 && !fixed ? z = 1 : 0;
	}
};

class TypeObject
{
public:
	char *typeNames;
	TypeFlag *typeFlags;

	TypeObject(void);
	void loadTypes(void);
	TypeFlag getTypeFlag(uint16_t);
	void loadTypeNames(void);
	char *getTypeName(int16_t);
};

class WorldPoint
{
public:
	uint16_t x;
	uint16_t y;
	uint8_t z;

	WorldPoint(void) {}
	WorldPoint(uint16_t px, uint16_t py, uint8_t pz) { x = px; y = py; z = pz; }
	void set(uint16_t px, uint16_t py, uint8_t pz) { x = px; y = py; z = pz; }
	uint8_t operator==(WorldPoint &);
};

int16_t getFamilyOfType(uint16_t);
int16_t guardianBark(int16_t);

extern char *typeFlagFileName;
extern char *typeNameFile;
extern TypeObject GlobalTypes;

#endif
