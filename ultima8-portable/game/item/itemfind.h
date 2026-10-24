#ifndef ITEMFIND_H
#define ITEMFIND_H

#include "u8port.h"

#include "item.h"
#include "itemdata.h"

// SurfaceItemFinder::what
#define SURFACE_ON		1	// items standing on the surface
#define SURFACE_UNDER	2	// items the surface stands on

class AreaItemFinder;
class ContainerItemFinder;
class ItemFinder;
class RegionItemFinder;

class ItemFinder : public Item
{
public:
	Referent first;				// head of the list being searched
	uint8_t criteria[32];	// a SearchCriteria program

	ItemFinder(void) {}
	ItemFinder(uint16_t, char *, uint16_t);
	Boolean found(void) { return referent != 0; }
	uint16_t getType(void) { return ItemData::typeArray[referent]; }
	virtual void findNext(void);
	void restartSearch(void);
	uint8_t meetsSearchCriteria(void);
};

class RegionItemFinder : public ItemFinder
{
public:
	RegionItemFinder(void) {}
	RegionItemFinder(uint16_t, uint16_t, char *, uint16_t);
	void gotoRegion(uint16_t, uint16_t);
};

class AreaItemFinder : public RegionItemFinder
{
public:
	uint16_t left;
	uint16_t top;
	uint16_t right;
	uint16_t bottom;
	uint16_t curX;		// corner of the region being searched
	uint16_t curY;

	AreaItemFinder(void) {}
	AreaItemFinder(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) { init(x1, y1, x2, y2, "$", 1); }
	AreaItemFinder(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, char *s, uint16_t q) { init(x1, y1, x2, y2, s, q); }
	// The square of the given radius around x, y.
	AreaItemFinder(int16_t x, int16_t y, uint16_t range, char *s, uint16_t q) { init(x - range, y - range, x + range, y + range, s, q); }
	virtual void findNext(void);
	virtual Boolean inArea(void);
	void init(uint16_t, uint16_t, uint16_t, uint16_t, char *, uint16_t);
	void init(int16_t, int16_t, char *, uint16_t);
	void gotoNextList(void);
};

class RecursiveAreaItemFinder : public AreaItemFinder
{
public:
	RecursiveAreaItemFinder(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, char *s, uint16_t q) { init(x1, y1, x2, y2, s, q); }
	RecursiveAreaItemFinder(int16_t x, int16_t y, int16_t range, char *s, uint16_t q) { init(x - range, y - range, x + range, y + range, s, q); }
	virtual void findNext(void);
	virtual Boolean inArea(void);
};

class ContainerItemFinder : public ItemFinder
{
public:
	ContainerItemFinder(void) {}
	ContainerItemFinder(Referent container, char *s, uint16_t q) { init(container, s, q); }
	void init(uint16_t, char *, uint16_t);
};

class RecursiveContainerItemFinder : public ContainerItemFinder
{
public:
	Referent container;

	RecursiveContainerItemFinder(Referent container) { init(container, "$", 1); }
	RecursiveContainerItemFinder(Referent container, char *s, uint16_t q) { init(container, s, q); }
	virtual void findNext(void);
	void init(uint16_t, char *, uint16_t);
};

class SurfaceItemFinder : public AreaItemFinder
{
public:
	uint16_t x;
	uint16_t y;
	uint8_t z;
	Referent surface;
	uint8_t what;		// SURFACE_ON, SURFACE_UNDER
	uint8_t where;	// which test the found item passed

	SurfaceItemFinder(Referent r, char *s, uint16_t q, uint8_t what) { init(r, s, q, what); }
	virtual Boolean inArea(void);
	void init(uint16_t, char *, uint16_t, uint8_t);
};

#endif
