// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: DLIST.C
// The queue routines, DisplayList::draw(void), checkNewItems, sort(uint16_t)
// and DL_ItemNode::compare are in dlasm.cpp.

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "item.h"
#include "itemdata.h"
#include "type.h"
#include "gametime.h"
#include "basecam.h"
#include "camera.h"
#include "cglobvp.h"
#include "error.h"
#include "scratchm.h"
#include "shaphand.h"
#include "statrest.h"
#include "dlist.h"
#include "dlitem.h"

// Inline in the shared headers when this file was built.
U8_INLINE GraphicYtable::GraphicYtable(void) { f_04 = 2; index = 0; }
U8_INLINE Vport::Vport(void) { mem = 0; f_0f = 2; f_10 = 0; }
U8_INLINE Vport::~Vport(void) { free(); }
U8_INLINE Point::Point(void) {}
U8_INLINE void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE Rect::Rect(void) {}
U8_INLINE void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }

class AnimItem : public Item
{
public:
	AnimItem(Referent r) { referent = r; }
};

U8_INLINE Item itemOf(Referent r) { return AnimItem(r); }

#define TRACE_DLIST	((TraceLevel)50)

ScreenArea FullScreenArea(0, 0, 319, 199);
uint8_t DisplayList::animating = 0;
int32_t DisplayList::lasttime = 0;
uint32_t *DisplayList::animatingitem = 0;
uint32_t *DisplayList::newitem = 0;
int16_t ShadowSquareLC = 0;
int16_t ShadowSquareTC = 0;
uint16_t DisplayListNode::freelist = 0;
uint16_t DisplayListEdge::freelist = 0;
char *DisplayListNode::buffer = 0;
const size_t DisplayListNode::slot = (sizeof(DL_ItemNode) + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
char *DisplayListEdge::buffer = 0;
uint16_t DisplayListNode::freecount = 0;
uint16_t DisplayListEdge::freecount = 0;
DisplayListQueue DisplayList::_waiting_queue;
DisplayListQueue DisplayList::_drawing_queue;
int16_t DisplayList::clip_l;
int16_t DisplayList::clip_t;
int16_t DisplayList::clip_r;
int16_t DisplayList::clip_b;
Vport DisplayList::vport;
uint16_t ShadowSquare[SQUARE_ROWS][SQUARE_COLS];

// Names are 16 bits; the edges are as on DOS (6 words, no vtable).
static_assert(sizeof(DisplayListEdge) == 12, "DisplayListEdge is 6 words");
static_assert(sizeof(DisplayListQueue) == 4, "DisplayListQueue is 2 words");
static_assert(NUM_NODES <= 0xFFFF && NUM_EDGES <= 0xFFFF, "names are 16 bits");

void ScreenArea::intersect(const ScreenArea &a)
{
	if (left < a.left)
		left = a.left;
	if (top < a.top)
		top = a.top;
	if (right > a.right)
		right = a.right;
	if (bottom > a.bottom)
		bottom = a.bottom;
}

void ScreenArea::include(const ScreenArea &a)
{
	if (left > a.left)
		left = a.left;
	if (top > a.top)
		top = a.top;
	if (right < a.right)
		right = a.right;
	if (bottom < a.bottom)
		bottom = a.bottom;
}

void *DisplayListNode::operator new(size_t) noexcept
{
	DisplayListNode *node = 0;
	if (freelist)
	{
		node = NODE(freelist);
		freelist = node->next;
		freecount--;
	}
	return node;
}

void DisplayListNode::operator delete(void *p)
{
	DisplayListNode *node = (DisplayListNode *)p;
	node->next = freelist;
	freelist = NODE_OFF(node);
	freecount++;
}

void DisplayListNode::init(void)
{
	if (!buffer)
		buffer = new char[NUM_NODES * slot];
	if (!buffer)
		halt(__FILE__, 152);	// __LINE__
	// Name 0 means no node, so the first slot is never used. (On DOS the
	// block always started at offset 0 and lost its first node the same way.)
	uint16_t n = 0;
	freecount = NUM_NODES;
	freelist = n + 1;
	freecount--;
	for (int16_t i = 0; i < NUM_NODES - 1; i++)
	{
		NODE(n)->next = n + 1;
		n++;
	}
	NODE(n)->next = 0;
}

void DisplayListNode::uninit(void)
{
	if (buffer)
	{
		delete[] buffer;
		buffer = 0;
	}
}

void DisplayListQueue::remove(uint16_t n)
{
	if (head == n)
		head = NODE(n)->next;
	if (tail == n)
		tail = NODE(n)->prev;
	NODE(n)->unlink();
}

// Queues the nodes drawn over n, so they are drawn again after it.
void DisplayList::draw(uint16_t n)
{
	DisplayListNode *node = NODE(n);
	if (node->nodetype() != 1)
	{
		uint16_t e = node->above;
		while (e)
		{
			uint16_t up = EDGE(e)->upper;
			if (!(NODE(up)->flags & (DLN_WAITING | DLN_DRAWING)))
			{
				_waiting_queue.add(up);
				NODE(up)->flags |= DLN_WAITING;
			}
			e = EDGE(e)->nextAbove;
		}
	}
}

void DisplayList::draw(ScreenArea area)
{
	area.left -= Camera::_h;
	area.top -= Camera::_v;
	area.right -= Camera::_h;
	area.bottom -= Camera::_v;
	if (area.intersects(FullScreenArea))
	{
		area.intersect(FullScreenArea);
		clip_l = area.left;
		clip_t = area.top;
		clip_r = area.right;
		clip_b = area.bottom;
		vport = *GlobalVport::global_ptr;
		vport.rect.set(area.left, area.top, area.right, area.bottom);
		draw();
	}
}

// Records that upper is drawn over lower.
uint8_t DisplayList::connect_nodes(uint16_t upper, uint16_t lower)
{
	if (upper != lower)
	{
		uint16_t e = EDGE_OFF(new DisplayListEdge(upper, lower));
		if (e)
		{
			DisplayListNode *up = NODE(upper);
			DisplayListNode *low = NODE(lower);
			up->below = e;
			low->above = e;
			if (up->isTranslucent())
				low->flags |= DLN_UNDER_TRANSLUCENT;
		}
		else
		{
			trace(TRACE_DLIST, "dl edge alloc ");
			return 0;
		}
	}
	return 1;
}

void DisplayList::add_node(uint16_t n)
{
	if (n)
	{
		NODE(n)->flags |= DLN_WAITING;
		_waiting_queue.add(n);
	}
}

void DisplayList::remove_node(uint16_t n)
{
	DisplayListNode *node = NODE(n);
	uint16_t e, dead, low;
	for (e = node->below; e; )
	{
		low = EDGE(e)->lower;
		if (!NODE(low)->waiting())
		{
			_waiting_queue.add(low);
			NODE(low)->flags |= DLN_WAITING;
		}
		EDGE(e)->unlink();
		dead = e;
		e = EDGE(e)->nextBelow;
		delete EDGE(dead);
	}
	node->below = 0;
	for (e = node->above; e; )
	{
		EDGE(e)->unlink();
		dead = e;
		e = EDGE(e)->nextAbove;
		delete EDGE(dead);
	}
	node->above = 0;
	if (node->flags & DLN_WAITING)
	{
		node->flags &= ~DLN_WAITING;
		_waiting_queue.remove(n);
	}
}

// Connects a and b in drawing order, if they overlap.
char DisplayList::sort(uint16_t a, uint16_t b)
{
	char ok = 1;
	DisplayListNode *na = NODE(a);
	DisplayListNode *nb = NODE(b);
	if (b != a)
	{
		int16_t order;
		if (na->nodetype() < nb->nodetype())
			order = 2;
		else if (na->nodetype() > nb->nodetype())
			order = 1;
		else
			order = na->compare(b);
		if (order == 1)
		{
			if (!connect_nodes(a, b))
				ok = 0;
		}
		else if (order == 2)
		{
			if (!connect_nodes(b, a))
				ok = 0;
		}
	}
	return ok;
}

// Sorts n again against everything in the shadow squares under it.
char DisplayList::resort(uint16_t n)
{
	char ok = 1;
	ScreenArea area;
	int16_t x1, y1, x2, y2, x, y;
	uint16_t square, marked = 0;
	DisplayListNode *sq, *mn, *node, *on;
	uint16_t e, other;
	node = NODE(n);
	area = node->area;
	x1 = (area.left - ShadowSquareLC) >> SQUARE_SHIFT;
	y1 = (area.top - ShadowSquareTC) >> SQUARE_SHIFT;
	x2 = (area.right - ShadowSquareLC) >> SQUARE_SHIFT;
	y2 = (area.bottom - ShadowSquareTC) >> SQUARE_SHIFT;
	if (x1 < SQUARE_COLS && y1 < SQUARE_ROWS && x2 >= 0 && y2 >= 0)
	{
		// Nodes already connected to n need no sorting.
		for (e = node->above; e; e = EDGE(e)->nextAbove)
		{
			uint16_t up = EDGE(e)->upper;
			mn = NODE(up);
			mn->flags |= DLN_MARKED;
			mn->markNext = marked;
			marked = up;
		}
		for (e = node->below; e; e = EDGE(e)->nextBelow)
		{
			uint16_t low = EDGE(e)->lower;
			mn = NODE(low);
			mn->flags |= DLN_MARKED;
			mn->markNext = marked;
			marked = low;
		}
		if (x1 < 0)
			x1 = 0;
		if (y1 < 0)
			y1 = 0;
		if (x2 >= SQUARE_COLS)
			x2 = SQUARE_COLS - 1;
		if (y2 >= SQUARE_ROWS)
			y2 = SQUARE_ROWS - 1;
		for (y = y1; y <= y2; y++)
		{
			for (x = x1; x <= x2; x++)
			{
				square = ShadowSquare[y][x];
				sq = NODE(square);
				if (!(sq->flags & DLN_MARKED))
				{
					for (e = sq->above; e; e = EDGE(e)->nextAbove)
					{
						other = EDGE(e)->upper;
						on = NODE(other);
						if (!(on->flags & DLN_MARKED) && node->area.intersects(on->area))
						{
							if (!resort(n, other))
								return 0;
							on->flags |= DLN_MARKED;
							on->markNext = marked;
							marked = other;
						}
					}
					if (!resort(n, square))
						return 0;
				}
			}
		}
		while (marked)
		{
			other = marked;
			marked = NODE(other)->markNext;
			NODE(other)->flags &= ~DLN_MARKED;
		}
	}
	return ok;
}

uint8_t DisplayList::resort(uint16_t a, uint16_t b)
{
	uint8_t ok = 1;
	DisplayListNode *na = NODE(a);
	DisplayListNode *nb = NODE(b);
	if (b != a)
	{
		int16_t order;
		if (na->nodetype() < nb->nodetype())
			order = 2;
		else if (na->nodetype() > nb->nodetype())
			order = 1;
		else
			order = na->compare(b);
		if (order == 1)
		{
			if (!connect_nodes(a, b))
				ok = 0;
		}
		else if (order == 2)
		{
			if (!connect_nodes(b, a))
				ok = 0;
		}
	}
	return ok;
}

void DisplayList::init(void)
{
	int16_t x, y;
	DisplayListNode::init();
	DisplayListEdge::init();
	animating = 0;
	clip_l = 0;
	clip_t = 0;
	clip_r = 319;
	clip_b = 199;
	for (x = 0; x < SQUARE_COLS; x++)
	{
		for (y = 0; y < SQUARE_ROWS; y++)
		{
			uint16_t square = NODE_OFF(new DL_ShadowSquareNode);
			if (square)
			{
				NODE(square)->flags |= DLN_SHADOW;
				ShadowSquare[y][x] = square;
				int16_t left = x * SQUARE_SIZE;
				int16_t top = y * SQUARE_SIZE;
				NODE(square)->area = ScreenArea(left, top, left + SQUARE_SIZE - 1, top + SQUARE_SIZE - 1);
				add_node(square);
			}
			else
			{
				trace(TRACE_DLIST, "dl init ");
				ShadowSquare[y][x] = 0;
			}
		}
	}
	if (!animatingitem)
		animatingitem = (uint32_t *)new char[0x480];
	memset(animatingitem, 0, 0x480);
	if (!newitem)
		newitem = (uint32_t *)new char[0x480];
	memset(newitem, 0, 0x480);
}

void DisplayList::uninit(void)
{
	DisplayListNode::uninit();
	DisplayListEdge::uninit();
	if (animatingitem)
	{
		delete[] (char *)animatingitem;
		animatingitem = 0;
	}
	if (newitem)
	{
		delete[] (char *)newitem;
		newitem = 0;
	}
}

void DisplayList::delete_node(uint16_t n)
{
	remove_node(n);
	delete NODE(n);
}

void DisplayList::delete_edge(uint16_t e)
{
	EDGE(e)->unlink();
	delete EDGE(e);
}

void DisplayList::writeDebugInfo(char *)
{
}

// The topmost node that claims screen point (x, y), or 0.
uint16_t DisplayList::find_target(int16_t x, int16_t y)
{
	ScreenArea area;
	uint16_t target = 0;
	uint16_t square;
	x += Camera::_h;
	y += Camera::_v;
	area = ScreenArea(x, y);
	square = ShadowSquare[(y - ShadowSquareTC) >> SQUARE_SHIFT][(x - ShadowSquareLC) >> SQUARE_SHIFT];
	DisplayListNode *sq = NODE(square);
	DisplayListNode *n;
	DisplayListEdge *edge, *edge2;
	uint16_t e;
	for (e = sq->above; e; e = edge->nextAbove)
	{
		edge = EDGE(e);
		n = NODE(edge->upper);
		if (n->is_target(x, y))
		{
			target = edge->upper;
			n->flags |= DLN_MARKED;
		}
	}
	uint16_t first = e = sq->above;
	if (target)
	{
		// Of the marked nodes, take one with no marked node over it.
		for (e = first; e; e = edge->nextAbove)
		{
			edge = EDGE(e);
			target = edge->upper;
			n = NODE(target);
			if (n->flags & DLN_MARKED)
			{
				for (e = n->above; e; e = edge2->nextAbove)
				{
					edge2 = EDGE(e);
					if (NODE(edge2->upper)->flags & DLN_MARKED)
					{
						target = 0;
						break;
					}
				}
				if (target)
					break;
			}
		}
		for (e = first; e; e = edge->nextAbove)
		{
			edge = EDGE(e);
			NODE(edge->upper)->flags &= ~DLN_MARKED;
		}
	}
	return target;
}

DisplayListNode::DisplayListNode(void)
{
	next = prev = 0;
	above = below = 0;
	flags = 0;
}

// Queues every node that overlaps area to be drawn again.
void DisplayList::mark(ScreenArea &area)
{
	int16_t x1, x2, y2, x, y;
	DisplayListNode *node;
	x1 = (area.left - ShadowSquareLC) >> SQUARE_SHIFT;
	int16_t y1 = (area.top - ShadowSquareTC) >> SQUARE_SHIFT;
	x2 = (area.right - ShadowSquareLC) >> SQUARE_SHIFT;
	y2 = (area.bottom - ShadowSquareTC) >> SQUARE_SHIFT;
	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;
	if (x2 >= SQUARE_COLS)
		x2 = SQUARE_COLS - 1;
	if (y2 >= SQUARE_ROWS)
		y2 = SQUARE_ROWS - 1;
	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			uint16_t e = ShadowSquare[y][x];
			for (e = NODE(e)->above; e; e = EDGE(e)->nextAbove)
			{
				uint16_t up = EDGE(e)->upper;
				node = NODE(up);
				if (!node->waiting() && node->area.intersects(area))
				{
					_waiting_queue.add(up);
					node->flags |= DLN_WAITING;
				}
				uint16_t r = node->getReferent();
				newitem[r >> 5] |= (uint32_t)1 << (r & 31);
			}
		}
	}
}

// Fills list with the animating items on screen that are due a new frame
// at time now, and returns how many. Items gone from view stop animating.
int16_t DisplayList::checkAnimatingItems(int32_t now, uint16_t *list)
{
	Referent base;
	uint32_t mask, word;
	uint32_t *bits;
	int16_t w, b, count;
	Item item;
	TypeFlag type;
	Referent r;
	count = 0;
	base = 0;
	bits = animatingitem;
	for (w = 0; w < 0x120; w++)
	{
		if (*bits)
		{
			word = *bits;
			for (r = base, b = 0, mask = 1; b < 32; r++, b++, mask <<= 1)
			{
				item = itemOf(r);
				if (word & mask)
				{
					if (ItemData::statusArray[item.referent] & 1)
					{
						int16_t x, y, z, sx, sy, left, top, right, bottom, xd, yd, zd;
						type = GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
						x = ItemData::xArray[item.referent] >> 2;
						y = ItemData::yArray[item.referent] >> 2;
						z = ItemData::zArray[item.referent];
						sx = x - y;
						sy = (x >> 1) + (y >> 1) - z;
						if (ItemData::statusArray[item.referent] & 0x20)
							footpad(yd, xd, zd, type);
						else
							footpad(xd, yd, zd, type);
						left = sx - xd - Camera::_h;
						top = sy - (xd >> 1) - (yd >> 1) - zd - Camera::_v;
						right = sx + yd - Camera::_h;
						bottom = sy - Camera::_v;
						if (left < 320 && top < 200 && right >= 0 && bottom >= 0)
						{
							int16_t speed = type.animSpeed;
							if (speed < 2 || lasttime / speed != now / speed)
								list[count++] = r;
						}
						else
							*bits &= ~mask;
					}
					else
						*bits &= ~mask;
				}
			}
		}
		base += 32;
		bits++;
	}
	return count;
}

void DisplayList::animate(void)
{
	if (theAnimation->f_4d)
		return;
	int16_t maxWidth, maxHeight, frames, i, count;
	TypeFlag type;
	Item item;
	uint16_t *list;
	int32_t now;
	int16_t frame;
	DL_ItemNode::delayed_draw();
	list = (uint16_t *)theScratchMem->checkOut(0x4800);
	if (!list)
		outOfMemory(__FILE__, 1154);	// __LINE__
	now = theAnimation->f_3c;
	checkNewItems();
	count = checkAnimatingItems(now, list);
	lasttime = now;
	DL_ItemNode::delayed_draw();
	for (i = 0; i < count; i++)
	{
		item = itemOf(list[i]);
		type = GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
		shapeHandler->getInfo(ItemData::typeArray[item.referent], maxWidth, maxHeight, frames);
		switch (type.animType)
		{
		case 2:
			// Random frames.
			if (urandom(2))
			{
				item.setFrame(urandom(frames));
				BaseCamera::needSlam = 1;
			}
			break;
		case 1:
		case 3:
			frame = item.getFrame();
			if (type.animData == 0 || (type.animData == 1 && urandom(2)))
			{
				if (++frame >= frames)
					frame = 0;
				item.setFrame(frame);
			}
			else
			{
				// Loop within a group of data frames.
				frame = (frame + 1) % type.animData + (frame - frame % type.animData);
				if (frame >= frames)
					frame -= frame % type.animData;
				item.setFrame(frame);
			}
			BaseCamera::needSlam = 1;
			break;
		case 4:
			frame = item.getFrame();
			if (frame == 0 && urandom(type.animData + 2))
				break;
			if (++frame >= frames)
				frame = 0;
			item.setFrame(frame);
			BaseCamera::needSlam = 1;
			break;
		case 5:
			item.anim();
			break;
		case 6:
			frame = item.getFrame();
			if (type.animData == 0 || (type.animData == 1 && urandom(2)))
			{
				if (frame)
				{
					if (++frame >= frames)
						frame = 1;
					item.setFrame(frame);
					BaseCamera::needSlam = 1;
				}
			}
			else if (frame % type.animData)
			{
				frame = (frame + 1) % type.animData + (frame - frame % type.animData);
				if (frame % type.animData == 0)
					frame++;
				if (frame >= frames)
					frame -= frame % type.animData;
				item.setFrame(frame);
				BaseCamera::needSlam = 1;
			}
			break;
		}
	}
	theScratchMem->checkIn((char *)list);
}

// Empties the display list down to its shadow squares.
void DisplayList::genesis(void)
{
	int16_t x, y;
	DisplayListNode *node;
	animating = 0;
	for (y = 0; y < SQUARE_ROWS; y++)
	{
		for (x = 0; x < SQUARE_COLS; x++)
		{
			uint16_t e = ShadowSquare[y][x];
			for (e = NODE(e)->above; e; )
			{
				uint16_t up = EDGE(e)->upper;
				node = NODE(up);
				node->cleanup();
				e = EDGE(e)->nextAbove;
				delete_node(up);
			}
		}
	}
	memset(animatingitem, 0, 0x480);
	memset(newitem, 0, 0x480);
}

int16_t DisplayListNode::compare(uint16_t)
{
	return 0;
}

void DisplayListNode::link(uint16_t p, uint16_t n)
{
	prev = p;
	next = n;
	if (p)
		NODE(p)->next = NODE_OFF(this);
	if (n)
		NODE(n)->prev = NODE_OFF(this);
}

void DisplayListNode::unlink(void)
{
	if (prev)
		NODE(prev)->next = next;
	if (next)
		NODE(next)->prev = prev;
}

void *DisplayListEdge::operator new(size_t) noexcept
{
	DisplayListEdge *edge = 0;
	if (freelist)
	{
		edge = EDGE(freelist);
		freelist = edge->nextAbove;
		freecount--;
	}
	return edge;
}

void DisplayListEdge::operator delete(void *p)
{
	DisplayListEdge *edge = (DisplayListEdge *)p;
	edge->nextAbove = freelist;
	freelist = EDGE_OFF(edge);
	freecount++;
}

void DisplayListEdge::init(void)
{
	if (!buffer)
		buffer = new char[NUM_EDGES * sizeof(DisplayListEdge)];
	if (!buffer)
		halt(__FILE__, 1380);	// __LINE__
	// As for the nodes, the first slot is never used.
	uint16_t e = 0;
	freecount = NUM_EDGES;
	freelist = e + 1;
	freecount--;
	for (int16_t i = 0; i < NUM_EDGES - 1; i++)
	{
		EDGE(e)->nextAbove = e + 1;
		e++;
	}
	EDGE(e)->nextAbove = 0;
}

void DisplayListEdge::uninit(void)
{
	if (buffer)
	{
		delete[] buffer;
		buffer = 0;
	}
}

DisplayListEdge::DisplayListEdge(uint16_t up, uint16_t low)
{
	upper = up;
	lower = low;
	nextAbove = NODE(low)->above;
	if (nextAbove)
		EDGE(nextAbove)->prevAbove = EDGE_OFF(this);
	prevAbove = 0;
	nextBelow = NODE(up)->below;
	if (nextBelow)
		EDGE(nextBelow)->prevBelow = EDGE_OFF(this);
	prevBelow = 0;
}

void DisplayListEdge::unlink(void)
{
	if (prevBelow)
		EDGE(prevBelow)->nextBelow = nextBelow;
	else
		NODE(upper)->below = nextBelow;
	if (nextBelow)
		EDGE(nextBelow)->prevBelow = prevBelow;
	if (prevAbove)
		EDGE(prevAbove)->nextAbove = nextAbove;
	else
		NODE(lower)->above = nextAbove;
	if (nextAbove)
		EDGE(nextAbove)->prevAbove = prevAbove;
}

void initDisplayList(void)
{
	DisplayList::init();
}

void uninitDisplayList(void)
{
	DisplayList::uninit();
}
