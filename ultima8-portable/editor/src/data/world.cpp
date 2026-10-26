#include "data/world.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStringList>

#include <functional>

enum
{
	MaxMaps = 256,
	MaxItems = 9216,	// ItemData's arrays (item/itemdata.h)
	MaxNpcs = 256,
	ZGone = 0xfb,		// an item out of play (item/itemcach.cpp)
	ZFree = 0xfe,		// a free item slot
};

QString flagNames(uint16_t flags)
{
	static const struct { uint16_t bit; const char *name; } names[] = {
		{FlagInDisplayList, "in display list"}, {FlagDisposable, "disposable"}, {FlagOwned, "owned"},
		{FlagContained, "contained"}, {FlagInvisible, "invisible"}, {FlagFlipped, "flipped"},
		{FlagInNpcList, "in NPC list"}, {FlagFastOnly, "fast only"}, {FlagGumpOpen, "gump open"},
		{FlagEquipped, "equipped"}, {FlagEthereal, "ethereal"}, {FlagFastArea, "fast area"},
		{FlagHasGump, "has gump"},
	};
	QStringList out;
	uint16_t known = 0;
	for (const auto &n : names)
	{
		known |= n.bit;
		if (flags & n.bit)
			out << QString::fromLatin1(n.name);
	}
	if (flags & ~known)
		out << QStringLiteral("0x%1").arg(flags & ~known, 4, 16, QLatin1Char('0'));
	return out.isEmpty() ? QStringLiteral("none") : out.join(QStringLiteral(", "));
}

int MapData::receivingEgg(const Shapes &shapes, int id) const
{
	for (size_t i = 0; i < items.size(); i++)
	{
		const MapItem &it = items[i];
		if (it.onMap() && shapes.type(it.shape).family == FamilyTeleportEgg && it.frame == 1 &&
			EggInfo::teleportId(it) == id)
			return int(i);
	}
	return -1;
}

// DOS names are case blind; on disk they may have either case.
static QString findEntry(const QString &dir, const QString &name)
{
	QDir d(dir);
	if (d.exists(name))
		return d.filePath(name);
	for (const QString &entry : d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot))
		if (entry.compare(name, Qt::CaseInsensitive) == 0)
			return d.filePath(entry);
	return QString();
}

static MapItem readRecord(const char *p)
{
	MapItem it;
	it.x = read16(p);
	it.y = read16(p + 2);
	it.z = uint8_t(p[4]);
	it.shape = read16(p + 5);
	it.frame = uint8_t(p[7]);
	it.flags = read16(p + 8);
	it.quality = read16(p + 10);
	it.npcNum = uint8_t(p[12]);
	it.mapNum = uint8_t(p[13]);
	it.next = read16(p + 14);
	return it;
}

const MapData &World::map(int n) const
{
	static const MapData none;
	if (n < 0 || n >= int(maps_.size()))
		return none;
	return maps_[n];
}

QVector<int> World::mapNumbers() const
{
	QVector<int> out;
	for (const MapData &m : maps_)
		if (!m.empty())
			out << m.number;
	return out;
}

bool World::load(const QString &dataDir, const QString &savePath, QString *error)
{
	dataDir_ = dataDir;
	QString staticDir = findEntry(dataDir, "STATIC");
	QString usecodeDir = findEntry(dataDir, "USECODE");
	if (staticDir.isEmpty() || usecodeDir.isEmpty())
	{
		if (error)
			*error = QStringLiteral("%1 has no STATIC and USECODE folders. Choose the folder Ultima 8 is installed in.")
				.arg(QDir::toNativeSeparators(dataDir));
		return false;
	}
	if (!shapes_.load(staticDir, error))
		return false;
	if (!usecode_.load(findEntry(usecodeDir, "EUSECODE.FLX"), error))
		return false;
	if (!globs_.load(findEntry(staticDir, "GLOB.FLX"), error))
		return false;
	Flex fixed;
	if (!fixed.load(findEntry(staticDir, "FIXED.DAT"), error))
		return false;

	// The movable items: a saved game, a GAMEDAT folder, or the game's own choice.
	QString source = savePath;
	if (source.isEmpty())
	{
		QString gamedat = findEntry(dataDir, "GAMEDAT");
		if (!gamedat.isEmpty() && !findEntry(gamedat, "NONFIXED.DAT").isEmpty())
			source = gamedat;
		else
		{
			QString saves = findEntry(dataDir, "SAVEGAME");
			source = saves.isEmpty() ? QString() : findEntry(saves, "U8SAVE.000");
		}
	}
	QByteArray nonFixedBytes, itemCache;
	if (!source.isEmpty() && QFileInfo(source).isDir())
	{
		QFile nf(findEntry(source, "NONFIXED.DAT"));
		if (nf.open(QIODevice::ReadOnly))
			nonFixedBytes = nf.readAll();
		QFile ic(findEntry(source, "ITEMCACH.DAT"));
		if (ic.open(QIODevice::ReadOnly))
			itemCache = ic.readAll();
	}
	else if (!source.isEmpty())
	{
		SaveFile save;
		if (!save.load(source, error))
			return false;
		nonFixedBytes = save.file("NONFIXED.DAT");
		itemCache = save.file("ITEMCACH.DAT");
	}
	Flex nonFixed;
	if (!nonFixedBytes.isEmpty())
	{
		if (!nonFixed.setData(nonFixedBytes, "NONFIXED.DAT", error))
			return false;
		saveSource_ = source;
	}
	else
		notes_ += QStringLiteral("No saved game was found, so only the fixed items are shown.\n");

	maps_.resize(MaxMaps);
	for (int m = 0; m < MaxMaps; m++)
	{
		MapData &map = maps_[m];
		map.number = m;
		if (m == 0)
			continue;	// map 0 of NONFIXED.DAT holds the NPCs (ItemCache::saveNpcs)
		loadRecords(map, fixed, m, MapItem::Fixed);
		loadRecords(map, nonFixed, m, MapItem::NonFixed);
		expandGlobs(map);
	}
	loadNpcs(itemCache, nonFixed);
	findTransitions();
	return true;
}

// Items in file order; a contained item follows its container, its x the depth it is at
// (ItemCache::loadMapLevel).
void World::loadRecords(MapData &map, const Flex &file, int record, MapItem::Source source)
{
	const char *p = file.data(record);
	const int count = file.size(record) / 16;
	int container = -1, depth = 0, last = -1;
	for (int i = 0; i < count; i++, p += 16)
	{
		MapItem it = readRecord(p);
		if (it.shape == 0)
			continue;
		if (int16_t(it.x) < 0 || int16_t(it.y) < 0 || it.z >= ZGone || it.shape >= 2048 ||
			(it.npcNum == 0xff && record != 0))
		{
			map.badCount++;
			continue;
		}
		it.source = source;
		it.record = i;
		if (it.contained())
		{
			if (it.x > depth && last >= 0)
			{
				container = last;
				depth = it.x;
			}
			while (it.x < depth && container >= 0)
			{
				container = map.items[container].parent;
				depth--;
			}
			it.parent = container;
		}
		else
		{
			depth = 0;
			container = -1;
		}
		map.items.push_back(it);
		last = int(map.items.size()) - 1;
		if (it.parent >= 0)
			map.items[it.parent].contents.push_back(last);
		if (source == MapItem::Fixed)
			map.fixedCount++;
		else
			map.nonFixedCount++;
	}
}

// A glob egg stands for a glob: a group of items placed relative to the 512 unit region the egg
// is in, x and y in 2 unit steps and z over the egg's (GlobExpander::expand, item/glob.cpp).
// A glob record is a 16-bit count, then x, y, z, a 16-bit shape and a frame for each item.
void World::expandGlobs(MapData &map)
{
	const size_t original = map.items.size();
	for (size_t e = 0; e < original; e++)
	{
		const MapItem egg = map.items[e];
		if (egg.contained() || shapes_.type(egg.shape).family != FamilyGlobEgg)
			continue;
		const char *g = globs_.data(egg.quality);
		const int size = globs_.size(egg.quality);
		if (!g || size < 2)
			continue;
		const int count = read16(g);
		const uint16_t x0 = (egg.x & ~0x1ff) + 1;
		const uint16_t y0 = (egg.y & ~0x1ff) + 1;
		for (int i = 0; i < count && 2 + (i + 1) * 6 <= size; i++)
		{
			const char *r = g + 2 + i * 6;
			MapItem it;
			it.x = uint16_t(uint8_t(r[0]) * 2 + x0);
			it.y = uint16_t(uint8_t(r[1]) * 2 + y0);
			it.z = uint8_t(uint8_t(r[2]) + egg.z);
			it.shape = read16(r + 3);
			it.frame = uint8_t(r[5]);
			it.flags = FlagDisposable | FlagFastOnly;
			it.source = MapItem::Glob;
			it.record = i;
			it.parent = int(e);
			if (it.shape >= 2048)
				continue;
			map.items.push_back(it);
			map.globCount++;
		}
	}
}

// NPCs live in item slots 1-255, their map in mapNum. A saved game's ITEMCACH.DAT holds the slot
// arrays (ItemCache::save); without it, map 0 of NONFIXED.DAT has them as records, each
// non-contained one an NPC numbered by its npcNum (ItemCache::loadMapLevel).
void World::loadNpcs(const QByteArray &itemCache, const Flex &nonFixed)
{
	std::function<void(MapData &, const MapData &, int, int)> copyTree;
	copyTree = [&](MapData &to, const MapData &from, int index, int parent) {
		MapItem it = from.items[index];
		std::vector<int> kids = it.contents;
		it.contents.clear();
		it.parent = parent;
		to.items.push_back(it);
		const int at = int(to.items.size()) - 1;
		if (parent >= 0)
			to.items[parent].contents.push_back(at);
		for (int k : kids)
			copyTree(to, from, k, at);
	};

	Flex cache;
	if (!itemCache.isEmpty() && cache.setData(itemCache, "ITEMCACH.DAT", nullptr) &&
		cache.size(0) >= MaxItems * 16)
	{
		const char *a = cache.data(0);
		// The arrays one after another: x, y (16 bits), z (8), shape (16), frame (8), status,
		// quality (16), npcNum, mapNum (8), next (16).
		auto w16 = [&](int at, int ref) { return read16(a + at + ref * 2); };
		auto b8 = [&](int at, int ref) { return uint8_t(a[at + ref]); };
		const int M = MaxItems;
		MapData all;
		std::vector<int> slotOf(MaxItems, -1);
		for (int ref = 1; ref < MaxItems; ref++)
		{
			MapItem it;
			it.z = b8(4 * M, ref);
			if (it.z == ZFree)
				continue;
			it.x = w16(0, ref);
			it.y = w16(2 * M, ref);
			it.shape = w16(5 * M, ref);
			it.frame = b8(7 * M, ref);
			it.flags = w16(8 * M, ref);
			it.quality = w16(10 * M, ref);
			it.npcNum = b8(12 * M, ref);
			it.mapNum = b8(13 * M, ref);
			it.next = w16(14 * M, ref);
			it.source = MapItem::Npc;
			it.record = ref;
			slotOf[ref] = int(all.items.size());
			all.items.push_back(it);
		}
		// A contained item's x is its container's slot (Item::getContainer).
		for (size_t i = 0; i < all.items.size(); i++)
		{
			MapItem &it = all.items[i];
			if (it.contained() && it.x < MaxItems && slotOf[it.x] >= 0)
			{
				it.parent = slotOf[it.x];
				all.items[it.parent].contents.push_back(int(i));
			}
		}
		for (size_t i = 0; i < all.items.size(); i++)
		{
			const MapItem &it = all.items[i];
			if (it.record >= MaxNpcs || it.contained() || it.z >= ZGone || it.shape >= 2048)
				continue;
			MapData &to = maps_[it.mapNum];
			copyTree(to, all, int(i), -1);
			to.npcCount++;
		}
		const char *head = cache.data(1);
		if (head && cache.size(1) >= 2)
		{
			int mapIn = int16_t(read16(head));
			if (mapIn > 0 && mapIn < MaxMaps)
				notes_ += QStringLiteral("The game was saved on map %1. That map's movable items are shown "
					"as NONFIXED.DAT last had them, which can be older than the game's own state.\n").arg(mapIn);
		}
		return;
	}

	MapData npcs;
	loadRecords(npcs, nonFixed, 0, MapItem::Npc);
	for (size_t i = 0; i < npcs.items.size(); i++)
	{
		MapItem &it = npcs.items[i];
		if (it.parent >= 0)
			continue;
		it.record = it.npcNum;
		MapData &to = maps_[it.mapNum];
		copyTree(to, npcs, int(i), -1);
		to.npcCount++;
	}
}

int World::classOf(const MapItem &item) const
{
	if (item.source == MapItem::Npc && !item.contained() && item.record < MaxNpcs &&
		!(item.flags & FlagFastOnly))
		return NpcClassBase + item.record;
	if (shapes_.type(item.shape).family == FamilyUsecodeEgg)
		return UsecodeEggClassBase + item.quality;
	return item.shape;
}

QString World::describe(const MapItem &item) const
{
	QString name = usecode_.className(classOf(item));
	if (name.isEmpty())
		name = usecode_.className(item.shape);
	if (name.isEmpty())
		name = familyName(shapes_.type(item.shape).family);
	return QStringLiteral("%1 (shape %2:%3)").arg(name).arg(item.shape).arg(item.frame);
}

void World::findTransitions()
{
	transitions_.clear();
	for (const MapData &m : maps_)
	{
		for (size_t i = 0; i < m.items.size(); i++)
		{
			const MapItem &it = m.items[i];
			if (!it.onMap() || shapes_.type(it.shape).family != FamilyTeleportEgg || !EggInfo::sends(it))
				continue;
			Transition t;
			t.kind = Transition::TeleportEgg;
			t.fromMap = m.number;
			t.fromItem = int(i);
			t.toMap = EggInfo::destMap(it);
			t.toEgg = EggInfo::teleportId(it);
			t.toItem = map(t.toMap).receivingEgg(shapes_, t.toEgg);
			if (t.toItem >= 0)
			{
				const MapItem &d = map(t.toMap).items[t.toItem];
				t.x = d.x;
				t.y = d.y;
				t.z = d.z;
			}
			transitions_ << t;
		}
	}

	// Usecode: the class's items say which maps the call can come from.
	QSet<QString> seen;
	for (const UsecodeTeleport &u : usecode_.teleports())
	{
		Transition t;
		t.kind = u.kind == UsecodeTeleport::ToEgg ? Transition::UsecodeToEgg : Transition::UsecodeNpcTeleport;
		t.classId = u.classId;
		t.toMap = u.map;
		if (u.kind == UsecodeTeleport::ToEgg)
		{
			t.toEgg = u.egg;
			t.toItem = map(t.toMap).receivingEgg(shapes_, t.toEgg);
			if (t.toItem >= 0)
			{
				const MapItem &d = map(t.toMap).items[t.toItem];
				t.x = d.x;
				t.y = d.y;
				t.z = d.z;
			}
		}
		else
		{
			t.x = u.x;
			t.y = u.y;
			t.z = u.z;
		}
		bool placed = false;
		for (const MapData &m : maps_)
		{
			for (size_t i = 0; i < m.items.size(); i++)
			{
				if (classOf(m.items[i]) != u.classId)
					continue;
				Transition here = t;
				here.fromMap = m.number;
				here.fromItem = int(i);
				QString key = QStringLiteral("%1/%2/%3/%4/%5/%6/%7").arg(here.kind).arg(here.fromMap)
					.arg(here.classId).arg(here.toMap).arg(here.toEgg).arg(here.x).arg(here.y);
				if (!seen.contains(key))
				{
					seen.insert(key);
					transitions_ << here;
				}
				placed = true;
				break;	// one item a map is enough to say the map has the way out
			}
		}
		if (!placed)
		{
			QString key = QStringLiteral("%1/-1/%2/%3/%4/%5/%6").arg(t.kind).arg(t.classId).arg(t.toMap)
				.arg(t.toEgg).arg(t.x).arg(t.y);
			if (!seen.contains(key))
			{
				seen.insert(key);
				transitions_ << t;
			}
		}
	}
}
