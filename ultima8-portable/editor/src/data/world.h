// The game world as the data files describe it: every map's items, the ways between maps, and
// the statistics the game keeps elsewhere. It can be changed and written back.
#pragma once

#include <QSet>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>
#include <vector>

#include "data/flex.h"
#include "data/gametables.h"
#include "data/itemcache.h"
#include "data/shapes.h"
#include "data/usecode.h"

// Item status bits (item/item.h, item/itemcach.cpp).
enum ItemFlag : uint16_t
{
	FlagInDisplayList = 0x0001,
	FlagDisposable = 0x0002,
	FlagOwned = 0x0004,
	FlagContained = 0x0008,
	FlagInvisible = 0x0010,
	FlagFlipped = 0x0020,
	FlagInNpcList = 0x0040,
	FlagFastOnly = 0x0080,
	FlagGumpOpen = 0x0100,
	FlagEquipped = 0x0200,
	FlagEthereal = 0x0800,
	FlagFastArea = 0x2000,
	FlagHasGump = 0x8000,
};

QString flagNames(uint16_t flags);

// One item: the 16-byte record of FIXED.DAT, NONFIXED.DAT and the item cache (OneItem,
// item/glob.h), and where the editor found it.
struct MapItem
{
	enum Source { Fixed, NonFixed, Glob, Npc };

	uint16_t x = 0, y = 0;
	uint8_t z = 0;
	uint16_t shape = 0;
	uint8_t frame = 0;
	uint16_t flags = 0;
	uint16_t quality = 0;
	uint8_t npcNum = 0;
	uint8_t mapNum = 0;
	uint16_t next = 0;

	Source source = Fixed;
	int record = -1;	// its place in its file's map record, the slot it had in the item cache, or -1
	int parent = -1;	// the container holding it, or the glob egg that made it
	std::vector<int> contents;
	bool deleted = false;	// kept in the list so other items' indices stay put

	bool contained() const { return flags & FlagContained; }
	bool flipped() const { return flags & FlagFlipped; }
	bool onMap() const { return !deleted && !contained() && z < 0xfb; }
	// The fields of the record, without where it came from.
	bool sameRecord(const MapItem &o) const;
	void copyRecord(const MapItem &o);
};

// A teleport egg's fields (item/egg.cpp): frame 0 sends, frame 1 receives. A sender's egg id
// (mapNum) is the map it goes to and its quality's low byte the receiving egg's number; the
// quality's high byte asks for a fade. The hatch range is in the npcNum byte, x in the high
// nibble, y in the low, in 32 unit steps either side.
struct EggInfo
{
	static int xRange(const MapItem &i) { return i.npcNum >> 4; }
	static int yRange(const MapItem &i) { return i.npcNum & 0xf; }
	static bool sends(const MapItem &i) { return i.frame == 0; }
	static int teleportId(const MapItem &i) { return i.quality & 0xff; }
	static int destMap(const MapItem &i) { return i.mapNum; }
	// Monster eggs (item/monegg.cpp).
	static int monsterShape(const MapItem &i) { return i.quality & 0x7ff; }
	static int monsterProb(const MapItem &i) { return i.quality >> 11; }
	static int monsterCount(const MapItem &i) { return i.npcNum & 7; }
	static int monsterActivity(const MapItem &i) { return i.mapNum & 7; }
	static int monsterId(const MapItem &i) { return i.mapNum >> 3; }
};

struct MapData
{
	int number = 0;
	std::vector<MapItem> items;
	int fixedCount = 0;
	int nonFixedCount = 0;
	int globCount = 0;
	int npcCount = 0;
	int badCount = 0;
	bool fromCache = false;	// its movable items came from the saved game's item cache

	bool empty() const;
	// The teleport egg that receives teleport id, or -1.
	int receivingEgg(const Shapes &shapes, int id) const;
	// The item an item is (at last) inside of; the item itself when it is on the map.
	int root(int item) const;
	void recount();
};

// A way from one map to another.
struct Transition
{
	enum Kind
	{
		TeleportEgg,		// a sending teleport egg on the map
		UsecodeToEgg,		// usecode calling teleportToEgg
		UsecodeNpcTeleport	// usecode calling Npc::teleport with a fixed place
	};
	Kind kind = TeleportEgg;
	int fromMap = -1;		// -1: usecode that is not tied to any item on a map
	int fromItem = -1;		// the egg, or an item whose usecode does it
	int toMap = -1;
	int toEgg = -1;			// teleport id; -1 for a place
	int toItem = -1;		// the receiving egg, when it was found
	int x = 0, y = 0, z = 0;	// the place, for Npc::teleport and found eggs
	int classId = -1;		// the usecode class, for usecode transitions
};

class World
{
public:
	// dataDir holds STATIC and USECODE. savePath is a saved game (U8SAVE.nnn) or a GAMEDAT
	// folder (a game in progress); empty means SAVEGAME/U8SAVE.000, which new games start from.
	bool load(const QString &dataDir, const QString &savePath, QString *error);

	const Shapes &shapes() const { return shapes_; }
	const Usecode &usecode() const { return usecode_; }
	const MapData &map(int n) const;
	QVector<int> mapNumbers() const;	// maps with any items
	const QVector<Transition> &transitions() const;
	QString dataDir() const { return dataDir_; }
	QString saveSource() const { return saveSource_; }
	QString notes() const { return notes_; }

	// The usecode class an item runs (spawnUnk): its NPC class, usecode egg class or shape.
	int classOf(const MapItem &item) const;
	QString describe(const MapItem &item) const;

	// ---- Changing it ----

	// Why the item cannot be changed, or empty when it can.
	QString readOnlyReason(int map, int item) const;
	// Sets the item's record fields (not where it is in the lists).
	void setItem(int map, int item, const MapItem &fields);
	// A new item on the map, or in parent; returns its index. The file it goes in follows its
	// container, else its shape (fixed shapes in FIXED.DAT, the rest in NONFIXED.DAT).
	int addItem(int map, MapItem item, int parent = -1);
	// Takes the item and what it holds off the map, or puts them back.
	void setDeleted(int map, int item, bool deleted);

	ExeTables &exeTables() { return exe_; }
	const ExeTables &exeTables() const { return exe_; }
	bool hasNpcData() const { return npcData_.valid(); }
	NpcStats npcStats(int npc) const { return npcData_.get(npc); }
	void setNpcStats(int npc, const NpcStats &s);

	bool dirty() const;
	// What a save would write, for asking the user.
	QStringList pendingFiles() const;
	// Writes everything changed. The first time a file is written, the original is kept beside
	// it as name.orig.
	bool save(QString *error);
	// For tests: a map's items for one file as save would write them.
	QByteArray serializeForTest(int map, MapItem::Source source) const { return serialize(maps_[map], source); }

private:
	void loadRecords(MapData &map, const Flex &file, int record, MapItem::Source source);
	void loadFromCache(MapData &map);
	void expandGlob(MapData &map, int egg);
	void loadNpcs(const Flex &nonFixed);
	void findTransitions() const;
	void markDirty(int map, int item);
	QByteArray serialize(const MapData &map, MapItem::Source source) const;
	static QByteArray rebuildFlex(const QByteArray &original, const QVector<QByteArray> &records,
		bool fileSizeField);
	bool writeFile(const QString &path, const QByteArray &bytes, QString *error);
	bool backup(const QString &path, QString *error);

	QString dataDir_;
	QString staticDir_;
	QString saveSource_;
	bool saveIsDir_ = false;
	QString notes_;
	Shapes shapes_;
	Usecode usecode_;
	Flex globs_;
	QByteArray fixedFile_;
	QByteArray nonFixedFile_;
	QVector<QPair<QString, QByteArray>> saveFiles_;	// a saved game's files, in its order
	ItemCacheFile cache_;
	NpcDataFile npcData_;
	ExeTables exe_;
	std::vector<MapData> maps_;
	mutable QVector<Transition> transitions_;
	mutable bool transitionsStale_ = true;
	QSet<int> fixedDirty_;
	QSet<int> nonFixedDirty_;
	bool cacheDirty_ = false;
	bool npcDataDirty_ = false;
};
