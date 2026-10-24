#ifndef DLIST_H
#define DLIST_H

#include "u8port.h"

// #include <dos.h>  (DOS only)
#include "cvport.h"

// The display list orders what is on screen so that each thing is drawn
// after everything it covers. Nodes and edges live in segments of their own
// and are passed around as offsets:
//
//   upper node --below--> edge --lower--> lower node
//   lower node --above--> edge --upper--> upper node
//
// A fixed grid of shadow square nodes covers the screen; every node is
// drawn over the squares under it.

class DisplayListNode;
class DisplayListEdge;

#define NODE(n)	((DisplayListNode *)MK_FP(DisplayListNodeSeg, n))
#define EDGE(e)	((DisplayListEdge *)MK_FP(DisplayListEdgeSeg, e))

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
	uint8_t intersects(ScreenArea &a)
	{
		return left <= a.right && top <= a.bottom && right >= a.left && bottom >= a.top;
	}
	uint8_t contains(ScreenArea &a)
	{
		return left <= a.left && top <= a.top && right >= a.right && bottom >= a.bottom;
	}
	void intersect(ScreenArea &);
	void include(ScreenArea &);
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
	static void *operator new(uint16_t);
	static void operator delete(void *);
	static void init(void);
	static void uninit(void);
	void link(uint16_t, uint16_t);
	void unlink(void);
	static uint16_t freelist;
	static char *buffer;
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
	static void stopAnimating(uint16_t r) { animatingitem[r >> 5] &= ~(1L << (r & 31)); }
	static void clearNew(uint16_t r) { newitem[r >> 5] &= ~(1L << (r & 31)); }
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
	static void *operator new(uint16_t);
	static void operator delete(void *);
	static void init(void);
	static void uninit(void);
	void unlink(void);
	static uint16_t freelist;
	static char *buffer;
	static uint16_t freecount;
};

class DL_ShadowSquareNode : public DisplayListNode
{
public:
	virtual int16_t nodetype(void) { return 1; }
	virtual void draw(void);
	virtual int16_t compare(uint16_t) { return 0; }
};

void initDisplayList(void);
void uninitDisplayList(void);

extern ScreenArea FullScreenArea;
extern uint16_t DisplayListNodeSeg;
extern uint16_t DisplayListEdgeSeg;
extern int16_t ShadowSquareLC;
extern int16_t ShadowSquareTC;
extern uint16_t ShadowSquare[SQUARE_ROWS][SQUARE_COLS];

#endif
