// Port only: the weapon, armour and monster tables, taken from U8.EXE in the game folder.
//
// DOS U8.EXE has these tables in its data segment (u8/wpncache.cpp, u8/armordat.cpp,
// u8/mondata.cpp), and the editor (editor/, u8edit) changes them there. Reading them back from the
// same place keeps the port playing with the numbers DOS would. The tables are found by what
// editing leaves alone (the weapon shapes, the monster shapes, the ends of the armour lists); when
// U8.EXE is missing or does not hold them as expected, the port keeps its own copies, which are
// the original numbers.

#include "u8port.h"

#include <string.h>

#include <type_traits>

#include "combatbr.h"
#include "plat.h"
#include "u8stdint.h"
#include "wpncache.h"

namespace
{

const uint16_t weaponTypes[15] = {241, 243, 244, 245, 0x18e, 0x18f, 0x190, 0, 0, 0, 0, 0, 0, 0, 0};
const int armourLengths[12] = {3, 3, 6, 4, 5, 2, 2, 2, 2, 2, 2, 14};
enum { WeaponRecord = 10, MonsterRecord = 13, ArmourBytes = 47 };

uint16_t word(const uint8_t *p)
{
	return (uint16_t)(p[0] | p[1] << 8);
}

bool wordsAt(const uint8_t *p, const uint16_t *w, int n)
{
	for (int i = 0; i < n; i++)
		if (word(p + i * 2) != w[i])
			return false;
	return true;
}

bool armourAt(const uint8_t *p)
{
	for (int len : armourLengths)
	{
		for (int i = 0; i < len - 1; i++)
			if (p[i] == 0x80)
				return false;
		if (p[len - 1] != 0x80)
			return false;
		p += len;
	}
	return true;
}

// The one place in the file where found says yes, or -1 (none, or more than one).
template <typename F>
int32_t findOnly(const uint8_t *data, int32_t size, int32_t from, int32_t to, int32_t need, F found)
{
	int32_t at = -1;
	if (from < 0)
		from = 0;
	if (to > size - need)
		to = size - need;
	for (int32_t i = from; i <= to; i++)
	{
		if (!found(data + i))
			continue;
		if (at >= 0)
			return -1;
		at = i;
	}
	return at;
}

}

extern "C" void U8LoadExeTables(void)
{
	int16_t file = plat_file_open("U8.EXE", 0);
	if (file < 0)
		return;
	int32_t size = plat_file_length(file);
	uint8_t *exe = size > 0 ? new uint8_t[size] : 0;
	bool read = exe && plat_file_read(file, exe, size) == size;
	plat_file_close(file);
	if (!read)
	{
		delete[] exe;
		return;
	}

	// Weapons: the shapes the weapon list starts with, then the records, which start with the
	// shapes in WeaponCache::typeIndirectionArray.
	int32_t weapons = findOnly(exe, size, 0, size, 30 + 15 * WeaponRecord, [&](const uint8_t *p) {
		if (!wordsAt(p, weaponTypes, 15))
			return false;
		for (int i = 0; i < 15; i++)
			if (word(p + 30 + i * WeaponRecord) != WeaponCache::typeIndirectionArray[i].type)
				return false;
		return true;
	});
	int changed = 0;
	auto take = [&](auto &field, auto value) {
		if (field != (std::remove_reference_t<decltype(field)>)value)
			changed++;
		field = (std::remove_reference_t<decltype(field)>)value;
	};
	if (weapons >= 0)
	{
		const uint8_t *r = exe + weapons + 30;
		for (int i = 0; i < 15; i++, r += WeaponRecord)
		{
			WeaponInfo &w = WeaponCache::typeIndirectionArray[i];
			take(w.damageModifier, r[3]);
			take(w.baseDamage, r[4]);
			take(w.dexAttackBonus, (char)r[5]);
			take(w.dexDefendBonus, (char)r[6]);
			take(w.armorClassBonus, (char)r[7]);
			take(w.damageType, word(r + 8));
		}
	}

	// Monsters: the shape list, then the records, the first of them empty.
	int32_t monsters = findOnly(exe, size, 0, size, 62 + (int32_t)sizeof DataTable::data, [&](const uint8_t *p) {
		if (!wordsAt(p, DataTable::monsterTypes, 31))
			return false;
		for (int i = 0; i < MonsterRecord; i++)
			if (p[62 + i] != 0)
				return false;
		return true;
	});
	if (monsters >= 0)
	{
		changed += memcmp(DataTable::data, exe + monsters + 62, sizeof DataTable::data) != 0;
		memcpy(DataTable::data, exe + monsters + 62, sizeof DataTable::data);
	}

	// Armour: the lists end with -128 at their lengths and nowhere else. In U8.EXE they sit just
	// before the weapon list.
	if (weapons >= 0)
	{
		int32_t armour = findOnly(exe, size, weapons - 4096, weapons, ArmourBytes, armourAt);
		if (armour >= 0)
		{
			char *lists[12] = {ArmorData::shieldBonus, ArmorData::leggingBonus, ArmorData::helmetBonus,
				ArmorData::armourBonus, ArmorData::armGuardsBonus, ArmorData::magArmorBonus,
				ArmorData::magArmorTwoBonus, ArmorData::magShieldBonus, ArmorData::magArmsBonus,
				ArmorData::magLegsBonus, ArmorData::magHelmBonus, ArmorData::clothesBonus};
			const uint8_t *p = exe + armour;
			for (int k = 0; k < 12; k++)
			{
				changed += memcmp(lists[k], p, armourLengths[k]) != 0;
				memcpy(lists[k], p, armourLengths[k]);
				p += armourLengths[k];
			}
		}
	}
	delete[] exe;
	if (changed)
		plat_log("u8: U8.EXE has changed weapon, armour or monster statistics; playing with them\n");
}
