#include "data/world.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <functional>

enum
{
	MaxMaps = 256,
	MaxItems = 9216,	// ItemData's arrays (item/itemdata.h)
	MaxNpcs = 256,
	ZGone = 0xfb,		// an item out of play (item/itemcach.cpp)
	ZFree = 0xfe,		// a free item slot
	RecordBytes = 16,
	// ItemCache::loadMapLevel reads a map's items while the slots hold the NPCs and what they
	// carry, and globs near the camera expand into slots too; past this many a map may not load.
	MapItemLimit = 7000,
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

bool MapItem::sameRecord(const MapItem &o) const
{
	return x == o.x && y == o.y && z == o.z && shape == o.shape && frame == o.frame && flags == o.flags &&
		quality == o.quality && npcNum == o.npcNum && mapNum == o.mapNum && next == o.next;
}

void MapItem::copyRecord(const MapItem &o)
{
	x = o.x;
	y = o.y;
	z = o.z;
	shape = o.shape;
	frame = o.frame;
	flags = o.flags;
	quality = o.quality;
	npcNum = o.npcNum;
	mapNum = o.mapNum;
	next = o.next;
}

bool MapData::empty() const
{
	for (const MapItem &it : items)
		if (!it.deleted)
			return false;
	return true;
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

int MapData::root(int item) const
{
	while (item >= 0 && item < int(items.size()) && items[item].contained() && items[item].parent >= 0)
		item = items[item].parent;
	return item;
}

void MapData::recount()
{
	fixedCount = nonFixedCount = globCount = npcCount = 0;
	for (const MapItem &it : items)
	{
		if (it.deleted)
			continue;
		switch (it.source)
		{
		case MapItem::Fixed: fixedCount++; break;
		case MapItem::NonFixed: nonFixedCount++; break;
		case MapItem::Glob: globCount++; break;
		case MapItem::Npc: if (it.parent < 0) npcCount++; break;
		}
	}
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

static QByteArray readAll(const QString &path)
{
	QFile f(path);
	return !path.isEmpty() && f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
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

static void writeRecord(QByteArray &out, const MapItem &it)
{
	auto w16 = [&](uint16_t v) {
		out.append(char(v & 0xff));
		out.append(char(v >> 8));
	};
	w16(it.x);
	w16(it.y);
	out.append(char(it.z));
	w16(it.shape);
	out.append(char(it.frame));
	w16(it.flags);
	w16(it.quality);
	out.append(char(it.npcNum));
	out.append(char(it.mapNum));
	w16(it.next);
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
	staticDir_ = findEntry(dataDir, "STATIC");
	QString usecodeDir = findEntry(dataDir, "USECODE");
	if (staticDir_.isEmpty() || usecodeDir.isEmpty())
	{
		if (error)
			*error = QStringLiteral("%1 has no STATIC and USECODE folders. Choose the folder Ultima 8 is installed in.")
				.arg(QDir::toNativeSeparators(dataDir));
		return false;
	}
	if (!shapes_.load(staticDir_, error))
		return false;
	if (!usecode_.load(findEntry(usecodeDir, "EUSECODE.FLX"), error))
		return false;
	if (!globs_.load(findEntry(staticDir_, "GLOB.FLX"), error))
		return false;
	Flex fixed;
	fixedFile_ = readAll(findEntry(staticDir_, "FIXED.DAT"));
	if (!fixed.setData(fixedFile_, "FIXED.DAT", error))
		return false;

	// The movable items: the saved game asked for, else the one every new game starts from.
	QString source = savePath;
	if (source.isEmpty())
	{
		QString saves = findEntry(dataDir, "SAVEGAME");
		source = saves.isEmpty() ? QString() : findEntry(saves, "U8SAVE.000");
	}
	QByteArray itemCache, npcData;
	if (!source.isEmpty() && QFileInfo(source).isDir())
	{
		saveIsDir_ = true;
		nonFixedFile_ = readAll(findEntry(source, "NONFIXED.DAT"));
		itemCache = readAll(findEntry(source, "ITEMCACH.DAT"));
		npcData = readAll(findEntry(source, "NPCDATA.DAT"));
	}
	else if (!source.isEmpty())
	{
		SaveFile save;
		if (!save.load(source, error))
			return false;
		saveFiles_ = save.files();
		nonFixedFile_ = save.file("NONFIXED.DAT");
		itemCache = save.file("ITEMCACH.DAT");
		npcData = save.file("NPCDATA.DAT");
	}
	Flex nonFixed;
	if (!nonFixedFile_.isEmpty())
	{
		if (!nonFixed.setData(nonFixedFile_, "NONFIXED.DAT", error))
			return false;
		saveSource_ = source;
	}
	else
		notes_ += QStringLiteral("No saved game was found, so only the fixed items are shown and can be changed.\n");
	cache_.parse(itemCache);
	npcData_.parse(npcData);

	QString exeError;
	const QString exe = findEntry(dataDir, "U8.EXE");
	if (exe.isEmpty())
		notes_ += QStringLiteral("There is no U8.EXE in the game folder, so weapon, armour and monster statistics cannot be changed.\n");
	else if (!exe_.load(exe, &exeError))
		notes_ += exeError + QLatin1Char('\n');

	const int cachedMap = cache_.valid() ? cache_.mapIn() : -1;
	maps_.resize(MaxMaps);
	for (int m = 0; m < MaxMaps; m++)
	{
		MapData &map = maps_[m];
		map.number = m;
		if (m == 0)
			continue;	// map 0 of NONFIXED.DAT holds the NPCs (ItemCache::saveNpcs)
		loadRecords(map, fixed, m, MapItem::Fixed);
		if (m == cachedMap)
			loadFromCache(map);
		else
			loadRecords(map, nonFixed, m, MapItem::NonFixed);
		const size_t original = map.items.size();
		for (size_t e = 0; e < original; e++)
			expandGlob(map, int(e));
	}
	loadNpcs(nonFixed);
	for (MapData &m : maps_)
		m.recount();
	if (cachedMap > 0)
		notes_ += QStringLiteral("The game was saved on map %1, so its movable items come from the saved "
			"game's item cache, as the game would load them.\n").arg(cachedMap);
	transitionsStale_ = true;
	return true;
}

// Items in file order; a contained item follows its container, its x the depth it is at
// (ItemCache::loadMapLevel).
void World::loadRecords(MapData &map, const Flex &file, int record, MapItem::Source source)
{
	const char *p = file.data(record);
	const int count = file.size(record) / RecordBytes;
	int container = -1, depth = 0, last = -1;
	for (int i = 0; i < count; i++, p += RecordBytes)
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
	}
}

// The movable items of the map the game was saved on, in the order the game would write them to
// NONFIXED.DAT when it leaves the map (ItemCache::dumpMapLevel): region by region, each region's
// list in order, every item followed by what it holds. NPCs, fixed shapes and disposable items
// (glob items) are not among them.
void World::loadFromCache(MapData &map)
{
	map.fromCache = true;
	std::function<void(int, int)> addTree = [&](int slot, int parent) {
		MapItem it = cache_.slot(slot);
		it.source = MapItem::NonFixed;
		it.record = slot;
		it.parent = parent;
		map.items.push_back(it);
		const int at = int(map.items.size()) - 1;
		if (parent >= 0)
			map.items[parent].contents.push_back(at);
		for (int c : cache_.contents(slot, shapes_))
			addTree(c, at);
	};
	for (int ry = 0; ry < ItemCacheFile::Regions; ry++)
		for (int rx = 0; rx < ItemCacheFile::Regions; rx++)
			for (int r : cache_.chain(rx, ry))
			{
				const MapItem it = cache_.slot(r);
				if (r < MaxNpcs || it.shape >= 2048 || shapes_.type(it.shape).fixed || (it.flags & FlagDisposable))
					continue;
				addTree(r, -1);
			}
}

// A glob egg stands for a glob: a group of items placed relative to the 512 unit region the egg
// is in, x and y in 2 unit steps and z over the egg's (GlobExpander::expand, item/glob.cpp).
// A glob record is a 16-bit count, then x, y, z, a 16-bit shape and a frame for each item.
// The glob's items are made again from the egg as it is now; the old ones are dropped.
void World::expandGlob(MapData &map, int e)
{
	for (MapItem &it : map.items)
		if (it.source == MapItem::Glob && it.parent == e)
			it.deleted = true;
	const MapItem egg = map.items[e];
	if (!egg.onMap() || egg.source == MapItem::Glob || shapes_.type(egg.shape).family != FamilyGlobEgg)
		return;
	const char *g = globs_.data(egg.quality);
	const int size = globs_.size(egg.quality);
	if (!g || size < 2)
		return;
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
		it.parent = e;
		if (it.shape >= 2048)
			continue;
		map.items.push_back(it);
	}
}

// NPCs live in item slots 1-255, their map in mapNum. A saved game's ITEMCACH.DAT holds the slot
// arrays (ItemCache::save); without it, map 0 of NONFIXED.DAT has them as records, each
// non-contained one an NPC numbered by its npcNum (ItemCache::loadMapLevel).
void World::loadNpcs(const Flex &nonFixed)
{
	if (cache_.valid())
	{
		std::function<void(MapData &, int, int)> addTree = [&](MapData &to, int slot, int parent) {
			MapItem it = cache_.slot(slot);
			it.source = MapItem::Npc;
			it.record = slot;
			it.parent = parent;
			to.items.push_back(it);
			const int at = int(to.items.size()) - 1;
			if (parent >= 0)
				to.items[parent].contents.push_back(at);
			for (int c : cache_.contents(slot, shapes_))
				addTree(to, c, at);
		};
		for (int r = 1; r < MaxNpcs; r++)
		{
			if (cache_.slotFree(r))
				continue;
			const MapItem it = cache_.slot(r);
			if (it.contained() || it.z >= ZGone || it.shape >= 2048)
				continue;
			addTree(maps_[it.mapNum], r, -1);
		}
		return;
	}

	std::function<void(MapData &, const MapData &, int, int)> copyTree;
	copyTree = [&](MapData &to, const MapData &from, int index, int parent) {
		MapItem it = from.items[index];
		std::vector<int> kids = it.contents;
		it.contents.clear();
		it.parent = parent;
		it.record = -1;
		to.items.push_back(it);
		const int at = int(to.items.size()) - 1;
		if (parent >= 0)
			to.items[parent].contents.push_back(at);
		for (int k : kids)
			copyTree(to, from, k, at);
	};
	MapData npcs;
	loadRecords(npcs, nonFixed, 0, MapItem::Npc);
	for (size_t i = 0; i < npcs.items.size(); i++)
	{
		if (npcs.items[i].parent >= 0)
			continue;
		MapItem &it = npcs.items[i];
		copyTree(maps_[it.mapNum], npcs, int(i), -1);
		maps_[it.mapNum].items.back().record = -1;
	}
}

int World::classOf(const MapItem &item) const
{
	if (item.source == MapItem::Npc && !item.contained() && item.record > 0 && item.record < MaxNpcs &&
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

const QVector<Transition> &World::transitions() const
{
	if (transitionsStale_)
		findTransitions();
	return transitions_;
}

void World::findTransitions() const
{
	transitionsStale_ = false;
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

	// Usecode: the class's items say which maps the call can come from; one item a map is
	// enough.
	QSet<int> classes;
	for (const UsecodeTeleport &u : usecode_.teleports())
		classes.insert(u.classId);
	QHash<int, QVector<QPair<int, int>>> placed;	// class: (map, item) for each map
	for (const MapData &m : maps_)
	{
		QSet<int> here;
		for (size_t i = 0; i < m.items.size(); i++)
		{
			if (m.items[i].deleted)
				continue;
			const int c = classOf(m.items[i]);
			if (classes.contains(c) && !here.contains(c))
			{
				here.insert(c);
				placed[c] << qMakePair(m.number, int(i));
			}
		}
	}
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
		QVector<QPair<int, int>> from = placed.value(u.classId);
		if (from.isEmpty())
			from << qMakePair(-1, -1);
		for (const auto &f : from)
		{
			Transition here = t;
			here.fromMap = f.first;
			here.fromItem = f.second;
			const QString key = QStringLiteral("%1/%2/%3/%4/%5/%6/%7").arg(here.kind).arg(here.fromMap)
				.arg(here.classId).arg(here.toMap).arg(here.toEgg).arg(here.x).arg(here.y);
			if (!seen.contains(key))
			{
				seen.insert(key);
				transitions_ << here;
			}
		}
	}
}

// ---- Changing it ----

QString World::readOnlyReason(int m, int item) const
{
	const MapData &d = map(m);
	if (item < 0 || item >= int(d.items.size()))
		return QStringLiteral("No item.");
	const MapItem &it = d.items[item];
	if (it.source == MapItem::Glob)
		return QStringLiteral("This item is part of glob %1, which its glob egg places. Change or move the egg.")
			.arg(it.parent >= 0 ? d.items[it.parent].quality : -1);
	if (it.source == MapItem::Npc && !cache_.valid())
		return QStringLiteral("NPCs can only be changed in a saved game that has ITEMCACH.DAT.");
	if (it.source == MapItem::Npc && it.parent >= 0)
		return QStringLiteral("What NPCs carry cannot be changed yet.");
	if (it.source == MapItem::NonFixed && saveSource_.isEmpty())
		return QStringLiteral("There is no saved game to write movable items to.");
	return QString();
}

// Edits to the map the game was saved on also mean taking it out of the item cache, so the
// game loads it from the files again; that is done when saving.
void World::markDirty(int m, int item)
{
	MapData &d = maps_[m];
	const MapItem &top = d.items[d.root(item)];
	switch (top.source)
	{
	case MapItem::Fixed:
		fixedDirty_.insert(m);
		break;
	case MapItem::NonFixed:
		nonFixedDirty_.insert(m);
		break;
	case MapItem::Npc:
		cacheDirty_ = true;
		break;
	case MapItem::Glob:
		break;
	}
	if (d.fromCache)
		nonFixedDirty_.insert(m);
	transitionsStale_ = true;
}

void World::setItem(int m, int item, const MapItem &fields)
{
	MapData &d = maps_[m];
	MapItem &it = d.items[item];
	const bool wasGlobEgg = shapes_.type(it.shape).family == FamilyGlobEgg;
	markDirty(m, item);
	it.copyRecord(fields);
	if (it.source == MapItem::Npc && it.record > 0)
		cache_.setSlot(it.record, it);
	// A top-level item whose shape stops or starts being fixed moves to the other file.
	if (it.parent < 0 && (it.source == MapItem::Fixed || it.source == MapItem::NonFixed))
	{
		const MapItem::Source want = shapes_.type(it.shape).fixed ? MapItem::Fixed : MapItem::NonFixed;
		if (want != it.source)
		{
			std::function<void(int)> move = [&](int i) {
				d.items[i].source = want;
				for (int c : d.items[i].contents)
					move(c);
			};
			move(item);
			markDirty(m, item);
		}
	}
	if (wasGlobEgg || shapes_.type(it.shape).family == FamilyGlobEgg)
		expandGlob(d, item);
	d.recount();
}

int World::addItem(int m, MapItem item, int parent)
{
	MapData &d = maps_[m];
	item.deleted = false;
	item.contents.clear();
	item.parent = parent;
	item.record = -1;
	if (parent >= 0)
	{
		item.source = d.items[parent].source;
		item.flags |= FlagContained;
	}
	else
	{
		item.source = shapes_.type(item.shape).fixed ? MapItem::Fixed : MapItem::NonFixed;
		item.flags &= ~FlagContained;
	}
	d.items.push_back(item);
	const int at = int(d.items.size()) - 1;
	if (parent >= 0)
		d.items[parent].contents.push_back(at);
	markDirty(m, at);
	expandGlob(d, at);
	d.recount();
	return at;
}

void World::setDeleted(int m, int item, bool deleted)
{
	MapData &d = maps_[m];
	std::function<void(int)> mark = [&](int i) {
		d.items[i].deleted = deleted;
		for (int c : d.items[i].contents)
			mark(c);
	};
	mark(item);
	markDirty(m, item);
	expandGlob(d, item);
	d.recount();
}

void World::setNpcStats(int npc, const NpcStats &s)
{
	npcData_.set(npc, s);
	npcDataDirty_ = true;
}

bool World::dirty() const
{
	return !fixedDirty_.isEmpty() || !nonFixedDirty_.isEmpty() || cacheDirty_ || npcDataDirty_ || exe_.dirty();
}

QStringList World::pendingFiles() const
{
	QStringList out;
	if (!fixedDirty_.isEmpty())
		out << QStringLiteral("STATIC/FIXED.DAT");
	if (!nonFixedDirty_.isEmpty() || cacheDirty_ || npcDataDirty_)
		out << QDir(dataDir_).relativeFilePath(saveSource_);
	if (exe_.dirty())
		out << QStringLiteral("U8.EXE");
	return out;
}

// A map's items for one file: those on the map in their order, each followed by what it holds
// with x set to its depth, as dumpMapLevel writes them.
QByteArray World::serialize(const MapData &map, MapItem::Source source) const
{
	QByteArray out;
	std::function<void(int, int)> put = [&](int i, int depth) {
		MapItem it = map.items[i];
		bool holds = false;
		for (int c : it.contents)
			holds |= !map.items[c].deleted;
		const int family = shapes_.type(it.shape).family;
		if ((family == FamilyQuantity || family == FamilyReagent) && it.quality == 0)
			it.quality = 1;
		if (holds)
			it.quality = 0;
		it.flags &= ~(FlagGumpOpen | FlagHasGump);
		if (depth > 0)
		{
			it.x = uint16_t(depth);
			it.flags |= FlagContained;
		}
		else
			it.flags &= ~FlagContained;
		writeRecord(out, it);
		for (int c : map.items[i].contents)
			if (!map.items[c].deleted)
				put(c, depth + 1);
	};
	for (size_t i = 0; i < map.items.size(); i++)
	{
		const MapItem &it = map.items[i];
		if (!it.deleted && it.parent < 0 && it.source == source)
			put(int(i), 0);
	}
	return out;
}

// A flex file with new records, its header kept: the count, and (for FIXED.DAT) the size and the
// checksum FlexFile::getChecksum would give: the signed sum of the bytes from the end of the
// header to 0x80 before the end of the file.
QByteArray World::rebuildFlex(const QByteArray &original, const QVector<QByteArray> &records, bool fileSizeField)
{
	QByteArray out = original.left(0x80);
	out.resize(0x80);
	const int count = records.size();
	auto put32 = [](QByteArray &b, int at, uint32_t v) {
		for (int i = 0; i < 4; i++)
			b[at + i] = char((v >> (8 * i)) & 0xff);
	};
	put32(out, 0x54, uint32_t(count));
	QByteArray index(count * 8, '\0');
	uint32_t at = uint32_t(0x80 + count * 8);
	QByteArray data;
	for (int i = 0; i < count; i++)
	{
		if (!records[i].isEmpty())
		{
			put32(index, i * 8, at + uint32_t(data.size()));
			put32(index, i * 8 + 4, uint32_t(records[i].size()));
			data.append(records[i]);
		}
	}
	out.append(index);
	out.append(data);
	if (fileSizeField)
		put32(out, 0x5c, uint32_t(out.size()));
	int32_t sum = 0;
	for (int i = 0x80; i < out.size() - 0x80; i++)
		sum += int8_t(out[i]);
	put32(out, 0x60, uint32_t(sum));
	return out;
}

bool World::backup(const QString &path, QString *error)
{
	const QString orig = path + QStringLiteral(".orig");
	if (QFile::exists(orig) || !QFile::exists(path))
		return true;
	if (!QFile::copy(path, orig))
	{
		if (error)
			*error = QStringLiteral("Cannot keep a copy of %1 as %2").arg(path, orig);
		return false;
	}
	return true;
}

bool World::writeFile(const QString &path, const QByteArray &bytes, QString *error)
{
	if (!backup(path, error))
		return false;
	// Installed game files are often read-only.
	if (QFile::exists(path))
		QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::WriteOwner);
	QSaveFile f(path);
	if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
	{
		if (error)
			*error = QStringLiteral("Cannot write %1: %2").arg(path, f.errorString());
		return false;
	}
	return true;
}

bool World::save(QString *error)
{
	for (int m : fixedDirty_ + nonFixedDirty_)
	{
		const MapData &d = maps_[m];
		int count = 0;
		for (const MapItem &it : d.items)
			count += !it.deleted && it.source != MapItem::Glob;
		if (count > MapItemLimit)
		{
			if (error)
				*error = QStringLiteral("Map %1 has %2 items; the game cannot load more than about %3 at once.")
					.arg(m).arg(count).arg(MapItemLimit);
			return false;
		}
	}

	// The map the saved game was made on is in its item cache, which is what the game loads: a
	// change to that map is made there too, by loading it again from the records written here.
	const int cached = cache_.valid() ? cache_.mapIn() : -1;
	const bool reload = cached > 0 && (fixedDirty_.contains(cached) || nonFixedDirty_.contains(cached));
	if (reload)
		nonFixedDirty_.insert(cached);

	QByteArray fixedOut = fixedFile_;
	if (!fixedDirty_.isEmpty())
	{
		Flex fixed;
		fixed.setData(fixedFile_, "FIXED.DAT", nullptr);
		QVector<QByteArray> records;
		for (int m = 0; m < fixed.count(); m++)
			records << (fixedDirty_.contains(m) ? serialize(maps_[m], MapItem::Fixed) : fixed.record(m));
		fixedOut = rebuildFlex(fixedFile_, records, true);
		if (!writeFile(findEntry(staticDir_, "FIXED.DAT"), fixedOut, error))
			return false;
		fixedFile_ = fixedOut;
		fixedDirty_.clear();
	}

	if (!nonFixedDirty_.isEmpty() || cacheDirty_ || npcDataDirty_)
	{
		Flex nonFixed;
		nonFixed.setData(nonFixedFile_, "NONFIXED.DAT", nullptr);
		QVector<QByteArray> records;
		for (int m = 0; m < nonFixed.count(); m++)
			records << (nonFixedDirty_.contains(m) ? serialize(maps_[m], MapItem::NonFixed) : nonFixed.record(m));
		QByteArray nonFixedOut = nonFixedDirty_.isEmpty() ? nonFixedFile_ : rebuildFlex(nonFixedFile_, records, false);
		ItemCacheFile cache = cache_;
		if (reload)
		{
			Flex fixed;
			fixed.setData(fixedOut, "FIXED.DAT", nullptr);
			cache.reloadMap(cached, fixed.record(cached), records[cached], shapes_);
		}
		const bool cacheChanged = reload || cacheDirty_;

		if (saveIsDir_)
		{
			if (!nonFixedDirty_.isEmpty() &&
				!writeFile(QDir(saveSource_).filePath(QFileInfo(findEntry(saveSource_, "NONFIXED.DAT")).fileName()), nonFixedOut, error))
				return false;
			if (cacheChanged && !writeFile(findEntry(saveSource_, "ITEMCACH.DAT"), cache.bytes(), error))
				return false;
			if (npcDataDirty_ && !writeFile(findEntry(saveSource_, "NPCDATA.DAT"), npcData_.bytes(), error))
				return false;
		}
		else
		{
			QVector<QPair<QString, QByteArray>> files = saveFiles_;
			for (auto &f : files)
			{
				if (f.first.compare("NONFIXED.DAT", Qt::CaseInsensitive) == 0)
					f.second = nonFixedOut;
				else if (f.first.compare("ITEMCACH.DAT", Qt::CaseInsensitive) == 0 && cacheChanged)
					f.second = cache.bytes();
				else if (f.first.compare("NPCDATA.DAT", Qt::CaseInsensitive) == 0 && npcDataDirty_)
					f.second = npcData_.bytes();
			}
			if (!writeFile(saveSource_, SaveFile::build(files), error))
				return false;
			saveFiles_ = files;
		}
		nonFixedFile_ = nonFixedOut;
		cache_ = cache;
		nonFixedDirty_.clear();
		cacheDirty_ = false;
		npcDataDirty_ = false;
	}

	if (exe_.dirty())
	{
		if (!backup(exe_.path(), error) || !exe_.save(error))
			return false;
	}
	return true;
}
