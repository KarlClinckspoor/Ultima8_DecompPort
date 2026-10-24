// name: DLASM
// Ported from DLASM.ASM.
// Display list queues, drawing, new-item checks and depth sorting.
//
// DLIST.INC described the C++ classes field for field for the assembly. The
// classes now have an 8-byte vtable pointer and their nodes and edges are
// named by slot (see dlist.h), so this file uses the members by name:
//   DLQUEUE dlqHead/dlqTail            DisplayListQueue::head/tail
//   DLNODE  dlnMarkNext .. dlnFlags    DisplayListNode::markNext .. flags
//           dlnLeft .. dlnBottom       DisplayListNode::area.left .. bottom
//   DLEDGE  dleUpper .. dlePrevBelow   DisplayListEdge::upper .. prevBelow
//   DLITEMNODE dliReferent .. dliZSize DL_ItemNode::referent .. zd
//   TYPEFLAG                           TypeFlag (typeflag.dat records, 8 bytes)
// The vtable call [dlnVptr + VT_DRAW] is the virtual DisplayListNode::draw().
//
// Nodes:
//   upper node --below--> edge --lower--> lower node
//   lower node --above--> edge --upper--> upper node
// A lower node is drawn before every upper node that covers it.

#include "u8port.h"

#include "item.h"
#include "itemdata.h"
#include "type.h"
#include "basecam.h"
#include "camera.h"
#include "error.h"
#include "dlist.h"
#include "dlitem.h"

// The typeflag bytes are read through TypeFlag's bit fields, which lay them
// out as stored: fixed..noisy, draw..family, equipType/xSize, ySize/zSize,
// animType/animData, animSpeed/editor..unknown2, weight, volume.
static_assert(sizeof(TypeFlag) == 8, "TypeFlag is one 8-byte typeflag.dat record");

#define SCREEN_W	320
#define SCREEN_H	200

#define NEWITEM_BYTES	0x480	// one bit per item slot

// DL_ItemNode::compare results
#define CMP_APART	0	// no overlap on screen
#define CMP_OVER	1	// this is drawn over the other node
#define CMP_UNDER	2

static char dlLoopMsg[] = "dl loop ";

// A node's footpad in display list units: x and y sizes swap for a flipped
// item; a flat loose item still counts as 1 high. (The assembly pulled the
// nibbles out of the typeflag bytes; this is the same as TypeFlag::getFootpad.)
static void itemFootpad(uint16_t item, int16_t &xSize, int16_t &ySize, int16_t &zSize)
{
	TypeFlag &type = GlobalTypes.typeFlags[ItemData::typeArray[item]];
	if (ItemData::statusArray[item] & FLIPPED)
	{
		ySize = type.xSize << 3;
		xSize = type.ySize << 3;
	}
	else
	{
		xSize = type.xSize << 3;
		ySize = type.ySize << 3;
	}
	zSize = type.zSize << 3;
	if (zSize == 0 && !type.fixed)
		zSize = 1;
}

// unsigned DisplayListQueue::remove_head(void)
// Unlinks the first node and returns it, or 0 when the queue is empty.
uint16_t DisplayListQueue::remove_head(void)
{
	uint16_t n = head;
	if (n)
	{
		head = NODE(n)->next;
		if (head == 0)
			tail = 0;
		else
			NODE(head)->prev = 0;
	}
	return n;
}

// void DisplayListQueue::add_tail(unsigned short node)
void DisplayListQueue::add_tail(uint16_t n)
{
	if (n)
	{
		DisplayListNode *node = NODE(n);
		node->prev = tail;
		node->next = 0;
		if (tail)
			NODE(tail)->next = n;
		if (head == 0)
			head = n;
		tail = n;
	}
}

// static void DisplayList::draw(void)
// Moves each waiting node to the drawing queue once nothing below it is still
// waiting, then draws the drawing queue in order. Nodes off screen are dropped.
// While animating, the drawing queue goes back to wait and the pass repeats.
void DisplayList::draw(void)
{
	int16_t left, top, right, bottom;
	uint8_t stuck;
	uint16_t n, firstPutBack;
	DisplayListNode *node;
	BaseCamera::needSlam = 1;
	stuck = 0;
	left = clip_l + Camera::_h;
	top = clip_t + Camera::_v;
	right = clip_r + Camera::_h;
	bottom = clip_b + Camera::_v;

	// firstPutBack is the first node put back since one was last drawn;
	// meeting it again means the nodes below each other form a loop.
	firstPutBack = 0;
	while (_waiting_queue.head)
	{
		n = _waiting_queue.remove_head();
		node = NODE(n);
		if (firstPutBack == n)
			stuck = 1;
		else
		{
			// Wait while any node below (other than a shadow square) is still
			// waiting, unless this one is already in the drawing queue.
			char own = node->flags;
			uint8_t putBack = 0;
			for (uint16_t e = node->below; e; e = EDGE(e)->nextBelow)
			{
				char low = NODE(EDGE(e)->lower)->flags;
				if ((low & DLN_WAITING) && !(low & DLN_SHADOW))
				{
					if (!(own & DLN_DRAWING))
					{
						node->flags |= DLN_WAITING;
						putBack = 1;
					}
					break;
				}
			}
			if (putBack)
			{
				// Back to the end of the waiting queue.
				if (!firstPutBack)
					firstPutBack = n;
				_waiting_queue.add_tail(n);
				continue;
			}
			// (The assembly tested firstPutBack == n again here; it cannot be.)
		}
		node->flags &= ~DLN_WAITING;
		firstPutBack = 0;
		if (node->area.left > right || node->area.top > bottom ||
			node->area.right < left || node->area.bottom < top)
			continue;
		_drawing_queue.add_tail(n);
		node->flags |= DLN_DRAWING;
		draw(n);
	}

	while (_drawing_queue.head)
	{
		n = _drawing_queue.remove_head();
		node = NODE(n);
		node->flags &= ~DLN_DRAWING;
		if (animating)
		{
			_waiting_queue.add_tail(n);
			node->flags |= DLN_WAITING;
		}
		else
			node->draw();
	}

	if (stuck)
		trace((TraceLevel)50, dlLoopMsg);
	if (animating)
	{
		animating = 0;
		draw();
	}
}

// static void DisplayList::checkNewItems(void)
// Flags in animatingitem each item of newitem whose shape animates and whose
// box reaches the screen, then clears newitem.
void DisplayList::checkNewItems(void)
{
	uint16_t firstItem = 0;
	for (uint16_t offs = 0; offs < NEWITEM_BYTES; offs += 4, firstItem += 32)
	{
		uint32_t *newBits = &newitem[offs >> 2];
		if (*newBits == 0)
			continue;
		uint32_t bits = *newBits;
		uint16_t item = firstItem;
		uint32_t bit = 1;
		for (int16_t i = 0; i < 32; i++, item++, bit <<= 1)
		{
			if (!(bits & bit))
				continue;
			if (!GlobalTypes.typeFlags[ItemData::typeArray[item]].animType)
				continue;

			// sx = x - y, sy = (x + y) / 2 - z, x and y in world units / 4
			uint16_t x = ItemData::xArray[item] >> 2;
			uint16_t y = ItemData::yArray[item] >> 2;
			uint8_t z = ItemData::zArray[item];
			int16_t sx = x - y;
			int16_t sy = (int16_t)y >> 1;
			sy += (int16_t)x >> 1;
			sy -= z;

			int16_t xSize, ySize, zSize;
			itemFootpad(item, xSize, ySize, zSize);

			// Skip the item unless its box overlaps the screen. Each test is a
			// 16-bit signed compare, as the assembly made it.
			int16_t t = sx - xSize - Camera::_h;
			if (t >= SCREEN_W)
				continue;
			t = sy - (xSize >> 1) - (ySize >> 1) - zSize - Camera::_v;
			if (t >= SCREEN_H)
				continue;
			int16_t r = sx + ySize;
			if (r < Camera::_h)
				continue;
			if (sy < Camera::_v)
				continue;
			animatingitem[offs >> 2] |= bit;
		}
		*newBits = 0;
	}
}

// static unsigned char DisplayList::sort(unsigned short node)
// Sorts node against each node in its shadow squares whose rectangle overlaps
// it, and against the squares. Returns 0 as soon as a pairwise sort fails.
uint8_t DisplayList::sort(uint16_t n)
{
	DisplayListNode *node = NODE(n);
	int16_t firstCol, firstRow, lastCol, lastRow, col, row;
	uint16_t marked = 0;

	// Grid squares under the node's rectangle.
	firstCol = (int16_t)(node->area.left - ShadowSquareLC) >> SQUARE_SHIFT;
	firstRow = (int16_t)(node->area.top - ShadowSquareTC) >> SQUARE_SHIFT;
	lastCol = (int16_t)(node->area.right - ShadowSquareLC) >> SQUARE_SHIFT;
	lastRow = (int16_t)(node->area.bottom - ShadowSquareTC) >> SQUARE_SHIFT;
	if (firstCol >= SQUARE_COLS || firstRow >= SQUARE_ROWS || lastCol < 0 || lastRow < 0)
		return 1;
	if (firstCol < 0)
		firstCol = 0;
	if (firstRow < 0)
		firstRow = 0;
	if (lastCol >= SQUARE_COLS)
		lastCol = SQUARE_COLS - 1;
	if (lastRow >= SQUARE_ROWS)
		lastRow = SQUARE_ROWS - 1;

	for (row = firstRow; row <= lastRow; row++)
	{
		for (col = firstCol; col <= lastCol; col++)
		{
			uint16_t square = ShadowSquare[row][col];

			// Sort against each unmarked node above this square that overlaps
			// node. Every unmarked node met is marked, overlapping or not.
			for (uint16_t e = NODE(square)->above; e; e = EDGE(e)->nextAbove)
			{
				uint16_t other = EDGE(e)->upper;
				DisplayListNode *on = NODE(other);
				if (on->flags & DLN_MARKED)
					continue;
				if (!(node->area.left > on->area.right || node->area.top > on->area.bottom ||
					node->area.right < on->area.left || node->area.bottom < on->area.top))
				{
					// A failure returns at once and leaves the marks set, as
					// the assembly did.
					if (!sort(n, other))
						return 0;
				}
				on->flags |= DLN_MARKED;
				on->markNext = marked;
				marked = other;
			}
			if (!sort(n, square))
				return 0;
		}
	}

	// Clear the marks.
	while (marked)
	{
		DisplayListNode *mn = NODE(marked);
		mn->flags &= ~DLN_MARKED;
		marked = mn->markNext;
	}
	return 1;
}

// int DL_ItemNode::compare(unsigned short other)
// Orders two item nodes whose boxes overlap on screen: by x, then y, then z,
// then the unknown1 type flag, then by depth sum. Returns a CMP_ value.
// This node's position and sizes come from the node; the other's from its item.
int16_t DL_ItemNode::compare(uint16_t other)
{
	uint16_t item = ((DL_ItemNode *)NODE(other))->referent;

	// The other item's x, y, z (world units / 4) and sizes.
	int16_t itsX = ItemData::xArray[item] >> 2;
	int16_t itsY = ItemData::yArray[item] >> 2;
	int16_t itsZ = ItemData::zArray[item];
	int16_t xSize, ySize, zSize;
	itemFootpad(item, xSize, ySize, zSize);

	// Apart unless the two outlines overlap along x - z, y - z and x - y.
	// All compares are 16-bit signed, on values wrapped to 16 bits.
	int16_t a, b;
	int16_t myXZ = x - z;
	a = myXZ - xd - zd;
	b = itsX - itsZ;
	if (a >= b)
		return CMP_APART;
	a = b - xSize - zSize;
	if (myXZ <= a)
		return CMP_APART;
	int16_t myYZ = y - z;
	a = myYZ - yd - zd;
	int16_t itsYZ = itsY - itsZ;
	if (a >= itsYZ)
		return CMP_APART;
	a = itsYZ - ySize - zSize;
	if (myYZ <= a)
		return CMP_APART;
	int16_t itsScreenX = itsX - itsY;
	a = screenX - xd;
	b = itsScreenX + ySize;
	if (a >= b)
		return CMP_APART;
	a = screenX + yd;
	b = itsScreenX - xSize;
	if (a <= b)
		return CMP_APART;

	// Boxes separated along x, then y.
	a = x - xd;
	if (a >= itsX)
		return CMP_OVER;
	a = itsX - xSize;
	if (x <= a)
		return CMP_UNDER;
	a = y - yd;
	if (a >= itsY)
		return CMP_OVER;
	a = itsY - ySize;
	if (y <= a)
		return CMP_UNDER;

	// Then along z.
	int16_t itsTop = itsZ + zSize;
	int16_t myTop = z + zd;
	if (z >= itsTop && myTop > itsZ)
		return CMP_OVER;
	if (myTop <= itsZ && z < itsTop)
		return CMP_UNDER;

	// A type with unknown1 goes over one without.
	uint8_t mine = GlobalTypes.typeFlags[ItemData::typeArray[referent]].unknown1 ? 1 : 0;
	uint8_t its = GlobalTypes.typeFlags[ItemData::typeArray[item]].unknown1 ? 1 : 0;
	if (mine && !its)
		return CMP_OVER;
	if (its && !mine)
		return CMP_UNDER;

	// Last resort: (x - xSize) + (y - ySize) + 2z, the larger goes over.
	int16_t mySum = x - xd + y - yd + z + z;
	int16_t itsSum = itsX - xSize + itsY - ySize + itsZ + itsZ;
	if (mySum > itsSum)
		return CMP_OVER;
	if (mySum < itsSum)
		return CMP_UNDER;
	return CMP_APART;
}
