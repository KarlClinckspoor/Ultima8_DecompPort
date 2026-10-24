// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: GRAVITY.C

#include "u8port.h"
#include "plat.h"

// #include <dos.h>  (DOS only)
#include <stdlib.h>
#include "npc.h"
#include "itemdata.h"
#include "itemfind.h"
#include "type.h"
#include "kernel.h"
#include "priority.h"
#include "error.h"
#include "colbuf.h"
#include "motrak.h"
#include "statrest.h"
#include "scratchm.h"
#include "dsfxman.h"
#include "gravity.h"

// Inline in the shared headers when this file was built.
inline void killAnimProcesses(uint16_t ref)
{
	PriorityClass all(0x21);
	Kernel::killProcess(ref, PT_FIRST, all);
}

ItemSupport::ItemSupport(uint16_t item, uint8_t strict)
{
	Npc npc(item);
	figureInfo(ItemData::typeArray[npc.referent], npc.getLoc(), strict, item);
}

// Looks at what lies under each corner of an item of the given type at the point;
// strict counts only things whose top is exactly at the point's height.
void ItemSupport::figureInfo(uint16_t type, WorldPoint &point, uint8_t strict, uint16_t self)
{
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t xSize;
	int16_t ySize;
	int16_t zSize;
	int16_t top;
	int16_t x1;
	int16_t y1;
	int16_t x2;
	int16_t y2;
	int16_t midX1;
	int16_t midX2;
	int16_t midY1;
	int16_t midY2;
	TypeFlag flag;
	uint16_t volume;
	uint16_t blockers;
	Boolean touching = 0;
	Item item;

	x = point.x;
	y = point.y;
	flag = GlobalTypes.typeFlags[type];
	flag.getWorldSize(xSize, ySize, zSize);
	z = point.z;
	if (z <= 0)
	{
		flags = 0x1f;
		return;
	}
	flags = 0;
	volume = CollisionBuffer::volumeFlags(flag);
	x1 = x - xSize + 1;
	y1 = y - ySize + 1;
	x2 = x;
	y2 = y;
	midX1 = (uint16_t)(x1 + x2) >> 1;
	midX2 = (uint16_t)(x1 + x2 + 1) >> 1;
	midY1 = (uint16_t)(y1 + y2) >> 1;
	midY2 = (uint16_t)(y1 + y2 + 1) >> 1;
	{
		AreaItemFinder finder(x1, y1, x2 + 480, y2 + 480);
		for (; finder.found(); finder.findNext())
		{
			item.set(finder.referent);
			x = ItemData::xArray[item.referent];
			y = ItemData::yArray[item.referent];
			if (item.referent == self)
				continue;
			if (x < x1 || y < y1 || ItemData::zArray[item.referent] > z)
				continue;
			flag = GlobalTypes.typeFlags[ItemData::typeArray[item.referent]];
			flag.getWorldSize(xSize, ySize, zSize);
			top = ItemData::zArray[item.referent] + zSize;
			if (x - xSize + 1 > x2 || y - ySize + 1 > y2)
				continue;
			if (top != z && (strict == FALSE || top > z || z - 9 > top))
				continue;
			blockers = CollisionBuffer::volumeFlags(flag) & 0x7fff;
			if (self && !(CollisionBuffer::surfaceFlags(flag) & 4))
				blockers &= ~1;
			if (!(blockers & volume))
				continue;
			if (top == z)
				touching = 1;
			if (x - xSize + 1 <= midX1)
			{
				if (y - ySize + 1 <= midY1)
					flags |= SUPPORT_WEST_NORTH;
				if (y >= midY2)
					flags |= SUPPORT_WEST_SOUTH;
			}
			if (x >= midX2)
			{
				if (y - ySize + 1 <= midY1)
					flags |= SUPPORT_EAST_NORTH;
				if (y >= midY2)
					flags |= SUPPORT_EAST_SOUTH;
			}
		}
	}
	if ((flags & SUPPORT_WEST_NORTH && flags & SUPPORT_EAST_SOUTH) ||
		(flags & SUPPORT_WEST_SOUTH && flags & SUPPORT_EAST_NORTH))
		flags |= SUPPORTED;
	if (strict && !touching)
		flags = 0;
}

GravityTracker::GravityTracker(uint16_t item, int16_t x, int16_t y, int16_t z, int16_t g, int16_t ticks, WorldPoint *point)
{
	Kernel::setIdString(pid, "Gravity");
	setRef(item);
	setProcessType((ProcessType)0x203);
	vx = x;
	vy = y;
	vz = z;
	gravity = g;
	lifetime = ticks;
	dest = point;
	sliding = 0;
	grabbed = 0;
	Item(item).andStatus(~0x400);
	Npc npc(ref);
	if (npc.isNpc())
	{
		if (npc.isFalling())
			halt(__FILE__, 320);	// __LINE__
		Npc::npcData[npc.referent].falling = 1;
		npc.setFallZ(npc.getZ());
	}
}

// A falling NPC lands: hurt by long falls, otherwise it gets up.
GravityTracker::~GravityTracker(void)
{
	Npc npc(ref);
	if (npc.getZ() < 251 && npc.isNpc())
	{
		Npc::npcData[npc.referent].falling = 0;
		if (!isFailed() && !grabbed)
		{
			uint8_t startZ = npc.getFallZ();
			int16_t fall = (startZ - npc.getZ()) / 8;
			if (npc.isDead())
			{
				Process *anim = Kernel::getLinearExecutor(ref, (ProcessType)0xf0);
				if (anim)
					Kernel::checkSynchronization(anim);
			}
			else if (fall >= 10)
			{
				int16_t damage;
				if (fall >= 13)
					damage = npc.getHp();
				else
					damage = (fall - 9) << 1;
				npc.receiveHit(0, npc.getDir(), damage, 0x180);
				playSFX(51, (int16_t)250);
			}
			else if (npc.getLastAnimSet() != ANIM_DIE)
			{
				killAnimProcesses(npc.referent);
				npc.doAnim(ANIM_LAND, 8, 0, 0);
				if (npc.isInCombat())
					npc.doAnim(ANIM_COMBAT_STAND, 8, 1, 0);
			}
		}
	}
}

void GravityTracker::process(void)
{
	MotionTracker tracker;
	int16_t x;
	int16_t y;
	int16_t z;
	Npc item(ref);
	uint16_t blocked;
	Boolean landed = 0;
	Boolean stepped;

	if (item.getZ() >= 251)
	{
		Kernel::killProcess(ref, (ProcessType)6, PriorityClass(0x21));
		return;
	}
	do
	{
		if (ItemData::statusArray[item.referent] & CONTAINED)
		{
			pop(0);
			return;
		}
		if (dest)
		{
			int16_t dist = abs(ItemData::zArray[item.referent] - dest->z) +
				(abs(ItemData::xArray[item.referent] - dest->x) +
				abs(ItemData::yArray[item.referent] - dest->y));
			if (dist < 50)
			{
				pop(0);
				return;
			}
		}
		if (!item.isInFastArea())
		{
			if (dest)
				pop(0);
			return;
		}
		if (vz == 0 || (ItemData::statusArray[item.referent] & 0x4400))
		{
			if (vx || vy)
				item.grab();
			else
			{
				for (SurfaceItemFinder above(item.referent, "$", 1, 1); above.found(); above.findNext())
					above.fall();
			}
		}
		item.andStatus(~0x400);
		x = ItemData::xArray[item.referent] + vx;
		y = ItemData::yArray[item.referent] + vy;
		z = ItemData::zArray[item.referent] + vz;
		vz -= gravity;
		blocked = 0;
		if (move())
			return;
		stepped = 0;
		if (!tracker.tryMove(ref, x, y, z, 4, 0))
		{
			afterMove(tracker.hitItem);
			blocked = tracker.blocked;
			if (blocked)
				item.orStatus(0x400);
			if (blocked & BLOCKED_Z)
			{
				if (ItemData::statusArray[item.referent] & 0x4000)
				{
					tracker.tryMove(ref, x, y, tracker.point.z, 4, 0);
					blocked = tracker.blocked | BLOCKED_Z;
				}
				stepped = 1;
			}
		}
		if (dest)
			item.move(tracker.point);
		else
		{
			tracker.makeItSo();
			if (isPhantom())
				return;
		}
		if (tracker.offMap)
			vx = vy = 0;
		if (blocked)
		{
			int16_t unused;		// gives this block its own stack scope

			if (dest)
			{
				pop(0);
				return;
			}
			if (!sliding)
			{
				if (ItemData::statusArray[item.referent] & 0x4000)
				{
					int16_t r = urandom(20);
					if (r < 4)
					{
						if (vx < 0)
							vx++;
						else if (vx > 0)
							vx--;
						else
							vx = 0;
					}
					else if (r < 8)
					{
						if (vy < 0)
							vy++;
						else if (vy > 0)
							vy--;
						else
							vy = 0;
					}
					else if (r == 8)
						vx++;
					else if (r == 9)
						vx--;
					else if (r == 10)
						vy++;
					else if (r == 11)
						vy--;
					if (!(ItemData::statusArray[item.referent] & 0x400))
						blocked &= ~(BLOCKED_X | BLOCKED_Y);
				}
				if (!(ItemData::statusArray[item.referent] & 0x4000) || (blocked & (BLOCKED_X | BLOCKED_Y)))
				{
					if (vx < 0)
						vx++;
					vx >>= 2;
					if (vy < 0)
						vy++;
					vy >>= 2;
				}
			}
			if (!(blocked & BLOCKED_Z))
			{
				if (vz < 0)
					vz++;
				vz >>= 2;
			}
			if (blocked & BLOCKED_X)
			{
				if (!sliding && item.isNpc())
					vx = 0;
				else
					vx = -vx;
			}
			if (blocked & BLOCKED_Y)
			{
				if (!sliding && item.isNpc())
					vy = 0;
				else
					vy = -vy;
			}
			if (landed)
				sliding = 0;
			if (blocked & BLOCKED_Z)
			{
				vz += gravity;
				landed = 0;
				if (vz <= 0)
				{
					ItemSupport support(ref, 0);
					support = ItemSupport(ref, 0);
					if (support.onlyNorth())
						vy += 4;
					if (support.onlyEast())
						vx -= 4;
					if (support.onlySouth())
						vy -= 4;
					if (support.onlyWest())
						vx += 4;
					landed = support.flags & SUPPORTED;
				}
				if (ItemData::statusArray[item.referent] & 0x4400)
				{
				if (landed)
				{
					if (stepped && item.isNpc())
					{
						pop(0);
						return;
					}
					if (vz < 0)
					{
						vz = -vz;
						vz >>= 2;
					}
					else
					{
						vz >>= 2;
						vz = -vz;
					}
					sliding = 0;
					if (ItemData::statusArray[item.referent] & 0x4000)
					{
						if (vz < gravity)
							vz = 0;
						else
						{
							vx += urandom(vz);
							vx -= urandom(vz);
							vy += urandom(vz);
							vy -= urandom(vz);
						}
						if (abs(vx) < 4 && abs(vy) < 4 && vz < gravity)
						{
							pop(0);
							return;
						}
					}
					else if (vz < gravity)
					{
						pop(0);
						return;
					}
				}
				else
				{
					if (abs(vx) < 16)
					{
						if (vx > 0)
							vx += 4;
						else if (vx < 0)
							vx -= 4;
						else
							urandom(2) ? vx += 4 : vx -= 4;
					}
					if (abs(vy) < 16)
					{
						if (vy > 0)
							vy += 4;
						else if (vy < 0)
							vy -= 4;
						else
							urandom(2) ? vy += 4 : vy -= 4;
					}
					vz = 0;
					sliding = 1;
				}
				}
				else
				{
					pop(0);
					return;
				}
			}
			if (landed && impact())
			{
				pop(0);
				return;
			}
		}
		if (lifetime && --lifetime == 0)
		{
			pop(0);
			return;
		}
	} while (dest);
}

uint8_t GravityTracker::impact(void)
{
	MotionTracker tracker;
	Npc item(ref);

	if (vz * gravity <= 0)
	{
		if (!tracker.tryMove(ref, WorldPoint(ItemData::xArray[item.referent], ItemData::yArray[item.referent], ItemData::zArray[item.referent] + vz), 4, 0))
			return TRUE;
	}
	return FALSE;
}

void GravityTracker::bump(int16_t x, int16_t y, int16_t z)
{
	vx += x;
	vy += y;
	vz += z;
}

void GravityTracker::cancel(void)
{
	Item item(ref);
	MotionTracker tracker;

	tracker.tryMove(item.referent, item.getX(), item.getY(), 0, 4, 0);
	tracker.makeItSo();
	if (isPhantom())
		return;
	pop(0);
}

uint8_t GravityTrackerCling::move(void)
{
	uint8_t buttons;
	Boolean caught = 0;

	// int 33h function 3: the buttons held now.
	{
		uint16_t held;
		plat_mouse_state(0, 0, &held);
		buttons = (uint8_t)held;
	}
	if (buttons & 2)
	{
		Npc npc(ref);
		WorldPoint to;
		WorldPoint from;
		int16_t x = npc.getX();
		int16_t y = npc.getY();
		uint8_t z = avatar.getZ() & 0xf8;
		char dir = avatar.getDir();
		Process *waiter = new ResultProcess;
		AnimPrimitive *climb;
		int16_t i;

		hitItem = 0;
		for (i = 0; i < 5; i++)
		{
			if (i == 0)
				to.set(x + x8add[dir] * 64, y + y8add[dir] * 64, z);
			else if (i == 1)
				to.set(x + x8add[dir] * 32, y + y8add[dir] * 32, z);
			else if (i == 2)
				to.set(x + x8add[dir] * 32, y + y8add[dir] * 32, z);
			else if (i == 3)
				to.set(x + x8add[dir] * 16, y + y8add[dir] * 16, z);
			else
				to.set(x, y, z);
			from = to;
			climb = new Climb(npc.referent, 5, 8, &to);
			climb->then(waiter);
			climb->process();
			if (!waiter->dependencyFailed())
			{
				ItemSupport support;
				support.figureInfo(npc.getType(), WorldPoint(to.x, to.y, to.z), 1, npc.referent);
				if (support.onlyEast() || support.onlyWest() || support.onlySouth() || support.onlyNorth())
				{
					npc.move(from.x, from.y, from.z);
					grabbed = 1;
					Kernel::resetRef(npc.referent, (ProcessType)0xf0);
					caught = 1;
					pop(0);
					npc.doAnim(ANIM_HANG, 8, 0, 0);
					npc.setLastAnimSet(ANIM_HANG);
					break;
				}
			}
		}
		waiter->pop(0);
	}
	return caught;
}

void GravityTrackerCling::afterMove(uint16_t hit)
{
	Item item(ref);
	if (item.isAvatar())
		hitItem = hit;
}

int16_t Item::hurl(int16_t x, int16_t y, int16_t z, int16_t gravity)
{
	GravityTracker *tracker = (GravityTracker *)Kernel::findValidProcess(referent, (ProcessType)0x203);
	if (tracker)
		tracker->bump(x, y, z);
	else if (isAvatar())
		tracker = new GravityTrackerCling(referent, x, y, z, gravity);
	else
		tracker = new GravityTracker(referent, x, y, z, gravity, 200, 0);
	return tracker->pid;
}

void Item::fall(void)
{
	if (!(getStatus() & 0x1000))
	{
		TypeFlag flag = GlobalTypes.typeFlags[getType()];
		if (!flag.fixed && flag.weight)
			hurl(0, 0, 0, 4);
	}
}
