// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: DLITEM.C

#include "u8port.h"


#include "item.h"
#include "itemdata.h"
#include "npc.h"
#include "type.h"
#include "gametime.h"
#include "basecam.h"
#include "camera.h"
#include "cglobvp.h"
#include "crect.h"
#include "dispatch.h"
#include "error.h"
#include "pal.h"
#include "shaphand.h"
#include "wpncache.h"
#include "dlist.h"
#include "dlitem.h"

// Inline in the shared headers when this file was built.
inline Point::Point(void) {}
inline Point::Point(int16_t x, int16_t y) { set(x, y); }
inline void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
inline Rect::Rect(void) {}
inline void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
inline Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }

extern "C" int16_t SkipDraw(Vport *, int16_t, int16_t, char *);
extern "C" int16_t FlipSkipDraw(Vport *, int16_t, int16_t, char *);
extern "C" int16_t SkipTransformDraw(Vport *, int16_t, int16_t, char *, uint8_t *);
extern "C" int16_t FlipSkipTransformDraw(Vport *, int16_t, int16_t, char *, uint8_t *);
extern "C" int16_t SkipDoubleTransformDraw(Vport *, int16_t, int16_t, char *, uint8_t *);
extern "C" int16_t FlipSkipDoubleTransformDraw(Vport *, int16_t, int16_t, char *, uint8_t *);
extern "C" uint8_t Collision(char *, Point, Point);

extern uint8_t inGameMode;


class NodeItem : public Item
{
public:
	NodeItem(Referent r) { referent = r; }
};

inline Item itemOf(Referent r) { return NodeItem(r); }


uint8_t DL_ItemNode::showFootpads = 0;
uint8_t DL_ItemNode::footpadTargetMode = 0;
uint8_t DL_ItemNode::ignoreMode = 1;
uint16_t DL_ItemNode::_popref = 0;
uint8_t DL_ItemNode::gumpflag = 0;
uint8_t DL_ItemNode::drawflag = 0;
ScreenArea DL_ItemNode::gumprect;
ScreenArea DL_ItemNode::drawrect;

void DL_ItemNode::draw(void)
{
	Item item;
	TypeFlag flags;
	int16_t xd, yd, zd;
	item = itemOf(referent);
	uint16_t type = ItemData::typeArray[item.referent];
	uint16_t frame = item.getFrame();
	flags = GlobalTypes.typeFlags[type];
	if (flags.animType == 1)
	{
		// Frames that follow the clock rather than an animation process.
		int16_t speed = flags.animSpeed;
		if (!speed)
			speed++;
		int16_t data = flags.animData;
		if (data)
			frame = (frame + theAnimation->f_3c / speed) % data + (frame - frame % data);
		else
		{
			int16_t maxWidth, maxHeight, frames;
			shapeHandler->getInfo(type, maxWidth, maxHeight, frames);
			frame = (frame + theAnimation->f_3c / speed) % frames;
		}
	}
	char *shape = shapeHandler->get(type, frame);
	if (shape)
	{
		uint16_t status = ItemData::statusArray[item.referent];
		if (status & FLIPPED)
		{
			if (status & INVISIBLE)
				FlipSkipTransformDraw(&DisplayList::vport, screenX - Camera::_h, screenY - Camera::_v, shape, theTransformPalette->table);
			else if (flags.trans)
				FlipSkipDoubleTransformDraw(&DisplayList::vport, screenX - Camera::_h, screenY - Camera::_v, shape, theTransformPalette->tables);
			else
				FlipSkipDraw(&DisplayList::vport, screenX - Camera::_h, screenY - Camera::_v, shape);
		}
		else
		{
			if (status & INVISIBLE)
				SkipTransformDraw(&DisplayList::vport, screenX - Camera::_h, screenY - Camera::_v, shape, theTransformPalette->table);
			else if (flags.trans)
				SkipDoubleTransformDraw(&DisplayList::vport, screenX - Camera::_h, screenY - Camera::_v, shape, theTransformPalette->tables);
			else
				SkipDraw(&DisplayList::vport, screenX - Camera::_h, screenY - Camera::_v, shape);
		}
	}
	// The avatar's drawn weapon is an overlay of its own.
	Item weapon;
	if (item.isAvatar() && (avatar.isInCombat() || avatar.getLastAnimSet() == 6))
	{
		Npc npc(item.referent);
		weapon = npc.getEquip(5);
		if (weapon.isValid() && WeaponCache::isWeaponDrawn(weapon.getType(), (AnimSet)npc.getLastAnimSet()))
		{
			uint16_t wshape, wframe;
			char dx, dy;
			WeaponCache::getOffsets(weapon.getType(), (AnimSet)npc.getLastAnimSet(), npc.getDir(),
				Npc::npcData[npc.referent].animFrame, wshape, dx, dy, wframe);
			shape = shapeHandler->get(wshape, wframe);
			SkipDraw(&DisplayList::vport, screenX - Camera::_h + dx, screenY - Camera::_v + dy, shape);
		}
	}
	if (showFootpads)
	{
		flags = GlobalTypes.typeFlags[type];
		if (ItemData::statusArray[item.referent] & FLIPPED)
			footpad(yd, xd, zd, flags);
		else
			footpad(xd, yd, zd, flags);
		int16_t sx = screenX - Camera::_h;
		int16_t sy = screenY - Camera::_v;
		DrawLine(&DisplayList::vport, sx, sy, sx + yd, sy - (yd >> 1), 15);
		DrawLine(&DisplayList::vport, sx, sy, sx - xd, sy - (xd >> 1), 15);
		DrawLine(&DisplayList::vport, sx, sy, sx, sy - zd, 15);
		DrawLine(&DisplayList::vport, sx + yd, sy - (yd >> 1), sx + yd, sy - (yd >> 1) - zd, 15);
		DrawLine(&DisplayList::vport, sx - xd, sy - (xd >> 1), sx - xd, sy - (xd >> 1) - zd, 15);
		DrawLine(&DisplayList::vport, sx, sy - zd, sx + yd, sy - (yd >> 1) - zd, 15);
		DrawLine(&DisplayList::vport, sx, sy - zd, sx - xd, sy - (xd >> 1) - zd, 15);
		DrawLine(&DisplayList::vport, sx - xd + yd, sy - (xd >> 1) - (yd >> 1) - zd, sx + yd, sy - (yd >> 1) - zd, 15);
		DrawLine(&DisplayList::vport, sx - xd + yd, sy - (xd >> 1) - (yd >> 1) - zd, sx - xd, sy - (xd >> 1) - zd, 15);
	}
}

// Recomputes the node's position and screen area from its item.
void DL_ItemNode::update(void)
{
	int16_t left, top, right, bottom;
	Item item;
	char *shape;
	Rect r;
	item = itemOf(referent);
	x = ItemData::xArray[item.referent] >> 2;
	y = ItemData::yArray[item.referent] >> 2;
	z = ItemData::zArray[item.referent];
	screenX = x - y;
	screenY = (x >> 1) + (y >> 1) - z;
	TypeFlag *type = &GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
	if (ItemData::statusArray[item.referent] & FLIPPED)
		footpad(yd, xd, zd, *type);
	else
		footpad(xd, yd, zd, *type);
	shape = shapeHandler->get(ItemData::typeArray[item.referent], item.getFrame());
	if (!shape)
		shape = shapeHandler->get(0, 0);
	if (shape)
	{
		shapeHandler->getRect(ItemData::typeArray[item.referent], item.getFrame(), r, 0, 0);
		if (ItemData::statusArray[item.referent] & FLIPPED)
		{
			left = screenX - r.f_04;
			top = screenY + r.f_02;
			right = screenX - r.f_00;
			bottom = screenY + r.f_06;
		}
		else
		{
			left = screenX + r.f_00;
			top = screenY + r.f_02;
			right = screenX + r.f_04;
			bottom = screenY + r.f_06;
		}
		if (item.isAvatar() && (avatar.isInCombat() || avatar.getLastAnimSet() == 6))
		{
			Item weapon = avatar.getEquip(5);
			if (weapon.isValid() && WeaponCache::isWeaponDrawn(weapon.getType(), (AnimSet)avatar.getLastAnimSet()))
			{
				uint16_t wshape, wframe;
				char dx, dy;
				Rect wr;
				WeaponCache::getOffsets(weapon.getType(), (AnimSet)avatar.getLastAnimSet(), avatar.getDir(),
					Npc::npcData[avatar.referent].animFrame, wshape, dx, dy, wframe);
				shapeHandler->getRect(wshape, wframe, wr, 0, 0);
				wr.moverel(screenX + dx, screenY + dy);
				if (wr.f_00 < left)
					left = wr.f_00;
				if (wr.f_04 > right)
					right = wr.f_04;
				if (wr.f_02 < top)
					top = wr.f_02;
				if (wr.f_06 > bottom)
					bottom = wr.f_06;
			}
		}
	}
	else
		halt(__FILE__, 482);	// __LINE__
	area = ScreenArea(left, top, right, bottom);
}

// Takes the item out of the display list.
void DL_ItemNode::flag_out_of_list(void)
{
	Item item;
	item = Item(referent);
	item.andStatus(~IN_DISPLAY_LIST);
	DisplayList::stopAnimating(referent);
	DisplayList::clearNew(referent);
}

// Looks for item r's node in the shadow squares x1..x2, y1..y2.
uint16_t DL_ItemNode::find(uint16_t r, int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
	int16_t x, y;
	DisplayListNode *sq, *n;
	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			if (x >= 0 && x < SQUARE_COLS && y >= 0 && y < SQUARE_ROWS)
			{
				sq = NODE(ShadowSquare[y][x]);
				for (uint16_t e = sq->above; e; e = EDGE(e)->nextAbove)
				{
					uint16_t up = EDGE(e)->upper;
					n = NODE(up);
					if (n->nodetype() == 2 && ((DL_ItemNode *)n)->referent == r)
						return up;
				}
			}
		}
	}
	return 0;
}

uint16_t DL_ItemNode::find(uint16_t r)
{
	uint16_t n = 0;
	Item item;
	int16_t sx, xd, yd, zd;
	item = itemOf(r);
	if (ItemData::statusArray[item.referent] & IN_DISPLAY_LIST)
	{
		sx = (ItemData::xArray[item.referent] >> 2) - (ItemData::yArray[item.referent] >> 2);
		int16_t sy = (ItemData::xArray[item.referent] >> 3) + (ItemData::yArray[item.referent] >> 3) - ItemData::zArray[item.referent];
		TypeFlag *type = &GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
		if (ItemData::statusArray[item.referent] & FLIPPED)
			footpad(yd, xd, zd, *type);
		else
			footpad(xd, yd, zd, *type);
		int16_t left = sx - xd;
		int16_t top = sy - (xd >> 1) - (yd >> 1) - zd;
		int16_t right = sx + yd;
		int16_t bottom = sy;
		left = (left - ShadowSquareLC) >> SQUARE_SHIFT;
		top = (top - ShadowSquareTC) >> SQUARE_SHIFT;
		right = (right - ShadowSquareLC) >> SQUARE_SHIFT;
		bottom = (bottom - ShadowSquareTC) >> SQUARE_SHIFT;
		n = find(r, left, top, right, bottom);
		if (!n)
			n = find(r, 0, 0, SQUARE_COLS - 1, SQUARE_ROWS - 1);
	}
	return n;
}

// Takes item r out of the display list before it moves, and marks where it was.
void DL_ItemNode::before_push(uint16_t r)
{
	NodeItem item(r);
	ScreenArea area;
	if (_popref == r)
	{
		_popref = 0;
		return;
	}
	if (ItemData::statusArray[item.referent] & IN_DISPLAY_LIST)
	{
		uint16_t n = find(r);
		if (n)
		{
			area = NODE(n)->area;
			area.left -= (uint16_t)Camera::_h;
			area.top -= (uint16_t)Camera::_v;
			area.right -= (uint16_t)Camera::_h;
			area.bottom -= (uint16_t)Camera::_v;
			if (drawflag && area.intersects(FullScreenArea) && !area.intersects(drawrect))
				delayed_draw();
			DisplayList::delete_node(n);
			if (area.intersects(FullScreenArea))
			{
				area.intersect(FullScreenArea);
				int16_t left = area.left;
				int16_t top = area.top;
				int16_t right = area.right;
				int16_t bottom = area.bottom;
				if (!inGameMode)
					DrawBox(GlobalVport::global_ptr, left, top, right, bottom, 0x58);
				TypeFlag flags = GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
				if (!inGameMode || flags.bag || r == 1)
					theCamera->show(left, top, right, bottom, 0);
				else if (drawflag)
					drawrect.include(area);
				else
				{
					drawrect = area;
					drawflag = 1;
				}
				if (gumpflag)
					gumprect.include(area);
				else
				{
					gumprect = area;
					gumpflag = 1;
				}
			}
		}
		item.andStatus(~IN_DISPLAY_LIST);
	}
}

void DL_ItemNode::after_pop(uint16_t r)
{
	if (_popref)
		delayed_pop();
	_popref = r;
	BaseCamera::needSlam = 1;
}

// Puts the item held by after_pop back in the display list.
void DL_ItemNode::delayed_pop(void)
{
	DL_ItemNode *node;
	ScreenArea grid;
	ScreenArea area;
	if (!_popref)
		return;
	NodeItem item(_popref);
	if (drawflag && GlobalTypes.typeFlags[ItemData::typeArray[item.referent]].bag)
		delayed_draw();
	int16_t left = ShadowSquareLC;
	int16_t top = ShadowSquareTC;
	grid = ScreenArea(left, top, left + SQUARE_COLS * SQUARE_SIZE - 1, top + SQUARE_ROWS * SQUARE_SIZE - 1);
	uint16_t n = (uint16_t)new DL_ItemNode;
	if (n)
	{
		node = (DL_ItemNode *)NODE(n);
		node->set(_popref);
		if (node->area.intersects(grid))
		{
			area = node->area;
			area.left -= (uint16_t)Camera::_h;
			area.top -= (uint16_t)Camera::_v;
			area.right -= (uint16_t)Camera::_h;
			area.bottom -= (uint16_t)Camera::_v;
			if (drawflag && area.intersects(FullScreenArea) && !area.intersects(drawrect))
				delayed_draw();
			item.orStatus(IN_DISPLAY_LIST);
			DisplayList::add_node(n);
			if (!DisplayList::sort(n))
			{
				item.andStatus(~IN_DISPLAY_LIST);
				DisplayList::remove_node(n);
				delete node;
			}
			else if (ItemData::statusArray[item.referent] & IN_DISPLAY_LIST)
			{
				if (node->flags & DLN_UNDER_TRANSLUCENT)
					DisplayList::mark(node->area);
				if (area.intersects(FullScreenArea))
				{
					area.intersect(FullScreenArea);
					if (drawflag)
						drawrect.include(area);
					else
					{
						drawrect = area;
						drawflag = 1;
					}
					Rect rect(node->area.left - Camera::_h, node->area.top - Camera::_v,
						node->area.right - Camera::_h, node->area.bottom - Camera::_v);
					rect.clip(GlobalVport::global_ptr->rect);
					if (gumpflag)
						gumprect.include(ScreenArea(rect));
					else
					{
						gumprect = ScreenArea(rect);
						gumpflag = 1;
					}
				}
			}
		}
		else
			delete node;
	}
	_popref = 0;
}

void DL_ItemNode::delayed_draw(void)
{
	if (drawflag)
	{
		DisplayList::clip_l = drawrect.left;
		DisplayList::clip_t = drawrect.top;
		DisplayList::clip_r = drawrect.right;
		DisplayList::clip_b = drawrect.bottom;
		DisplayList::vport = *GlobalVport::global_ptr;
		DisplayList::vport.rect.set(drawrect.left, drawrect.top, drawrect.right, drawrect.bottom);
		DisplayList::animating = 1;
		DisplayList::draw();
		drawflag = 0;
	}
}

void DL_ItemNode::include_area(int16_t left, int16_t top, int16_t right, int16_t bottom)
{
	ScreenArea area(left, top, right, bottom);
	area.intersect(FullScreenArea);
	if (drawflag)
		drawrect.include(area);
	else
	{
		drawrect = area;
		drawflag = 1;
	}
	if (gumpflag)
		gumprect.include(area);
	else
	{
		gumprect = area;
		gumpflag = 1;
	}
}

void DL_ItemNode::delayed_gump(void)
{
	if (gumpflag)
	{
		dispatcher->refresh(Rect(gumprect.left, gumprect.top, gumprect.right, gumprect.bottom));
		gumpflag = 0;
	}
}

// Whether screen point (x, y) hits the item: inside its footpad box, or on
// a drawn pixel of its shape.
uint8_t DL_ItemNode::is_target(int16_t x, int16_t y)
{
	NodeItem item(referent);
	TypeFlag flags;
	uint16_t type = ItemData::typeArray[item.referent];
	flags = GlobalTypes.typeFlags[type];
	if (ignoreMode && flags.ignore)
		return 0;
	if (footpadTargetMode)
	{
		int16_t xd, yd, zd;
		x -= screenX;
		y -= screenY;
		if (ItemData::statusArray[item.referent] & FLIPPED)
			footpad(yd, xd, zd, flags);
		else
			footpad(xd, yd, zd, flags);
		if (-xd <= x && x <= yd && -zd - (xd >> 1) - (yd >> 1) <= y && y <= 0)
		{
			int16_t side = y + (x >> 1);
			if (side <= 0 && -xd - zd <= side)
			{
				side = y - (x >> 1);
				if (side <= 0 && -yd - zd <= side)
					return 1;
			}
		}
	}
	else
	{
		if (area.intersects(ScreenArea(x, y)))
		{
			Point origin, p;
			origin = Point(screenX, screenY);
			char *shape = shapeHandler->get(ItemData::typeArray[item.referent], item.getFrame());
			if (shape)
			{
				if (item.getStatus() & FLIPPED)
					p = Point(screenX - (x - screenX), y);
				else
					p = Point(x, y);
				return Collision(shape, origin, p);
			}
		}
	}
	return 0;
}

int16_t DL_ItemNode::getReferent(void)
{
	return referent;
}

uint8_t DL_ItemNode::isTranslucent(void)
{
	NodeItem item(referent);
	return (ItemData::statusArray[item.referent] & INVISIBLE) ||
		GlobalTypes.typeFlags[ItemData::typeArray[item.referent]].trans;
}

char isGameRunning(void)
{
	return DL_ItemNode::ignoreMode;
}
