#ifndef DLIST_H
#define DLIST_H

#include "u8port.h"

// #include <dos.h>  (DOS only)
#include "cvport.h"

// The display list orders what is on screen so that each thing is drawn
// after everything it covers. Nodes and edges live in blocks of their own
// (DisplayListNode::buffer, DisplayListEdge::buffer) and are passed around as
// 16-bit names, 0 meaning none:
//
//   upper node --below--> edge --lower--> lower node
//   lower node --above--> edge --upper--> upper node
//
// A fixed grid of shadow square nodes covers the screen; every node is
// drawn over the squares under it.

class DisplayListNode;
class DisplayListEdge;

// On DOS a name was the node's offset in its segment. A node now takes more
// room (the vtable pointer is 8 bytes), and 1500 of them no longer fit in
// 64K, so a name is the number of the node's slot in its block, and likewise
// for edges: NODE(n) is buffer + n * slot size. Slot 0 is never handed out,
// as offset 0 was not. NODE_OFF/EDGE_OFF give a pointer's name back (0 for a
// null pointer), as FP_OFF did.
#define NODE(n)	((DisplayListNode *)(DisplayListNode::buffer + (size_t)(uint16_t)(n) * DisplayListNode::slot))
#define EDGE(e)	((DisplayListEdge *)(DisplayListEdge::buffer + (size_t)(uint16_t)(e) * sizeof(DisplayListEdge)))
#define NODE_OFF(p)	dlNodeName(p)
#define EDGE_OFF(p)	dlEdgeName(p)

#define NUM_NODES	1500
#define NUM_EDGES	4000

// DisplayListNode::flags
#define DLN_WAITING		0x01	// in the waiting queue
#define DLN_MARKED		0x08	// already visited by this pass
#define DLN_DRAWING		0x20	// in the drawing queue
#define DLN_SHADOW		0x40	// a shadow square
#define DLN_UNDER_TRANSLUCENT	0x80	// something translucent is drawn over it

// The screen is covered by SQUARE_COLS x SQUARE_ROWS shadow squares.
#define SQUARE_SHIFT	6
#define SQUARE_SIZE		(1 << SQUARE_SHIFT)
#define SQUARE_COLS		6
#define SQUARE_ROWS		5

class ScreenArea
{
public:
	int16_t left;
	int16_t top;
	int16_t right;
	int16_t bottom;

	ScreenArea(void) { left = 0; top = 0; right = 0; bottom = 0; }
	ScreenArea(int16_t x, int16_t y) { left = x; top = y; right = x; bottom = y; }
	ScreenArea(int16_t l, int16_t t, int16_t r, int16_t b) { left = l; top = t; right = r; bottom = b; }
	ScreenArea(Rect &r) { left = r.f_00; top = r.f_02; right = r.f_04; bottom = r.f_06; }
	uint8_t intersects(const ScreenArea &a) const
	{
		return left <= a.right && top <= a.bottom && right >= a.left && bottom >= a.top;
	}
	uint8_t contains(const ScreenArea &a) const
	{
		return left <= a.left && top <= a.top && right >= a.right && bottom >= a.bottom;
	}
	void intersect(const ScreenArea &);
	void include(const ScreenArea &);
};

class DisplayListNode
{
public:
	uint16_t markNext;	// chains the nodes marked during a pass
	uint16_t next;		// queue links
	uint16_t prev;
	uint16_t above;		// edges to the nodes drawn over this one
	uint16_t below;		// edges to the nodes this one is drawn over
	ScreenArea area;			// screen rectangle, camera included
	char flags;

	DisplayListNode(void);
	virtual int16_t nodetype(void) { return 0; }
	virtual void draw(void) {}
	virtual int16_t compare(uint16_t);
	virtual uint8_t is_target(int16_t, int16_t) { return 0; }
	virtual int16_t getReferent(void) { return 0; }
	virtual void cleanup(void) {}
	virtual uint8_t isTranslucent(void) { return 0; }
	uint8_t waiting(void) { return flags & DLN_WAITING; }
	// Hands out a slot of buffer, or 0 when none is free (then no constructor runs).
	static void *operator new(size_t) noexcept;
	static void operator delete(void *);
	static void init(void);
	static void uninit(void);
	void link(uint16_t, uint16_t);
	void unlink(void);
	static uint16_t freelist;
	static char *buffer;		// NUM_NODES slots
	static const size_t slot;	// bytes per slot: sizeof(DL_ItemNode), the largest node, rounded up to pointer alignment
	static uint16_t freecount;
};

class DisplayListQueue
{
public:
	uint16_t head;
	uint16_t tail;

	DisplayListQueue(void) { head = tail = 0; }
	void remove(uint16_t);
	uint16_t remove_head(void);
	void add_tail(uint16_t);
	void add(uint16_t n) { add_tail(n); }
};

class DisplayList
{
public:
	static void draw(uint16_t);
	static void draw(ScreenArea);
	static uint8_t connect_nodes(uint16_t, uint16_t);
	static void add_node(uint16_t);
	static void remove_node(uint16_t);
	static char sort(uint16_t, uint16_t);
	static char resort(uint16_t);
	static uint8_t resort(uint16_t, uint16_t);
	static void init(void);
	static void uninit(void);
	static void delete_node(uint16_t);
	static void delete_edge(uint16_t);
	static void writeDebugInfo(char *);
	static uint16_t find_target(int16_t, int16_t);
	static void mark(ScreenArea &);
	static int16_t checkAnimatingItems(int32_t, uint16_t *);
	static void animate(void);
	static void genesis(void);
	static void draw(void);
	static void checkNewItems(void);
	static uint8_t sort(uint16_t);
	static void stopAnimating(uint16_t r) { animatingitem[r >> 5] &= ~((uint32_t)1 << (r & 31)); }
	static void clearNew(uint16_t r) { newitem[r >> 5] &= ~((uint32_t)1 << (r & 31)); }
	static uint8_t animating;
	static int32_t lasttime;
	static uint32_t *animatingitem;	// one bit per item
	static uint32_t *newitem;
	static DisplayListQueue _waiting_queue;
	static DisplayListQueue _drawing_queue;
	static int16_t clip_l;
	static int16_t clip_t;
	static int16_t clip_r;
	static int16_t clip_b;
	static Vport vport;
};

class DisplayListEdge
{
public:
	uint16_t upper;
	uint16_t lower;
	uint16_t nextAbove;	// links in the lower node's above list
	uint16_t prevAbove;
	uint16_t nextBelow;	// links in the upper node's below list
	uint16_t prevBelow;

	DisplayListEdge(uint16_t, uint16_t);
	// Hands out a slot of buffer, or 0 when none is free (then no constructor runs).
	static void *operator new(size_t) noexcept;
	static void operator delete(void *);
	static void init(void);
	static void uninit(void);
	void unlink(void);
	static uint16_t freelist;
	static char *buffer;		// NUM_EDGES slots of sizeof(DisplayListEdge)
	static uint16_t freecount;
};

class DL_ShadowSquareNode : public DisplayListNode
{
public:
	virtual int16_t nodetype(void) { return 1; }
	virtual void draw(void);
	virtual int16_t compare(uint16_t) { return 0; }
};

inline uint16_t dlNodeName(const void *p)
{
	return p ? (uint16_t)(((const char *)p - DisplayListNode::buffer) / DisplayListNode::slot) : 0;
}

inline uint16_t dlEdgeName(const void *p)
{
	return p ? (uint16_t)(((const char *)p - DisplayListEdge::buffer) / sizeof(DisplayListEdge)) : 0;
}

void initDisplayList(void);
void uninitDisplayList(void);

extern ScreenArea FullScreenArea;
extern int16_t ShadowSquareLC;
extern int16_t ShadowSquareTC;
extern uint16_t ShadowSquare[SQUARE_ROWS][SQUARE_COLS];

#endif
