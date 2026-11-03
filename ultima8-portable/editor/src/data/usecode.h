// What the viewer reads from EUSECODE.FLX: class names, and the teleports usecode makes with
// constant arguments.
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <climits>

#include "data/flex.h"

// Usecode classes: a shape's class is its number, a permanent NPC's is 1024 + its number, and
// a usecode egg's is 1151 + its quality (spawnUnk, u8/uprocess.cpp). Class c is flex record
// c + 2; record 1 holds the names, 13 bytes each after a 4 byte header.
enum { NpcClassBase = 1024, UsecodeEggClassBase = 1151 };

struct UsecodeTeleport
{
	enum Kind
	{
		ToEgg,		// teleportToEgg(map, egg, fade): moves the avatar onto a teleport egg
		NpcTeleport	// Npc::teleport(x, y, z, map) on some NPC, often the avatar
	};
	Kind kind;
	int classId;
	int map;
	int egg = -1;
	int x = 0, y = 0, z = 0;
};

// A read or a write of a global flag: opcodes 4e and 4f, a bit index and a width in bits
// (GlobalFlag, u8/flag.cpp).
struct FlagUse
{
	int bit;
	int width;
	bool write;
	int classId;
	int offset;		// of the instruction, in the class's code
	int value = INT_MIN;	// a write of a constant: the constant
};

class Usecode
{
public:
	bool load(const QString &path, QString *error);
	QString className(int classId) const;
	bool hasClass(int classId) const;
	const QVector<UsecodeTeleport> &teleports() const { return teleports_; }

	// The whole file, as the game reads it.
	const QByteArray &file() const { return file_; }
	int classCount() const { return names_.size(); }
	// A class's code: its record after the 12-byte header (event offsets, then instructions).
	QByteArray code(int classId) const;
	// The events the class handles: a bit each, from the overload table (usecode record 1).
	uint32_t events(int classId) const;
	// Where an event's routine starts in the class's code; 0 when it has none.
	int eventOffset(int classId, int event) const;

	// Every read and write of a global flag, in class and offset order.
	const QVector<FlagUse> &flagUses() const { return flagUses_; }
	// The routine an offset is in: the event's name ("use"), or "routine 1a2b".
	QString routineAt(int classId, int offset) const;
	// The strings the routine holding offset pushes, up to n before it and n after it.
	QStringList stringsAround(int classId, int offset, int n) const;

private:
	void scan(int classId, const char *code, int size);
	void index(int classId);

	Flex flex_;
	QByteArray file_;
	QVector<QString> names_;
	QVector<uint32_t> events_;
	QVector<UsecodeTeleport> teleports_;
	QVector<FlagUse> flagUses_;
	QHash<int, QVector<int>> entries_;			// a class's routine starts, sorted
	QHash<int, QVector<QPair<int, QString>>> strings_;	// a class's string pushes
};
