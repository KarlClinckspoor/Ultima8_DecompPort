// ITEMCACH.DAT, the game's item slots as a saved game keeps them (ItemCache::save, load and
// dumpMapLevel in item/itemcach.cpp): record 0 holds the slot arrays, record 1 the map that was
// loaded, its region lists and the free lists.
#pragma once

#include <QByteArray>

#include <cstdint>
#include <vector>

#include "data/flex.h"

class Shapes;
struct MapItem;

class ItemCacheFile
{
public:
	enum { Slots = 9216, Npcs = 256, Regions = 64, ZGone = 0xfb, ZFree = 0xfe };

	bool parse(const QByteArray &file);
	bool valid() const { return valid_; }
	QByteArray bytes() const { return file_; }

	// The map that was loaded when the game was saved; -1 or 0 for none.
	int mapIn() const;
	bool slotFree(int r) const { return z(r) == ZFree; }
	MapItem slot(int r) const;
	// Writes the item's fields into slot r (not its links: next, or x of a contained item).
	void setSlot(int r, const MapItem &item);

	// A region's items in list order, and a container's contents in list order.
	std::vector<int> chain(int rx, int ry) const;
	std::vector<int> contents(int r, const Shapes &shapes) const;

	// Loads the map again from its records (16 bytes each, as FIXED.DAT and NONFIXED.DAT have
	// them), as the game does when it goes to a map: the loaded map leaves the slots as
	// dumpMapLevel takes it out, then loadMapLevel puts the records in. No region is marked as
	// loaded, so the game brings in the ones around the camera itself, expanding their globs.
	// The game keeps whatever map the cache says is loaded, so a change to that map has to be
	// made here as well as in the files.
	void reloadMap(int map, const QByteArray &fixedRecords, const QByteArray &nonFixedRecords,
		const Shapes &shapes);

private:
	uint16_t w16(int array, int r) const;
	void s16(int array, int r, uint16_t v);
	uint8_t b8(int array, int r) const;
	void s8(int array, int r, uint8_t v);
	uint16_t z(int r) const { return b8(4, r); }
	uint16_t head(int at) const;
	void setHead(int at, uint16_t v);
	void linkToFree(int r);
	void freeTree(int r, const Shapes &shapes);
	void unloadMap(const Shapes &shapes);
	void loadRecords(int map, const QByteArray &records, const Shapes &shapes);
	void putRecord(int r, const char *p);
	void pushEthereal(int r);
	int popEthereal();

	QByteArray file_;
	int slots_ = 0;		// record 0's offset in the file
	int lists_ = 0;		// record 1's
	bool valid_ = false;
};
