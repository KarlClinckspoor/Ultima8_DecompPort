// What the viewer reads from EUSECODE.FLX: class names, and the teleports usecode makes with
// constant arguments.
#pragma once

#include <QString>
#include <QVector>

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

class Usecode
{
public:
	bool load(const QString &path, QString *error);
	QString className(int classId) const;
	bool hasClass(int classId) const;
	const QVector<UsecodeTeleport> &teleports() const { return teleports_; }

private:
	void scan(int classId, const char *code, int size);

	Flex flex_;
	QVector<QString> names_;
	QVector<UsecodeTeleport> teleports_;
};
