// An item's name as the game gives it: what the item says when the Avatar looks at it (its
// class's look event, which barks the name, often chosen by quality or frame: a key's quality
// says which door it opens). The event runs in the editor's usecode machine against the item's
// fields and the saved game's flags; one that needs more of the world than that (finding other
// items, asking the player) gives no name.
//
// The full name is the one the item has once the story has got far enough: the event runs with
// every flag set, so a person the Avatar has not met yet is still named. When that run gives no
// name, it is the name the saved game's flags give.
#pragma once

#include <QHash>
#include <QPair>
#include <QString>

class World;
struct MapItem;

class ItemNames
{
public:
	explicit ItemNames(const World &world, bool full) : world_(world), full_(full) {}

	// The name the item barks when looked at, trimmed; empty when it barks none.
	QString name(int map, const MapItem &item) const;
	// The same, not cached, saying why there is none.
	QString look(int map, const MapItem &item, QString *why) const;
	// Forgets the names (after the item, its class or the flags change).
	void clear() { cache_.clear(); }

private:
	const World &world_;
	bool full_;
	// By class, shape, frame, quality, npcNum, mapNum, referent and map.
	mutable QHash<QPair<quint64, quint64>, QString> cache_;
};
