// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: IRGUMP.C

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "newgump.h"
#include "crect.h"
#include "dispatch.h"
#include "kernel.h"
#include "item.h"
#include "type.h"
#include "npc.h"
#include "camera.h"
#include "cfile.h"
#include "filespec.h"
#include "fexist.h"
#include "scratchm.h"
#include "cexit.h"
#include "bark.h"
#include "contgump.h"
#include "menu.h"
#include "irgump.h"

// Inline in the shared headers when this file was built.
inline NewGumpId::NewGumpId(uint16_t i) { instance = i; f_02 = ~instance; f_04 = 0; }
inline NewGumpId::NewGumpId(uint8_t type, uint8_t id) { instance = (type << 8) | id; f_02 = ~instance; f_04 = 0; }
inline uint8_t NewGumpId::getType(void) { return instance >> 8; }
inline uint8_t NewGumpId::getInstance(void) { return instance & 0xff; }
inline void NewGump::moveto(int16_t x, int16_t y) { rect.moveto(x, y); }
inline void NewGump::popVisibility(void) { visibilityStack >>= 2; }
inline uint8_t BaseFile::is_valid(void) { return handle != -1; }
inline BaseFile::~BaseFile(void) { if (is_valid()) close(); }

GumpNotifier::GumpNotifier(uint16_t seconds, NewGump *gump, uint16_t ref, ProcessType type, uint8_t flag) :
	Wait(seconds * 30, 0)
{
	setRef(ref);
	setProcessType(type);
	f_10 &= ~1;
	f_70 = flag;
	int16_t i = -1;
	NewGump *g;
	for (g = gump; g; g = g->parent)
		i++;
	depth = i;
	for (g = gump; i >= 0; g = g->parent)
		path[i--] = g->newGumpId;
	active = 1;
}

GumpNotifier::~GumpNotifier(void)
{
	if (active)
	{
		ItemRelativeGump *gump = (ItemRelativeGump *)dispatcher->findGump(path, depth);
		if (gump)
		{
			gump->f_4e = 0;
			gump->killMyself();
		}
	}
}

void GumpNotifier::timerDone(void)
{
	NewGump *gump = dispatcher->findGump(path, depth);
	if (gump)
	{
		active = 0;
		gump->killMyself();
	}
}

void GumpNotifier::restart(void)
{
	active = 1;
	Wait::restart();
}

ItemRelativeGump::ItemRelativeGump(IRGumpType type, NewGump *parent, Rect r, uint16_t referent, IRGumpMove move, int16_t x, int16_t y, uint16_t seconds) :
	PanelGump(NewGumpId(0), parent, r, GumpColorMap[5])
{
	uint8_t id;
	NewGump *gump;
	char used[256];
	memset(used, 0, sizeof(used));
	gump = 0;
	if (parent)
		while (parent->newGumpList.hasType(0x43, gump))
			used[gump->newGumpId.getInstance()] = 1;
	for (int16_t i = 0; i < 256; i++)
		if (!used[i])
		{
			id = i;
			break;
		}
	if (i == 256)
		halt(__FILE__, 170);	// __LINE__ in the original file
	newGumpId = NewGumpId(0x43, id);
	f_52 = type;
	Item(referent).orStatus(0x8000);
	item = referent;
	dx = x;
	dy = y;
	moveType = move;
	switch (moveType)
	{
	case 0:
	case 1:
		loc.x = Item(item).getX();
		loc.y = Item(item).getY();
		loc.z = Item(item).getZ();
		break;
	case 2:
		loc.x = avatar.getX();
		loc.y = avatar.getY();
		loc.z = avatar.getZ();
		break;
	case 3:
		loc.x = Camera::getX();
		loc.y = Camera::getY();
		loc.z = Camera::getZ();
		dy += loc.z;
	}
	delay = seconds;
	if (seconds)
	{
		GumpNotifier *notifier = new GumpNotifier(seconds, this, item, (ProcessType)0x200, 0);
		notifier->setRef(item);
		notifier->start();
		f_4e = notifier->pid;
	}
	else
		f_4e = 0;
}

ItemRelativeGump::~ItemRelativeGump(void)
{
	ItemRelativeGump *gump = 0;
	if (parent)
		while (parent->newGumpList.hasType(0x43, (NewGump *&)gump))
			if (gump->item == item)
				return;
	Item(item).andStatus(0x7fff);
}

void ItemRelativeGump::killMyself(void)
{
	if (f_4e)
	{
		GumpNotifier *notifier = (GumpNotifier *)Kernel::getProcess(f_4e);
		if ((Boolean)(notifier != 0))
		{
			notifier->active = 0;
			notifier->pop(notifier->result);
		}
		f_4e = 0;
	}
	NewGump::killMyself();
}

void ItemRelativeGump::notifyMoved(uint16_t referent, MovedState state)
{
	ItemRelativeGump *gump = 0;
	NewGump *base = Dispatcher::base[Dispatcher::baseSP];
	while (base->newGumpList.hasType(0x43, (NewGump *&)gump))
		if (!referent || gump->item == referent)
			gump->move(state);
	Item container(referent);
	if (container.isValid())
	{
		Item child = container.getContents();
		while (child.isValid())
		{
			if (child.getStatus() & 0x8000)
			{
				notifyMoved(child.referent, (MovedState)0);
				notifyMoved(child.referent, (MovedState)1);
			}
			child = child.getNext();
		}
	}
}

// Brings gumps of type 2 to the top, keeping their order.
void ItemRelativeGump::sort(void)
{
	NewGump *prev = 0;
	NewGump *gump = 0;
	NewGump *base = Dispatcher::base[Dispatcher::baseSP];
	while (base->newGumpList.traverseBackward((DoubleLink *&)gump))
		if (gump->newGumpId.getType() == 0x43 && ((ItemRelativeGump *)gump)->f_52 == 2)
		{
			prev = (NewGump *)gump->prev;
			base->newGumpList.moveToTail(gump);
			if (!prev)
				break;
			gump = (NewGump *)prev->next;
		}
}

void ItemRelativeGump::notifyDestroyed(uint16_t referent)
{
	ItemRelativeGump *gump = 0;
	NewGump *base = Dispatcher::base[Dispatcher::baseSP];
	while (base->newGumpList.hasType(0x43, (NewGump *&)gump))
		if (gump->item == referent)
		{
			gump->killMyself();
			gump = 0;
		}
}

void ItemRelativeGump::move(MovedState state)
{
	Item it(item);
	if (state == 0)
	{
		pushVisibility(3);
		restore();
		return;
	}
	switch (moveType)
	{
	case 1:
		loc.x = Item(item).getX();
		loc.y = Item(item).getY();
		loc.z = Item(item).getZ();
		break;
	case 2:
		loc.x = avatar.getX();
		loc.y = avatar.getY();
		loc.z = avatar.getZ();
		break;
	case 3:
		popVisibility();
		refresh();
		return;
	}
	{
		int16_t x = 0;
		int16_t y = 0;
		WorldToScreenCoords(loc.x, loc.y, x, y);
		y -= loc.z;
		moveto(x + dx, y + dy);
	}
	popVisibility();
	refresh();
}

void ItemRelativeGump::commandMouseBoth(Event &event)
{
	if (event.isRelease())
		parent->commandMouseBoth(event);
}

void ItemRelativeGump::commandMouseRight(Event &event)
{
	if (event.isRelease())
		parent->commandMouseBoth(event);
}

char *GumpSaveFile = "gumps.dat";

void loadGumps(void)
{
	if (!FileExists(fileSpec(0, GamedatDir, GumpSaveFile, 0)))
	{
		BarkGump::load(0);
		ContainerGump::load(0);
		return;
	}
	BaseFile file;
	file.open(fileSpec(0, GamedatDir, GumpSaveFile, 0), (OpenMode)0);
	BarkGump::load(&file);
	ContainerGump::load(&file);
	file.close();
}

void saveGumps(void)
{
	BaseFile file;
	file.forceOpen(fileSpec(0, GamedatDir, GumpSaveFile, 0), (OpenMode)2);
	BarkGump::save(&file);
	ContainerGump::save(&file);
	file.close();
}
