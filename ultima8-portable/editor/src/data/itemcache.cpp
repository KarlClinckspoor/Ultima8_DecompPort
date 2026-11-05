#include "data/itemcache.h"

#include "data/shapes.h"
#include "data/world.h"

// Record 0: the arrays one after another, at these multiples of the slot count: x, y (16 bits),
// z (8), shape (16), frame (8), status, quality (16), npcNum, mapNum (8), next (16).
enum { ArrX = 0, ArrY = 2, ArrZ = 4, ArrShape = 5, ArrFrame = 7, ArrStatus = 8, ArrQuality = 10,
	ArrNpcNum = 12, ArrMapNum = 13, ArrNext = 14, SlotBytes = 16 };

// Record 1: mapIn, regionsIn (four words), regionStartHash[64][64], itemCount[64][64], then
// freeCount, freeNpcCount, firstFree, firstFreeNpc, lastFree, lastFreeNpc, etherealTop and
// etherealCount.
enum { ListMapIn = 0, ListStarts = 10, ListCounts = ListStarts + 64 * 64 * 2,
	ListFreeCount = ListCounts + 64 * 64 * 2, ListFreeNpcCount = ListFreeCount + 2,
	ListFirstFree = ListFreeNpcCount + 2, ListFirstFreeNpc = ListFirstFree + 2,
	ListLastFree = ListFirstFreeNpc + 2, ListLastFreeNpc = ListLastFree + 2,
	ListEtherealTop = ListLastFreeNpc + 2, ListEtherealCount = ListEtherealTop + 2,
	ListBytes = ListEtherealCount + 2 };

static void put16(QByteArray &b, int at, uint16_t v)
{
	b[at] = char(v & 0xff);
	b[at + 1] = char(v >> 8);
}

bool ItemCacheFile::parse(const QByteArray &file)
{
	valid_ = false;
	file_ = file;
	Flex flex;
	if (!flex.setData(file, "ITEMCACH.DAT", nullptr) || flex.count() < 2 ||
		flex.size(0) < Slots * SlotBytes || flex.size(1) < ListBytes)
		return false;
	slots_ = int(flex.data(0) - file_.constData());
	lists_ = int(flex.data(1) - file_.constData());
	valid_ = true;
	return true;
}

uint16_t ItemCacheFile::w16(int array, int r) const
{
	return read16(file_.constData() + slots_ + array * Slots + r * 2);
}

void ItemCacheFile::s16(int array, int r, uint16_t v)
{
	put16(file_, slots_ + array * Slots + r * 2, v);
}

uint8_t ItemCacheFile::b8(int array, int r) const
{
	return uint8_t(file_[slots_ + array * Slots + r]);
}

void ItemCacheFile::s8(int array, int r, uint8_t v)
{
	file_[slots_ + array * Slots + r] = char(v);
}

uint16_t ItemCacheFile::head(int at) const
{
	return read16(file_.constData() + lists_ + at);
}

void ItemCacheFile::setHead(int at, uint16_t v)
{
	put16(file_, lists_ + at, v);
}

int ItemCacheFile::mapIn() const
{
	return valid_ ? int16_t(head(ListMapIn)) : -1;
}

MapItem ItemCacheFile::slot(int r) const
{
	MapItem it;
	it.x = w16(ArrX, r);
	it.y = w16(ArrY, r);
	it.z = b8(ArrZ, r);
	it.shape = w16(ArrShape, r);
	it.frame = b8(ArrFrame, r);
	it.flags = w16(ArrStatus, r);
	it.quality = w16(ArrQuality, r);
	it.npcNum = b8(ArrNpcNum, r);
	it.mapNum = b8(ArrMapNum, r);
	it.next = w16(ArrNext, r);
	return it;
}

void ItemCacheFile::setSlot(int r, const MapItem &it)
{
	if (!(it.flags & FlagContained))
		s16(ArrX, r, it.x);
	s16(ArrY, r, it.y);
	s8(ArrZ, r, it.z);
	s16(ArrShape, r, it.shape);
	s8(ArrFrame, r, it.frame);
	s16(ArrStatus, r, it.flags);
	s16(ArrQuality, r, it.quality);
	s8(ArrNpcNum, r, it.npcNum);
	s8(ArrMapNum, r, it.mapNum);
}

std::vector<int> ItemCacheFile::chain(int rx, int ry) const
{
	std::vector<int> out;
	int r = head(ListStarts + (rx * Regions + ry) * 2);
	while (r > 0 && r < Slots && out.size() < size_t(Slots))
	{
		out.push_back(r);
		r = w16(ArrNext, r);
	}
	return out;
}

// Item::getContents: a container's quality is its first item, the rest follow by next.
std::vector<int> ItemCacheFile::contents(int r, const Shapes &shapes) const
{
	std::vector<int> out;
	if (shapes.type(w16(ArrShape, r)).family != FamilyContainer)
		return out;
	int c = w16(ArrQuality, r);
	while (c > 0 && c < Slots && out.size() < size_t(Slots))
	{
		out.push_back(c);
		c = w16(ArrNext, c);
	}
	return out;
}

// ItemCache::linkToFree, its odd lastFree test and all.
void ItemCacheFile::linkToFree(int r)
{
	if (r < Npcs)
	{
		s16(ArrNext, r, head(ListFirstFreeNpc));
		if (head(ListFirstFreeNpc) == head(ListLastFreeNpc))
			setHead(ListLastFreeNpc, uint16_t(r));
		setHead(ListFirstFreeNpc, uint16_t(r));
		setHead(ListFreeNpcCount, uint16_t(head(ListFreeNpcCount) + 1));
		return;
	}
	s16(ArrNext, r, head(ListFirstFree));
	if (head(ListFirstFree) == head(ListLastFree))
		setHead(ListLastFree, uint16_t(r));
	setHead(ListFirstFree, uint16_t(r));
	setHead(ListFreeCount, uint16_t(int16_t(head(ListFreeCount)) + 1));
}

// ItemCache::destroy: the contents go first, then the item.
void ItemCacheFile::freeTree(int r, const Shapes &shapes)
{
	for (int c : contents(r, shapes))
		freeTree(c, shapes);
	s16(ArrStatus, r, 0);
	linkToFree(r);
	s8(ArrZ, r, ZFree);
}

void ItemCacheFile::unloadMap(const Shapes &shapes)
{
	if (!valid_)
		return;
	for (int ry = 0; ry < Regions; ry++)
		for (int rx = 0; rx < Regions; rx++)
		{
			for (int r : chain(rx, ry))
			{
				if (r < Npcs)
				{
					// popToLunch: out of the region, into the NPC list, its map kept. Leaving the
					// fast area is redone when the map loads.
					uint16_t status = w16(ArrStatus, r);
					status &= ~(FlagInDisplayList | FlagFastArea | FlagEthereal);
					status |= FlagInNpcList;
					s16(ArrStatus, r, status);
					s16(ArrNext, r, 0);
				}
				else
					freeTree(r, shapes);
			}
			setHead(ListStarts + (rx * Regions + ry) * 2, 0);
			setHead(ListCounts + (rx * Regions + ry) * 2, 0);
		}
}

void ItemCacheFile::putRecord(int r, const char *p)
{
	s16(ArrX, r, read16(p));
	s16(ArrY, r, read16(p + 2));
	s8(ArrZ, r, uint8_t(p[4]));
	s16(ArrShape, r, read16(p + 5));
	s8(ArrFrame, r, uint8_t(p[7]));
	s16(ArrStatus, r, read16(p + 8));
	s16(ArrQuality, r, read16(p + 10));
	s8(ArrNpcNum, r, uint8_t(p[12]));
	s8(ArrMapNum, r, uint8_t(p[13]));
	s16(ArrNext, r, read16(p + 14));
}

// ItemCache::push's last part: onto the ethereal list.
void ItemCacheFile::pushEthereal(int r)
{
	s16(ArrNext, r, head(ListEtherealTop));
	s16(ArrStatus, r, w16(ArrStatus, r) | FlagEthereal);
	setHead(ListEtherealTop, uint16_t(r));
	setHead(ListEtherealCount, uint16_t(head(ListEtherealCount) + 1));
}

int ItemCacheFile::popEthereal()
{
	if (int16_t(head(ListEtherealCount)) <= 0)
		return 0;
	setHead(ListEtherealCount, uint16_t(head(ListEtherealCount) - 1));
	const int r = head(ListEtherealTop);
	setHead(ListEtherealTop, w16(ArrNext, r));
	return r;
}

// One pass of ItemCache::loadMapLevel for a map other than 0.
void ItemCacheFile::loadRecords(int map, const QByteArray &records, const Shapes &shapes)
{
	(void) map;
	const int count = records.size() / SlotBytes;
	int container = 0, depth = 0, r = 0;
	for (int i = 0; i < count; i++)
	{
		const char *p = records.constData() + i * SlotBytes;
		const uint16_t x = read16(p), y = read16(p + 2), shape = read16(p + 5), flags = read16(p + 8);
		const uint8_t z = uint8_t(p[4]), npcNum = uint8_t(p[12]);
		if (shape == 0)
			continue;
		if (int16_t(x) < 0 || int16_t(y) < 0 || z >= ZGone || shape >= 2048 || npcNum == 0xff)
			continue;
		if (int16_t(head(ListFreeCount)) <= 0)
			return;
		QByteArray rec(p, SlotBytes);
		if (shapes.type(shape).family == FamilyContainer)
		{
			rec[10] = 0;
			rec[11] = 0;
		}
		if (flags & FlagContained)
		{
			if (x > depth)
			{
				container = r;
				depth = x;
			}
			while (x < depth && container)
			{
				container = w16(ArrX, container);	// Item::getContainer
				depth--;
			}
			// create: the first free slot, cleared, onto the ethereal list.
			r = head(ListFirstFree);
			s16(ArrStatus, r, 0);
			s16(ArrQuality, r, 0);
			s8(ArrNpcNum, r, 0);
			s8(ArrMapNum, r, 0);
			if (r == head(ListLastFree))
				setHead(ListLastFree, 0);
			setHead(ListFirstFree, w16(ArrNext, r));
			setHead(ListFreeCount, uint16_t(int16_t(head(ListFreeCount)) - 1));
			pushEthereal(r);
			putRecord(r, rec.constData());
			s16(ArrStatus, r, w16(ArrStatus, r) & ~FlagGumpOpen);
			// popToEnd
			popEthereal();
			if (shapes.type(w16(ArrShape, container)).family == FamilyContainer)
			{
				int last = 0;
				for (int c = w16(ArrQuality, container); c; c = w16(ArrNext, c))
					last = c;
				if (last)
					s16(ArrNext, last, uint16_t(r));
				else
					s16(ArrQuality, container, uint16_t(r));
				s16(ArrNext, r, 0);
				s16(ArrX, r, uint16_t(container));
				s8(ArrZ, r, 0);
				s16(ArrStatus, r, (w16(ArrStatus, r) & ~FlagEthereal) | FlagContained);
			}
			s16(ArrY, r, y);
		}
		else
		{
			depth = 0;
			// prependToRegion with one item.
			const int rx = x >> 9, ry = y >> 9;
			const int at = (rx * Regions + ry) * 2;
			const uint16_t oldStart = head(ListStarts + at);
			r = head(ListFirstFree);
			setHead(ListStarts + at, uint16_t(r));
			setHead(ListFreeCount, uint16_t(int16_t(head(ListFreeCount)) - 1));
			setHead(ListCounts + at, uint16_t(head(ListCounts + at) + 1));
			const uint16_t next = w16(ArrNext, r);
			putRecord(r, rec.constData());
			s16(ArrStatus, r, w16(ArrStatus, r) & ~FlagInDisplayList);
			s16(ArrNext, r, oldStart);
			setHead(ListFirstFree, next);
		}
	}
}

void ItemCacheFile::reloadMap(int map, const QByteArray &fixedRecords, const QByteArray &nonFixedRecords,
	const Shapes &shapes)
{
	if (!valid_)
		return;
	unloadMap(shapes);
	for (int i = 0; i < 4; i++)
		setHead(2 + i * 2, 0);	// regionsIn: nothing loaded
	loadRecords(map, fixedRecords, shapes);
	loadRecords(map, nonFixedRecords, shapes);

	// The free NPC slots again, and the map's NPCs back into the regions (push, then pop to
	// where they stand).
	setHead(ListFirstFreeNpc, 0);
	setHead(ListLastFreeNpc, 0);
	setHead(ListFreeNpcCount, 0);
	for (int n = Npcs - 1; n > 0; n--)
	{
		if (z(n) == ZFree && n >= 2)
		{
			s16(ArrNext, n, head(ListFirstFreeNpc));
			setHead(ListFirstFreeNpc, uint16_t(n));
			if (!head(ListFreeNpcCount))
				setHead(ListLastFreeNpc, uint16_t(n));
			setHead(ListFreeNpcCount, uint16_t(head(ListFreeNpcCount) + 1));
		}
		else if (b8(ArrMapNum, n) == map && map)
		{
			uint16_t status = w16(ArrStatus, n);
			if (status & FlagInNpcList)
				s16(ArrStatus, n, status & ~FlagInNpcList);
			pushEthereal(n);
			popEthereal();
			const uint16_t x = w16(ArrX, n), y = w16(ArrY, n);
			const int at = ((x >> 9) * Regions + (y >> 9)) * 2;
			setHead(ListCounts + at, uint16_t(head(ListCounts + at) + 1));
			// Outside the loaded regions, so it leaves the fast area.
			s16(ArrStatus, n, w16(ArrStatus, n) & ~(FlagFastArea | FlagEthereal | FlagContained));
			s16(ArrNext, n, head(ListStarts + at));
			setHead(ListStarts + at, uint16_t(n));
			if (z(n) >= ZGone)
				s8(ArrZ, n, ZGone - 1);
		}
	}
	setHead(ListMapIn, uint16_t(map));
}
