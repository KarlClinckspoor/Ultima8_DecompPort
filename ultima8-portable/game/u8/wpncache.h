#ifndef WPNCACHE_H
#define WPNCACHE_H

#include "u8port.h"

#include "u8process.h"
#include "animcach.h"
#include "flex.h"
#include "sprite.h"

// One weapon type: its overlay in wpnovlay.dat and its combat numbers.
struct WeaponInfo
{
	uint16_t type;
	uint8_t overlay;
	uint8_t damageModifier;
	uint8_t baseDamage;
	char dexAttackBonus;
	char dexDefendBonus;
	char armorClassBonus;
	uint16_t damageType;
};

// Where the weapon overlay sits in one Avatar animation frame.
struct WeaponOffset
{
	char x;
	char y;
	uint16_t frame;
};

// The overlay offsets of the current weapon for each of the Avatar's animations.
class WeaponCache
{
public:
	static void load(uint16_t);
	static void unload(void);
	static int16_t getWeaponIndex(uint16_t);
	static int16_t getWeaponOverlayIndex(uint16_t);
	static void setWeaponType(uint16_t);
	static void setOverlayIndex(uint16_t);
	static void getOffsets(uint16_t, AnimSet, char, uint8_t, uint16_t &, char &, char &, uint16_t &);
	static uint8_t isWeaponDrawn(uint16_t, AnimSet);
	static int16_t getDamageModifier(uint16_t);
	static int16_t getBaseDamage(uint16_t);
	static int16_t getDexAttackBonus(uint16_t);
	static int16_t getDexDefendBonus(uint16_t);
	static int16_t getArmorClassBonus(uint16_t);
	static int16_t getDamageType(uint16_t);
	static void flush(void);
	static uint16_t weaponType;
	static uint16_t overlayIndex;
	static uint16_t typeArray[15];
	static WeaponInfo typeIndirectionArray[15];
	static WeaponOffset *wpnAnimTable[64];
	static FlexFile file;
};

void initAnimCache(void);
void uninitAnimCache(void);

extern char *weaponOverlayFile;
extern char *weaponString[15];

#endif
