// The game world as the data files describe it: every map's items, and the ways between maps.
#pragma once

#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>
#include <vector>

#include "data/flex.h"
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
// item/glob.h), and where the viewer found it.
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
	int record = -1;	// its place in its file's map record, or the NPC's number
	int parent = -1;	// the container holding it, or the glob egg that made it
	std::vector<int> contents;

	bool contained() const { return flags & FlagContained; }
	bool flipped() const { return flags & FlagFlipped; }
	bool onMap() const { return !contained() && z < 0xfb; }
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

	bool empty() const { return items.empty(); }
	// The teleport egg that receives teleport id, or -1.
	int receivingEgg(const Shapes &shapes, int id) const;
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
	// folder; empty means GAMEDAT if it has NONFIXED.DAT, else SAVEGAME/U8SAVE.000.
	bool load(const QString &dataDir, const QString &savePath, QString *error);

	const Shapes &shapes() const { return shapes_; }
	const Usecode &usecode() const { return usecode_; }
	const MapData &map(int n) const;
	QVector<int> mapNumbers() const;	// maps with any items
	const QVector<Transition> &transitions() const { return transitions_; }
	QString dataDir() const { return dataDir_; }
	QString saveSource() const { return saveSource_; }
	QString notes() const { return notes_; }

	// The usecode class an item runs (spawnUnk): its NPC class, usecode egg class or shape.
	int classOf(const MapItem &item) const;
	QString describe(const MapItem &item) const;

private:
	void loadRecords(MapData &map, const Flex &file, int record, MapItem::Source source);
	void expandGlobs(MapData &map);
	void loadNpcs(const QByteArray &itemCache, const Flex &nonFixed);
	void findTransitions();

	QString dataDir_;
	QString saveSource_;
	QString notes_;
	Shapes shapes_;
	Usecode usecode_;
	Flex globs_;
	std::vector<MapData> maps_;
	QVector<Transition> transitions_;
};
