#ifndef DLITEM_H
#define DLITEM_H

#include "u8port.h"

#include "dlist.h"
#include "type.h"

// An item's node in the display list. Positions and sizes are world units / 4.
class DL_ItemNode : public DisplayListNode
{
public:
	uint16_t referent;	// the item
	int16_t screenX;	// x - y
	int16_t screenY;	// (x + y) / 2 - z
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t xd;		// footprint, x and y swapped for a flipped item
	int16_t yd;
	int16_t zd;

	virtual int16_t nodetype(void) { return 2; }
	virtual void draw(void);
	virtual int16_t compare(uint16_t);
	virtual uint8_t is_target(int16_t, int16_t);
	virtual int16_t getReferent(void);
	virtual void cleanup(void) { flag_out_of_list(); }
	virtual uint8_t isTranslucent(void);
	void set(uint16_t r) { referent = r; update(); }
	void update(void);
	void flag_out_of_list(void);
	static uint16_t find(uint16_t, int16_t, int16_t, int16_t, int16_t);
	static uint16_t find(uint16_t);
	static void before_push(uint16_t);
	static void after_pop(uint16_t);
	static void delayed_pop(void);
	static void delayed_draw(void);
	static void include_area(int16_t, int16_t, int16_t, int16_t);
	static void delayed_gump(void);
	static uint8_t showFootpads;
	static uint8_t footpadTargetMode;
	static uint8_t ignoreMode;
	static uint16_t _popref;	// item waiting to be put back in the list
	static uint8_t gumpflag;
	static uint8_t drawflag;
	static ScreenArea gumprect;		// screen to refresh through the dispatcher
	static ScreenArea drawrect;		// screen to redraw from the display list
};

char isGameRunning(void);

inline void footpad(int16_t &x, int16_t &y, int16_t &z, TypeFlag &type)
{
	type.getFootpad(x, y, z);
}


#endif
