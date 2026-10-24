#ifndef ITEMDATA_H
#define ITEMDATA_H

#include "u8port.h"

#include "mapfile.h"

class OneItem;

#define MAX_ITEMS 9216

class ItemData
{
public:
	~ItemData(void);
	static void init(void);
	static void getItem(uint16_t, OneItem &);
	static void putItem(uint16_t, OneItem &);
	static uint16_t *xArray;
	static uint16_t *yArray;
	static uint8_t *zArray;
	static uint16_t *typeArray;
	static uint8_t *frameArray;
	static uint16_t *statusArray;
	static uint16_t *qArray;
	static uint8_t *npcNumArray;
	static uint8_t *mapNumArray;
	static uint16_t *nextArray;
};

extern ItemData itemData;

#endif
