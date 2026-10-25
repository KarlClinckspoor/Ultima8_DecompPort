// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: WPNCACHE.C

#include "u8port.h"

#include "npc.h"
#include "error.h"
#include "filespec.h"
#include "scratchm.h"
#include "animcach.h"
#include "astream.h"
#include "wpncache.h"

// Inline in the shared headers when this file was built.
U8_INLINE uint8_t BaseFile::is_valid(void) { return handle != -1; }
U8_INLINE BaseFile::~BaseFile(void) { if (is_valid()) close(); }
U8_INLINE SharedFile::SharedFile(void) {}
U8_INLINE FlexFile::FlexFile(void) {}

static char weaponOverlayFile_text[] = "wpnovlay.dat";
char *weaponOverlayFile = weaponOverlayFile_text;
char *weaponString[15] = {"Mace      ", "Sword     ", "Axe       ", "Dagger    ", "Earth Glow", "Air Glow  ", "Fire Glow ", "", "", "", "", "", "", "", ""};
uint16_t WeaponCache::weaponType = 0;
uint16_t WeaponCache::overlayIndex = 0;
uint16_t WeaponCache::typeArray[15] = {241, 243, 244, 245, 0x18e, 0x18f, 0x190, 0, 0, 0, 0, 0, 0, 0, 0};
WeaponInfo WeaponCache::typeIndirectionArray[15] =
{
	{0x21c, 0, 9, 4, 0, 0, 0, 4},
	{0x21d, 1, 12, 5, 0, 0, 0, 2},
	{0x21e, 1, 12, 5, 0, 0, 0, 2},
	{0x1a4, 1, 13, 5, 0, 0, 0, 2},
	{0x1a3, 2, 14, 6, 0, 0, 0, 6},
	{0x1a2, 3, 7, 3, 0, 0, 0, 2},
	{0x1a1, 0, 11, 4, 0, 0, 0, 4},
	{0x32f, 0, 9, 11, 0, 0, 0, 36},
	{0x336, 2, 14, 8, 2, 2, 2, 38},
	{0x330, 0, 11, 4, 0, 0, 0, 100},
	{0x335, 1, 12, 5, 1, 0, 0, 50},
	{0x334, 3, 7, 7, 4, 0, 1, 34},
	{0x333, 1, 13, 5, 7, 0, 0, 34},
	{0x332, 1, 13, 5, 0, 4, 5, 34},
	{0x331, 1, 12, 5, 1, 0, 0, 42}
};

void WeaponCache::load(uint16_t type)
{
	int16_t anim;
	AnimStreamHeader header;

	weaponType = type;
	unload();
	file.init(fileSpec(0, StaticDir, weaponOverlayFile, 0), ReadOnly, -1);
	overlayIndex = getWeaponOverlayIndex(type);
	for (anim = 0; anim < 64; anim++)
	{
		header = AnimCache::getAnimStreamHeader(1, (AnimSet)anim);
		int16_t frames;
		if ((frames = header.frames) != 0 && header.attack)
		{
			int16_t size = frames * 8 * sizeof(WeaponOffset);
			wpnAnimTable[anim] = (WeaponOffset *)new char[size];
			if (!wpnAnimTable[anim])
				halt(__FILE__, 160);	// __LINE__
			file.readRecord(anim, wpnAnimTable[anim], size, size * overlayIndex);
		}
	}
	file.close();
}

void WeaponCache::unload(void)
{
	int16_t anim;

	for (anim = 0; anim < 64; anim++)
	{
		if (wpnAnimTable[anim])
		{
			delete wpnAnimTable[anim];
			wpnAnimTable[anim] = 0;
		}
	}
}

int16_t WeaponCache::getWeaponIndex(uint16_t type)
{
	int16_t i;

	for (i = 0; i < 15; i++)
		if (typeIndirectionArray[i].type == type)
			return i;
	halt(__FILE__, 194, "%d.\n", type);	// __LINE__
	return 0;
}

int16_t WeaponCache::getWeaponOverlayIndex(uint16_t type)
{
	return typeIndirectionArray[getWeaponIndex(type)].overlay;
}

void WeaponCache::setWeaponType(uint16_t type)
{
	if (weaponType != type)
		load(type);
}

void WeaponCache::setOverlayIndex(uint16_t overlay)
{
	if (overlayIndex != overlay)
	{
		int16_t i;

		for (i = 0; i < 15; i++)
		{
			if (typeIndirectionArray[i].overlay == overlay)
			{
				setWeaponType(typeIndirectionArray[i].type);
				return;
			}
		}
		halt(__FILE__, 232);	// __LINE__
	}
}

void WeaponCache::getOffsets(uint16_t type, AnimSet anim, char dir, uint8_t animFrame,
	uint16_t &shape, char &x, char &y, uint16_t &frame)
{
	AnimStreamHeader header;
	WeaponOffset offset;

	if (weaponType != type)
		load(type);
	if (!wpnAnimTable[anim])
		halt(__FILE__, 245);	// __LINE__
	header = AnimCache::getAnimStreamHeader(1, anim);
	offset = wpnAnimTable[anim][dir * header.frames + animFrame];
	shape = typeArray[overlayIndex];
	x = offset.x;
	y = offset.y;
	frame = offset.frame;
}

uint8_t WeaponCache::isWeaponDrawn(uint16_t type, AnimSet anim)
{
	if (weaponType != type)
		load(type);
	return wpnAnimTable[anim] != 0;
}

int16_t WeaponCache::getDamageModifier(uint16_t type)
{
	return typeIndirectionArray[getWeaponIndex(type)].damageModifier;
}

int16_t WeaponCache::getBaseDamage(uint16_t type)
{
	return typeIndirectionArray[getWeaponIndex(type)].baseDamage;
}

int16_t WeaponCache::getDexAttackBonus(uint16_t type)
{
	return typeIndirectionArray[getWeaponIndex(type)].dexAttackBonus;
}

int16_t WeaponCache::getDexDefendBonus(uint16_t type)
{
	return typeIndirectionArray[getWeaponIndex(type)].dexDefendBonus;
}

int16_t WeaponCache::getArmorClassBonus(uint16_t type)
{
	return typeIndirectionArray[getWeaponIndex(type)].armorClassBonus;
}

int16_t WeaponCache::getDamageType(uint16_t type)
{
	return typeIndirectionArray[getWeaponIndex(type)].damageType;
}

void WeaponCache::flush(void)
{
	unload();
	weaponType = 0;
}

WeaponOffset *WeaponCache::wpnAnimTable[64];
FlexFile WeaponCache::file;
