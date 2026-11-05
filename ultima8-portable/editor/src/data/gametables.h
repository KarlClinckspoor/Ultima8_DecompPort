// Game data that is not in the data files.
//
// Weapon, armour and monster statistics are tables in U8.EXE's data segment (u8/wpncache.cpp,
// u8/armordat.cpp, u8/mondata.cpp). They are found by what does not change when they are
// edited: the weapon shapes, the monster shapes and the ends of the armour lists. Editing patches
// those bytes in U8.EXE, so DOS U8.EXE plays with the new numbers, and the port reads them from
// the same place (game/port/exetables.cpp).
//
// NPC statistics are in the saved game, NPCDATA.DAT: 256 records of 49 bytes (NpcData in
// item/npc.h).
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include <cstdint>

struct WeaponStats
{
	int shape = 0;
	int overlay = 0;
	int damageModifier = 0;	// added to the strength roll (Npc::getDamageAmount)
	int baseDamage = 0;
	int dexAttack = 0;
	int dexDefend = 0;
	int armourClass = 0;
	int damageType = 0;		// DMG_ bits (item/item.cpp)

	bool operator==(const WeaponStats &o) const
	{
		return shape == o.shape && overlay == o.overlay && damageModifier == o.damageModifier &&
			baseDamage == o.baseDamage && dexAttack == o.dexAttack && dexDefend == o.dexDefend &&
			armourClass == o.armourClass && damageType == o.damageType;
	}
};

struct ArmourTable
{
	int shape = 0;
	int offset = 0;		// in the armour block
	QVector<int> bonus;	// by frame / 2; -128 ends a list and is not an allowed value

	bool operator==(const ArmourTable &o) const { return shape == o.shape && bonus == o.bonus; }
};

struct MonsterStats
{
	int shape = 0;		// 0: an unused record
	int minHp = 0, maxHp = 0, minDex = 0, maxDex = 0, minDamage = 0, maxDamage = 0;
	int armourClass = 0, alignment = 0, unknown8 = 0, damageType = 0, defenseType = 0;

	bool operator==(const MonsterStats &o) const
	{
		return shape == o.shape && minHp == o.minHp && maxHp == o.maxHp && minDex == o.minDex &&
			maxDex == o.maxDex && minDamage == o.minDamage && maxDamage == o.maxDamage &&
			armourClass == o.armourClass && alignment == o.alignment && unknown8 == o.unknown8 &&
			damageType == o.damageType && defenseType == o.defenseType;
	}
};

QString damageTypeNames(int bits);

class ExeTables
{
public:
	bool load(const QString &path, QString *error);
	bool loaded() const { return !exe_.isEmpty(); }
	QString path() const { return path_; }
	// Writes the changed bytes into the file; the rest of it is left as it is.
	bool save(QString *error);
	bool dirty() const;

	QVector<WeaponStats> weapons;
	QVector<ArmourTable> armour;
	QVector<MonsterStats> monsters;	// record 1 first (record 0 is empty)

	static const char *weaponName(int index);
	static const char *armourName(int index);

private:
	QByteArray encode() const;
	QString path_;
	QByteArray exe_;
	int weaponAt_ = -1, armourAt_ = -1, monsterAt_ = -1;
};

// One NPC's record in NPCDATA.DAT.
struct NpcStats
{
	enum
	{
		// Bits of the word at 27.
		Invincible = 0x0001,
		// Bits of the word at 47.
		InUse = 0x0001, Dead = 0x0010, Immortal = 0x0400, WithstandDeath = 0x0800, Poisoned = 0x4000,
	};
	int str = 0, dex = 0, intelligence = 0, hp = 0, mana = 0;
	int alignment = 0;	// its own in the low nibble, its enemies' in the high
	int flags1 = 0, flags2 = 0;
};

class NpcDataFile
{
public:
	enum { Count = 256, RecordSize = 49 };
	bool parse(const QByteArray &file);
	bool valid() const { return at_ >= 0; }
	QByteArray bytes() const { return file_; }
	NpcStats get(int npc) const;
	void set(int npc, const NpcStats &s);

private:
	QByteArray file_;
	int at_ = -1;
};
