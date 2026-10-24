#ifndef EDITEM_H
#define EDITEM_H

#include "u8port.h"

#include "item.h"
#include "crect.h"

class WorldPoint;

// The item the map editor places or drags with the mouse.
class EditorItem : public Item
{
public:
	Item dragged;
	Type type;
	Frame frame;
	uint8_t none;		// no type chosen: the mouse drags existing items
	uint8_t align;	// snap to the 32-unit grid
	uint8_t raise;	// lift by the item's z
	uint32_t f_0b;
	uint16_t locX;
	uint16_t locY;
	uint8_t locZ;
	Point offset;
	int16_t dropX;
	int16_t dropY;
	Referent f_1c;
	uint8_t isDefault;

	EditorItem(void);
	void set(Type, Frame);
	void update(void);
	void dragFromContainer(void);
	char move(int16_t, int16_t);
	char move(uint16_t, int16_t, int16_t);
	void click(void);
	void setDefault(void);
	void startDrag(uint16_t);
	void stopDrag(void);
	void calculateLocation(int16_t, int16_t, WorldPoint &);
};

#endif
