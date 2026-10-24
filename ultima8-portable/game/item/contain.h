#ifndef CONTAIN_H
#define CONTAIN_H

#include "u8port.h"

#include "item.h"

class Container : public Item
{
public:
	Container(Referent r) : Item(r) {}

	Boolean holds(uint16_t);
	Boolean can_hold(Referent);
	Boolean can_hold(uint16_t, int16_t);
	Boolean tooHeavy(int16_t);
	int16_t findItem(int16_t, int16_t);
	void removeContents(void);
	void destroyContents(void);
	int16_t getContentsWeight(void);
	int16_t getTotalContentsVolume(void);
	int16_t getContentsVolume(void);
	int16_t countItems(void);
};

extern uint16_t dragWeightHack;

#endif
