// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\ITEM\ITEMDATA.C

#include "u8port.h"

#include <stddef.h>
#include "cfile.h"
#include "cexit.h"
#include "filespec.h"
#include "scratchm.h"
#include "glob.h"
#include "itemcach.h"
#include "itemdata.h"

// Inline in the shared headers when this file was built.
inline Index::Index(void) { size = 0; offset = 0; }

ItemData itemData;

uint16_t *ItemData::xArray;
uint16_t *ItemData::yArray;
uint8_t *ItemData::zArray;
uint16_t *ItemData::typeArray;
uint8_t *ItemData::frameArray;
uint16_t *ItemData::statusArray;
uint16_t *ItemData::qArray;
uint8_t *ItemData::npcNumArray;
uint8_t *ItemData::mapNumArray;
uint16_t *ItemData::nextArray;

// Borland's new returned 0 when out of memory; nothrow keeps the check below working.
void ItemData::init(void)
{
	xArray = new (std::nothrow) uint16_t[MAX_ITEMS];
	yArray = new (std::nothrow) uint16_t[MAX_ITEMS];
	zArray = new (std::nothrow) uint8_t[MAX_ITEMS];
	typeArray = new (std::nothrow) uint16_t[MAX_ITEMS];
	frameArray = new (std::nothrow) uint8_t[MAX_ITEMS];
	statusArray = new (std::nothrow) uint16_t[MAX_ITEMS];
	qArray = new (std::nothrow) uint16_t[MAX_ITEMS];
	npcNumArray = new (std::nothrow) uint8_t[MAX_ITEMS];
	mapNumArray = new (std::nothrow) uint8_t[MAX_ITEMS];
	nextArray = new (std::nothrow) uint16_t[MAX_ITEMS];
	if (!xArray || !yArray || !zArray || !typeArray || !frameArray ||
		!qArray || !npcNumArray || !mapNumArray || !nextArray)
		outOfMemory(__FILE__, 80);	// __LINE__ in the original file
}

ItemData::~ItemData(void)
{
	if (xArray)
	{
		delete[] xArray;
		xArray = 0;
	}
	if (yArray)
	{
		delete[] yArray;
		yArray = 0;
	}
	if (zArray)
	{
		delete[] zArray;
		zArray = 0;
	}
	if (typeArray)
	{
		delete[] typeArray;
		typeArray = 0;
	}
	if (frameArray)
	{
		delete[] frameArray;
		frameArray = 0;
	}
	if (statusArray)
	{
		delete[] statusArray;
		statusArray = 0;
	}
	if (qArray)
	{
		delete[] qArray;
		qArray = 0;
	}
	if (npcNumArray)
	{
		delete[] npcNumArray;
		npcNumArray = 0;
	}
	if (mapNumArray)
	{
		delete[] mapNumArray;
		mapNumArray = 0;
	}
	if (nextArray)
	{
		delete[] nextArray;
		nextArray = 0;
	}
}

void ItemData::getItem(uint16_t referent, OneItem &item)
{
	item.x = xArray[referent];
	item.y = yArray[referent];
	item.z = zArray[referent];
	item.type = typeArray[referent];
	item.frame = frameArray[referent];
	item.flags = statusArray[referent];
	item.quality = qArray[referent];
	item.npcNum = npcNumArray[referent];
	item.mapNum = mapNumArray[referent];
	item.next_object = nextArray[referent];
}

void ItemData::putItem(uint16_t referent, OneItem &item)
{
	xArray[referent] = item.x;
	yArray[referent] = item.y;
	zArray[referent] = item.z;
	typeArray[referent] = item.type;
	frameArray[referent] = item.frame;
	statusArray[referent] = item.flags;
	qArray[referent] = item.quality;
	npcNumArray[referent] = item.npcNum;
	mapNumArray[referent] = item.mapNum;
	nextArray[referent] = item.next_object;
}

