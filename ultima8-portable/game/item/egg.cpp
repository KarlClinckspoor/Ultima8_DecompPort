// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\ITEM\EGG.C

#include "u8port.h"

#include "kernel.h"
#include "npc.h"
#include "type.h"
#include "itemdata.h"
#include "itemfind.h"
#include "scrit.h"
#include "itemcach.h"
#include "camera.h"
#include "gravity.h"
#include "pal.h"
#include "world.h"
#include "uprocess.h"
#include "sndmgr.h"
#include "dsfxman.h"
#include "cprotmem.h"
#include "error.h"
#include "init.h"
#include "main.h"
#include "egg.h"

#define TRACE_EGG	((TraceLevel)50)

uint8_t EggHatcher::eggOutlineVisible = 0;
static char outlineColor = 0;
int16_t TeleporterProcess::dontFire = 0;

SuspendAvatar::SuspendAvatar(void)
{
	wasInStasis = avatar.inStasis;
	avatar.inStasis = 1;
}

void SuspendAvatar::process(void)
{
	if (!wasInStasis)
		avatar.inStasis = 0;
	pop(0);
}

TeleporterProcess::TeleporterProcess(uint16_t egg)
{
	Kernel::setIdString(pid, "Teleporter");
	Egg &e = (Egg &)Item(egg);
	destMap = e.getEggId();
	destEgg = e.getQLo();
	uint8_t fire = e.getQHi();
	go(fire);
}

TeleporterProcess::TeleporterProcess(int16_t map, int16_t egg, uint8_t fire)
{
	destMap = map;
	destEgg = egg;
	go(fire);
	if (soundOn && SoundManager && SpkrMgr)
	{
		SpkrMgr->flush();
		SoundManager->flush();
	}
	ProtMemoryManager::garbageCollect(0);
}

void TeleporterProcess::go(uint8_t fire)
{
	Kernel::resetRef(avatar.referent, (ProcessType)0xf0);
	if (fire && !dontFire)
	{
		FadeProcess *fadeOut = new FadeProcess(RGB(0, 0, 0), 0x7fff, 1);
		FadeProcess *fadeIn = new FadeProcess(PALETTE_CURRENT, 0x7fff, 1);
		SuspendAvatar *suspend = new SuspendAvatar;
		fadeOut->then(this)->then(fadeIn)->then(suspend);
	}
}

void TeleporterProcess::process(void)
{
	if (dontFire)
	{
		pop(0);
		return;
	}
	int16_t x = avatar.getX();
	int16_t y = avatar.getY();
	uint8_t z = avatar.getZ();
	int16_t map = avatar.getMap();
	uint8_t sameMap = 1;

	if (destMap != ItemCache::mapIn)
	{
		sameMap = 0;
		if (inGameMode)
			avatar.teleport(0, 0, 0, destMap);
		else
			Camera::move_to(0, 0, 0, destMap);
	}
	AreaItemFinder finder(0, 0, 0xffff, 0xffff,
		SearchCriteria(':', '%', 8, '=', '`', '%', 1, '=', '&', '*', '%', 0xff, '&', '%', destEgg, '=', '&', '$'),
		0xffff);
	if (finder.isValid())
	{
		if (inGameMode)
			avatar.teleport(finder.getX(), finder.getY(), finder.getZ(), destMap);
		else
			Camera::move_to(finder.getX(), finder.getY(), finder.getZ(), destMap);
		ItemSupport support;
		WorldPoint dest(finder.getX(), finder.getY(), finder.getZ());
		support.figureInfo(1, dest, 1, avatar.referent);
		if (!support.isSupported())
			avatar.fall();
	}
	else
	{
		if (!sameMap)
		{
			dontFire = 1;
			if (inGameMode)
				avatar.teleport(x, y, z, map);
			else
				Camera::move_to(x, y, z, map);
			dontFire = 0;
		}
		trace(TRACE_EGG, "No destination for this egg!");
	}
	pop(0);
}

int16_t teleportToEgg(int16_t map, int16_t egg, uint8_t fire)
{
	TeleporterProcess *teleporter = new TeleporterProcess(map, egg, fire);
	return teleporter->pid;
}

// An egg's hatch range is kept in its npcNum byte (x high, y low nibble), in 32 unit steps.
int16_t Egg::getEggXRange(void)
{
	return ItemData::npcNumArray[referent] >> 4;
}

int16_t Egg::getEggYRange(void)
{
	return ItemData::npcNumArray[referent] & 0xf;
}

void Egg::setEggXRange(uint16_t range)
{
	ItemData::npcNumArray[referent] &= 0x0f;
	ItemData::npcNumArray[referent] |= range << 4;
}

void Egg::setEggYRange(uint16_t range)
{
	ItemData::npcNumArray[referent] &= 0xf0;
	ItemData::npcNumArray[referent] |= range & 0xf;
}

int16_t Egg::getEggId(void)
{
	return ItemData::mapNumArray[referent];
}

void Egg::setEggId(uint16_t id)
{
	ItemData::mapNumArray[referent] = id;
}

// Returns the pid of the process the egg started, or 0.
int16_t Egg::hatch(void)
{
	TeleporterProcess *teleporter;
	uint16_t pid;

	if (getFamily() == TELEPORTEGG_FAMILY)
	{
		teleporter = new TeleporterProcess(referent);
		return teleporter->pid;
	}
	if (spawnUnk(this, 0x80, 7, pid, 0))
		return pid;
	return 0;
}

EggHatcher::EggHatcher(uint16_t egg)
{
	Egg e(egg);
	setRef(e.referent);
	setProcessType((ProcessType)0x20f);
	hatched = 0;
	Kernel::setIdString(pid, "EggHatcher");
}

void EggHatcher::process(void)
{
	Egg egg(ref);
	uint16_t x1, y1, x2, y2;
	uint16_t avatarX, avatarY;
	int16_t avatarZ;
	int16_t xd, yd, zd;
	Npc av(avatar.referent);
	TypeFlag &type = GlobalTypes.typeFlags[ItemData::typeArray[av.referent]];

	type.getWorldSize(xd, yd, zd);
	avatarX = ItemData::xArray[av.referent];
	avatarY = ItemData::yArray[av.referent];
	avatarZ = ItemData::zArray[av.referent];
	int16_t eggZ = egg.getZ();
	x1 = x2 = egg.getX();
	y1 = y2 = egg.getY();
	x1 -= egg.getEggXRange() * 32;
	y1 -= egg.getEggYRange() * 32;
	x2 += egg.getEggXRange() * 32;
	y2 += egg.getEggYRange() * 32;
	uint8_t inside = avatarX >= x1 && avatarX - xd + 1 <= x2 && avatarY >= y1 && avatarY - yd + 1 <= y2;
	if (inside && (eggZ + 48 < avatarZ || eggZ - 48 > avatarZ))
		inside = 0;
	if (eggOutlineVisible && ((!hatched && inGameMode) || !inGameMode))
		drawDiamond(egg.getX() + egg.getEggXRange() * 32, egg.getY() + egg.getEggYRange() * 32, eggZ,
		            egg.getEggXRange() * 2, egg.getEggYRange() * 2, outlineColor++);
	if (hatched)
	{
		if (!inside)
		{
			hatched = 0;
			return;
		}
	}
	else if (inside && inGameMode)
	{
		egg.hatch();
		hatched = 1;
	}
}
