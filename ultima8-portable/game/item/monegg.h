#ifndef MONEGG_H
#define MONEGG_H

#include "u8port.h"

#include "item.h"

// An egg that hatches monsters. Its fields live in the item:
// quality = probability index << 11 | monster shape, mapArray = monster id << 3 | activity,
// npcArray low 3 bits = number of monsters.
class MonsterEgg : public Item
{
public:
	void hatch(void);
	int16_t getMonId(void);
	void setMonId(int16_t);
	int16_t getActivity(void);
	void setActivity(int16_t);
	int16_t getShapeType(void);
	void setShapeType(int16_t);
	int16_t getNumMonsters(void);
	void setNumMonsters(int16_t);
	int16_t getProb(void);
	void setProb(int16_t);
};

extern uint8_t chances[8];

#endif
