#include "data/gametables.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <functional>

#include "data/flex.h"

QString damageTypeNames(int bits)
{
	static const char *names[] = {"normal", "blade", "blunt", "fire", "undead", "magic", "slayer",
		"pierce", "falling"};
	QStringList out;
	for (int i = 0; i < 9; i++)
		if (bits & (1 << i))
			out << QString::fromLatin1(names[i]);
	if (bits & ~0x1ff)
		out << QStringLiteral("0x%1").arg(bits & ~0x1ff, 0, 16);
	return out.isEmpty() ? QStringLiteral("none") : out.join(", ");
}

// The anchors: what the tables hold that editing leaves alone.
static const uint16_t weaponTypes[15] = {241, 243, 244, 245, 0x18e, 0x18f, 0x190, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint16_t weaponShapes[15] = {0x21c, 0x21d, 0x21e, 0x1a4, 0x1a3, 0x1a2, 0x1a1, 0x32f, 0x336,
	0x330, 0x335, 0x334, 0x333, 0x332, 0x331};
static const uint16_t monsterTypes[31] = {142, 411, 119, 120, 83, 96, 214, 509, 413, 357, 574, 807, 806,
	707, 76, 861, 808, 824, 835, 836, 708, 200, 269, 377, 860, 0, 0, 1, 401, 144, 405};
// The armour lists in order, their lengths with the -128 that ends each, and their shapes
// (Npc::getArmourClass, item/npc.cpp).
static const int armourLengths[12] = {3, 3, 6, 4, 5, 2, 2, 2, 2, 2, 2, 14};
static const int armourShapes[12] = {539, 532, 531, 523, 530, 841, 64, 842, 844, 845, 843, 823};
enum { WeaponRecord = 10, MonsterRecord = 13, MonsterRecords = 32, ArmourBytes = 47 };

const char *ExeTables::weaponName(int i)
{
	static const char *names[15] = {"Mace", "Sword", "Axe", "Dagger", "Earth glow", "Air glow", "Fire glow"};
	return i >= 0 && i < 7 ? names[i] : "";
}

const char *ExeTables::armourName(int i)
{
	static const char *names[12] = {"Shield", "Leggings", "Helmet", "Armour", "Arm guards",
		"Magic armour", "Magic armour 2", "Magic shield", "Magic arm guards", "Magic leggings",
		"Magic helmet", "Clothes"};
	return i >= 0 && i < 12 ? names[i] : "";
}

static QByteArray words(const uint16_t *w, int n)
{
	QByteArray b;
	for (int i = 0; i < n; i++)
	{
		b.append(char(w[i] & 0xff));
		b.append(char(w[i] >> 8));
	}
	return b;
}

static int findOnly(const QByteArray &hay, const QByteArray &needle, std::function<bool(int)> check)
{
	int found = -1;
	for (int at = hay.indexOf(needle); at >= 0; at = hay.indexOf(needle, at + 1))
	{
		if (!check(at))
			continue;
		if (found >= 0)
			return -2;	// more than one: do not guess
		found = at;
	}
	return found;
}

bool ExeTables::load(const QString &path, QString *error)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
	{
		if (error)
			*error = QStringLiteral("Cannot open %1").arg(path);
		return false;
	}
	path_ = path;
	exe_ = f.readAll();
	weapons.clear();
	armour.clear();
	monsters.clear();
	const char *d = exe_.constData();

	weaponAt_ = findOnly(exe_, words(weaponTypes, 15), [&](int at) {
		const int t = at + 30;
		if (t + 15 * WeaponRecord > exe_.size())
			return false;
		for (int i = 0; i < 15; i++)
			if (read16(d + t + i * WeaponRecord) != weaponShapes[i])
				return false;
		return true;
	});
	if (weaponAt_ >= 0)
	{
		weaponAt_ += 30;
		for (int i = 0; i < 15; i++)
		{
			const char *r = d + weaponAt_ + i * WeaponRecord;
			WeaponStats w;
			w.shape = read16(r);
			w.overlay = uint8_t(r[2]);
			w.damageModifier = uint8_t(r[3]);
			w.baseDamage = uint8_t(r[4]);
			w.dexAttack = int8_t(r[5]);
			w.dexDefend = int8_t(r[6]);
			w.armourClass = int8_t(r[7]);
			w.damageType = read16(r + 8);
			weapons << w;
		}
	}

	monsterAt_ = findOnly(exe_, words(monsterTypes, 31), [&](int at) {
		const int t = at + 62;
		if (t + MonsterRecords * MonsterRecord > exe_.size())
			return false;
		for (int i = 0; i < MonsterRecord; i++)
			if (d[t + i] != 0)
				return false;
		return true;
	});
	if (monsterAt_ >= 0)
	{
		monsterAt_ += 62;
		for (int i = 1; i < MonsterRecords; i++)
		{
			const uint8_t *r = reinterpret_cast<const uint8_t *>(d + monsterAt_ + i * MonsterRecord);
			MonsterStats m;
			m.shape = monsterTypes[i - 1];
			m.minHp = r[0];
			m.maxHp = r[1];
			m.minDex = r[2];
			m.maxDex = r[3];
			m.minDamage = r[4];
			m.maxDamage = r[5];
			m.armourClass = r[6];
			m.alignment = r[7];
			m.unknown8 = r[8];
			m.damageType = r[9] | r[10] << 8;
			m.defenseType = r[11] | r[12] << 8;
			monsters << m;
		}
	}

	// The armour block: every list ends with -128 where its length says and nowhere else.
	auto armourFits = [&](int at) {
		if (at < 0 || at + ArmourBytes > exe_.size())
			return false;
		int p = at;
		for (int len : armourLengths)
		{
			for (int i = 0; i < len - 1; i++)
				if (uint8_t(d[p + i]) == 0x80)
					return false;
			if (uint8_t(d[p + len - 1]) != 0x80)
				return false;
			p += len;
		}
		return true;
	};
	armourAt_ = -1;
	if (weaponAt_ >= 0)
	{
		// In U8.EXE the block sits a little before the weapon table; look near it first, then
		// anywhere.
		for (int at = std::max(0, weaponAt_ - 4096); at < weaponAt_ && armourAt_ < 0; at++)
			if (armourFits(at))
				armourAt_ = at;
	}
	if (armourAt_ >= 0)
	{
		int p = armourAt_;
		for (int k = 0; k < 12; k++)
		{
			ArmourTable a;
			a.shape = armourShapes[k];
			a.offset = p - armourAt_;
			for (int i = 0; i < armourLengths[k] - 1; i++)
				a.bonus << int(int8_t(d[p + i]));
			armour << a;
			p += armourLengths[k];
		}
	}

	if (weaponAt_ < 0 && monsterAt_ < 0 && armourAt_ < 0)
	{
		if (error)
			*error = QStringLiteral("%1 does not hold the tables of the U8.EXE this was written for.")
				.arg(QFileInfo(path).fileName());
		exe_.clear();
		return false;
	}
	return true;
}

// The exe with the tables as they are now.
QByteArray ExeTables::encode() const
{
	QByteArray out = exe_;
	auto put = [&](int at, int v) { out[at] = char(v & 0xff); };
	if (weaponAt_ >= 0)
		for (int i = 0; i < weapons.size(); i++)
		{
			const int r = weaponAt_ + i * WeaponRecord;
			const WeaponStats &w = weapons[i];
			put(r + 3, w.damageModifier);
			put(r + 4, w.baseDamage);
			put(r + 5, w.dexAttack);
			put(r + 6, w.dexDefend);
			put(r + 7, w.armourClass);
			put(r + 8, w.damageType);
			put(r + 9, w.damageType >> 8);
		}
	if (monsterAt_ >= 0)
		for (int i = 0; i < monsters.size(); i++)
		{
			const int r = monsterAt_ + (i + 1) * MonsterRecord;
			const MonsterStats &m = monsters[i];
			const int v[13] = {m.minHp, m.maxHp, m.minDex, m.maxDex, m.minDamage, m.maxDamage, m.armourClass,
				m.alignment, m.unknown8, m.damageType, m.damageType >> 8, m.defenseType, m.defenseType >> 8};
			for (int k = 0; k < 13; k++)
				put(r + k, v[k]);
		}
	if (armourAt_ >= 0)
		for (const ArmourTable &a : armour)
			for (int i = 0; i < a.bonus.size(); i++)
				put(armourAt_ + a.offset + i, a.bonus[i] == -128 ? -127 : a.bonus[i]);
	return out;
}

bool ExeTables::dirty() const
{
	return loaded() && encode() != exe_;
}

bool ExeTables::save(QString *error)
{
	if (!loaded())
		return true;
	QByteArray out = encode();
	if (out == exe_)
		return true;
	QFile::setPermissions(path_, QFile::permissions(path_) | QFileDevice::WriteOwner);
	QFile f(path_);
	if (!f.open(QIODevice::ReadWrite))
	{
		if (error)
			*error = QStringLiteral("Cannot write %1: %2").arg(path_, f.errorString());
		return false;
	}
	// Only the bytes that differ, so nothing else in the file can change.
	for (int i = 0; i < out.size(); i++)
		if (out[i] != exe_[i])
		{
			int end = i;
			while (end < out.size() && out[end] != exe_[end])
				end++;
			f.seek(i);
			f.write(out.constData() + i, end - i);
			i = end;
		}
	f.close();
	exe_ = out;
	return true;
}

// NPCDATA.DAT: a flex file whose record 0 holds the 256 records.
bool NpcDataFile::parse(const QByteArray &file)
{
	file_ = file;
	at_ = -1;
	Flex flex;
	if (!flex.setData(file, "NPCDATA.DAT", nullptr) || flex.size(0) < Count * RecordSize)
		return false;
	at_ = int(flex.data(0) - file_.constData());
	return true;
}

NpcStats NpcDataFile::get(int npc) const
{
	NpcStats s;
	if (!valid() || npc < 0 || npc >= Count)
		return s;
	const char *r = file_.constData() + at_ + npc * RecordSize;
	s.str = int8_t(r[0]);
	s.dex = int8_t(r[1]);
	s.intelligence = int8_t(r[2]);
	s.hp = uint8_t(r[3]);
	s.alignment = uint8_t(r[11]);
	s.flags1 = read16(r + 27);
	s.mana = int16_t(read16(r + 45));
	s.flags2 = read16(r + 47);
	return s;
}

void NpcDataFile::set(int npc, const NpcStats &s)
{
	if (!valid() || npc < 0 || npc >= Count)
		return;
	const int r = at_ + npc * RecordSize;
	auto put16 = [&](int at, int v) {
		file_[at] = char(v & 0xff);
		file_[at + 1] = char((v >> 8) & 0xff);
	};
	file_[r + 0] = char(s.str);
	file_[r + 1] = char(s.dex);
	file_[r + 2] = char(s.intelligence);
	file_[r + 3] = char(s.hp);
	file_[r + 11] = char(s.alignment);
	put16(r + 27, s.flags1);
	put16(r + 45, s.mana);
	put16(r + 47, s.flags2);
}
