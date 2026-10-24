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

// Usecode's search opcodes (inter.cpp 0x70, 0x73) build a finder, copy it byte for byte onto
// the usecode stack, and call findNext() on the copy there; the stack is saved with the
// process in a saved game. So the finders keep their DOS layout and sizes, hold no pointers,
// and work after a plain memcpy.
//
// In DOS findNext and inArea were virtual, and the word at offset 2 was Borland's near vtable
// pointer (AREAFIND.ASM reads it at IF_VPTR = 2). A host vtable pointer would be 8 bytes and
// would not survive a save, so the word is now ItemFinder::vtable, holding the same values
// (the DGROUP offsets of the vtables in U8.MAP), and the virtual calls are made by hand:
//   findNext()    ItemFinder::findNext dispatches on vtable (it is what callers holding an
//                 ItemFinder * call); the overriding classes' findNext are the bodies.
//   inArea()      AreaItemFinder::callInArea dispatches; each class's inArea is its body, so
//                 SurfaceItemFinder's call of AreaItemFinder::inArea() still means that body.
// Every constructor sets vtable first, as Borland set the vptr. Assigning one finder to a
// finder of a base class would copy the derived class's vtable value; nothing does.
#define VT_ITEM_FINDER						0x089a
#define VT_REGION_ITEM_FINDER				0x089e
#define VT_AREA_ITEM_FINDER					0x08a2
#define VT_SURFACE_ITEM_FINDER				0x0b3c
#define VT_CONTAINER_ITEM_FINDER			0x1722
#define VT_RECURSIVE_CONTAINER_ITEM_FINDER	0x1726
#define VT_RECURSIVE_AREA_ITEM_FINDER		0x88a6

// Sizes in DOS (and now): ItemFinder, RegionItemFinder, ContainerItemFinder 38;
// RecursiveContainerItemFinder 40; AreaItemFinder, RecursiveAreaItemFinder 50;
// SurfaceItemFinder 59. Checked in itemfind.cpp.

class ItemFinder : public Item
{
public:
	uint16_t vtable;			// 2: which class this is (the DOS near vtable pointer)
	Referent first;				// 4: head of the list being searched
	uint8_t criteria[32];		// 6: a SearchCriteria program

	ItemFinder(void) { vtable = VT_ITEM_FINDER; }
	ItemFinder(uint16_t, char *, uint16_t);
	Boolean found(void) { return referent != 0; }
	uint16_t getType(void) { return ItemData::typeArray[referent]; }
	void findNext(void);		// virtual in DOS
	void restartSearch(void);
	uint8_t meetsSearchCriteria(void);
};

class RegionItemFinder : public ItemFinder
{
public:
	RegionItemFinder(void) { vtable = VT_REGION_ITEM_FINDER; }
	RegionItemFinder(uint16_t, uint16_t, char *, uint16_t);
	void gotoRegion(uint16_t, uint16_t);
};

class AreaItemFinder : public RegionItemFinder
{
public:
	uint16_t left;		// 0x26 (AF_LEFT in AREAFIND.ASM)
	uint16_t top;
	uint16_t right;
	uint16_t bottom;
	uint16_t curX;		// corner of the region being searched
	uint16_t curY;

	AreaItemFinder(void) { vtable = VT_AREA_ITEM_FINDER; }
	AreaItemFinder(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) { vtable = VT_AREA_ITEM_FINDER; init(x1, y1, x2, y2, "$", 1); }
	AreaItemFinder(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, char *s, uint16_t q) { vtable = VT_AREA_ITEM_FINDER; init(x1, y1, x2, y2, s, q); }
	// The square of the given radius around x, y.
	AreaItemFinder(int16_t x, int16_t y, uint16_t range, char *s, uint16_t q) { vtable = VT_AREA_ITEM_FINDER; init(x - range, y - range, x + range, y + range, s, q); }
	void findNext(void);		// areafind.cpp; overrides ItemFinder::findNext
	Boolean inArea(void);		// areafind.cpp; virtual in DOS
	Boolean callInArea(void);	// the virtual call of inArea()
	void init(uint16_t, uint16_t, uint16_t, uint16_t, char *, uint16_t);
	void init(int16_t, int16_t, char *, uint16_t);
	void gotoNextList(void);
};

class RecursiveAreaItemFinder : public AreaItemFinder
{
public:
	RecursiveAreaItemFinder(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, char *s, uint16_t q) { vtable = VT_RECURSIVE_AREA_ITEM_FINDER; init(x1, y1, x2, y2, s, q); }
	RecursiveAreaItemFinder(int16_t x, int16_t y, int16_t range, char *s, uint16_t q) { vtable = VT_RECURSIVE_AREA_ITEM_FINDER; init(x - range, y - range, x + range, y + range, s, q); }
	void findNext(void);		// overrides
	Boolean inArea(void);		// overrides
};

class ContainerItemFinder : public ItemFinder
{
public:
	ContainerItemFinder(void) { vtable = VT_CONTAINER_ITEM_FINDER; }
	ContainerItemFinder(Referent container, char *s, uint16_t q) { vtable = VT_CONTAINER_ITEM_FINDER; init(container, s, q); }
	void init(uint16_t, char *, uint16_t);
};

class RecursiveContainerItemFinder : public ContainerItemFinder
{
public:
	Referent container;		// 0x26

	RecursiveContainerItemFinder(Referent container) { vtable = VT_RECURSIVE_CONTAINER_ITEM_FINDER; init(container, "$", 1); }
	RecursiveContainerItemFinder(Referent container, char *s, uint16_t q) { vtable = VT_RECURSIVE_CONTAINER_ITEM_FINDER; init(container, s, q); }
	void findNext(void);		// overrides
	void init(uint16_t, char *, uint16_t);
};

class SurfaceItemFinder : public AreaItemFinder
{
public:
	uint16_t x;			// 0x32
	uint16_t y;
	uint8_t z;
	Referent surface;
	uint8_t what;		// SURFACE_ON, SURFACE_UNDER
	uint8_t where;	// which test the found item passed

	SurfaceItemFinder(Referent r, char *s, uint16_t q, uint8_t what) { vtable = VT_SURFACE_ITEM_FINDER; init(r, s, q, what); }
	Boolean inArea(void);		// overrides
	void init(uint16_t, char *, uint16_t, uint8_t);
};

#endif
