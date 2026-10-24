// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: BARK.C

#include "u8port.h"

// #include <conio.h>  (DOS only)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "newgump.h"
#include "cfile.h"
#include "crect.h"
#include "cvport.h"
#include "ccurrvp.h"
#include "cglobvp.h"
#include "dispatch.h"
#include "error.h"
#include "format.h"
#include "shaphand.h"
#include "camera.h"
#include "system.h"
#include "yamm.h"
#include "targgump.h"
#include "world.h"
#include "wait.h"
#include "npc.h"
#include "barkfidg.h"
#include "bark.h"

// Inline in the shared headers when this file was built.
inline Point::Point(void) {}
inline void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
inline Rect::Rect(void) {}
inline void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
inline Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }
inline int16_t Rect::width(void) { return f_04 - f_00 + 1; }
inline int16_t Rect::height(void) { return f_06 - f_02 + 1; }
inline NewGumpId::NewGumpId(uint16_t i) { instance = i; f_02 = ~instance; f_04 = 0; }
inline uint8_t NewGumpId::getInstance(void) { return instance & 0xff; }
inline void NewGump::moveto(int16_t x, int16_t y) { rect.moveto(x, y); }
inline CacheFont::~CacheFont(void) {}
inline GraphicYtable::GraphicYtable(void) { f_04 = 2; index = 0; }
inline Vport::Vport(void) { seg = 0; f_0f = 2; f_10 = 0; }
inline Vport::~Vport(void) { free(); }
inline SimpleVirtualString::operator char *(void) { return string; }
inline YammList::YammList(uint16_t l, uint16_t s, int16_t str) { list = l; size = s; isString = str; }

int16_t BarkGump::numBarks = 0;
char StringHeader[3] = " @";
Process *barkTest = 0;
Rect BarkGump::rects[30];

void DorkProcess::process(void)
{
	int16_t count = 0;
	int16_t i;

	if (BarkGump::numBarks)
	{
		for (i = 0; i < 30; i++)
		{
			if (BarkGump::rects[i].f_00 || BarkGump::rects[i].f_02 || BarkGump::rects[i].f_04 || BarkGump::rects[i].f_06)
			{
				DrawRectangle(GlobalVport::global_ptr, BarkGump::rects[i].f_00, BarkGump::rects[i].f_02,
					BarkGump::rects[i].f_04, BarkGump::rects[i].f_06, 15);
				count++;
			}
		}
	}
	gotoxy(1, 1);
	printf("%d", BarkGump::numBarks);
	gotoxy(1, 2);
	printf("%d", count);
}

void BarkTest(void)
{
	if (barkTest)
	{
		if (Kernel::isProcess(barkTest))
			barkTest->pop(0);
		barkTest = 0;
	}
	else
		barkTest = new DorkProcess;
}

BarkGump::BarkGump(NewGump *parent, uint16_t referent, char *message, uint8_t exclusive, uint16_t delay) :
	ItemRelativeGump((IRGumpType)2, parent, Rect(0, 0, 0, 0), referent, (IRGumpMove)1, 0, 0, delay)
{
	if (exclusive)
		f_1d |= 2;
	text = 0;
	fontType = (FontType)0;
	if (Item(referent).isAvatar())
		fontType = (FontType)6;
	else if (referent >= 256)
		fontType = (FontType)8;
	else
	{
		switch (referent % 3)
		{
		case 1:
			fontType = (FontType)5;
			break;
		case 2:
			fontType = (FontType)7;
			break;
		}
	}
	CacheFont font(fontType);
	rect = Rect(0, 0, 192, font.height((char)0) * 5);
	offset = -1;
	if (message)
	{
		message = printFormat(message, fontType, &rect, 1);
		textLength = strlen(message) + 1;
		text = new char[textLength];
		if (!text)
			outOfMemory(__FILE__, 164);	// __LINE__
		strcpy(text, message);
	}
	Item item(referent);
	int16_t width;
	int16_t height;
	int16_t xOffset;
	int16_t yOffset;
	shapeHandler->getDim(item.getType(), item.getFrame(), width, height, xOffset, yOffset);
	item.getScreenXY((uint16_t &)screenX, (uint16_t &)screenY);
	itemRect = Rect(screenX - xOffset, screenY - yOffset, screenX - xOffset + width, screenY - yOffset + height);
	screenY -= height;
	f_60 = 0;
	rectIndex = -1;
	numBarks++;
	this->parent->newGumpList.moveToTail(this);
	if (message)
	{
		findPlace();
		nextPage();
		if (item.isNpc())
		{
			Npc npc(referent);
			if (!Kernel::isProcessTypeActive(referent, PT_FIRST) &&
				(npc.getLastAnimSet() == ANIM_STAND || npc.getLastAnimSet() == ANIM_TALK) &&
				!Kernel::isProcessTypeActive(referent, (ProcessType)0x227))
			{
				World *world = (World *)Dispatcher::base[0];
				if (!(world->flags & 4) && !npc.isDead())
					new BarkFidget(referent, newGumpId.getInstance());
			}
		}
	}
}

BarkGump::~BarkGump(void)
{
	if (text)
		delete text;
	removeRect();
	numBarks--;
}

// Tries every place on screen and keeps the free one nearest the item.
void BarkGump::findPlace(void)
{
	int16_t width = rect.width();
	int16_t height = rect.height();
	int16_t maxX = 320 - width;
	int16_t maxY = 200 - height;
	int16_t bestX = 0;
	int16_t bestY = 0;
	uint16_t best = 0xffff;
	Boolean free = 1;
	int16_t x;

	for (x = 0; x < maxX; x += 8)
	{
		for (int16_t y = 0; y < maxY; y += 8)
		{
			uint16_t score = abs(screenX - (x + (width >> 1)));
			score += abs(screenY - (y + (height >> 1)));
			rect.moveto(x, y);
			Rect *r = rects;
			for (int16_t i = 0; i < numBarks; i++, r++)
			{
				if (rect.intersects(*r))
				{
					score += 3000;
					free = 0;
				}
			}
			if (rect.intersects(itemRect))
				score += 500;
			if (score < best && free)
			{
				best = score;
				bestX = x;
				bestY = y;
			}
			free = 1;
		}
	}
	rect.moveto(bestX, bestY);
	rectIndex = -1;
	for (int16_t i = 0; i < 30; i++)
	{
		if (!rects[i].f_00 && !rects[i].f_02 && !rects[i].f_04 && !rects[i].f_06)
		{
			rectIndex = i;
			rects[rectIndex] = rect;
			break;
		}
	}
	if (rectIndex == -1)
		halt(__FILE__, 305);	// __LINE__
	if (rectIndex < 0)
		halt(__FILE__, 312);	// __LINE__
	int16_t itemX = Item(item).getX();
	int16_t itemY = Item(item).getY();
	int16_t sx;
	int16_t sy;
	WorldToScreenCoords(itemX, itemY, sx, sy);
	sy -= Item(item).getZ();
	dx = rect.f_00 - sx;
	dy = rect.f_02 - sy;
}

void BarkGump::commandPrivate(Event &)
{
	nextPage();
}

void BarkGump::removeRect(void)
{
	if (rectIndex == -1)
		return;
	rects[rectIndex].f_00 = 0;
	rects[rectIndex].f_02 = 0;
	rects[rectIndex].f_04 = 0;
	rects[rectIndex].f_06 = 0;
	rectIndex = -1;
}

void BarkGump::killMyself(void)
{
	nextPage();
}

void BarkGump::nextPage(void)
{
	if (!text)
		return;
	if (offset == -1)
		offset = 0;
	else
	{
		offset += strlen(text + offset) + 1;
		if (offset >= textLength)
		{
			ItemRelativeGump::killMyself();
			return;
		}
	}
	if (f_4e)
	{
		Wait *wait = (Wait *)Kernel::getProcess(f_4e);
		wait->restart();
	}
	char *p = text + offset;
	while (*p++)
	{
		if (*p == -1 || *p == 0)
		{
			*p = 0;
			break;
		}
	}
	pushVisibility(3);
	restore();
	visibilityStack >>= 2;
	CacheFont font(fontType);
	font.getRect(rect, 30, 30, text + offset);
	removeRect();
	findPlace();
	refresh();
}

void BarkGump::move(MovedState state)
{
	ItemRelativeGump::move(state);
	rects[rectIndex] = rect;
}

void BarkGump::commandMouseLeft(Event &event)
{
	if (!event.isRelease())
	{
		if (f_4e)
		{
			Wait *wait = (Wait *)Kernel::getProcess(f_4e);
			uint16_t count = wait->count;
			if (delay * 30 - 12 < count)
				return;
		}
		nextPage();
		return;
	}
	event.to = parent;
	parent->commandMouseLeft(event);
}

void BarkGump::draw(int16_t x, int16_t y)
{
	CacheFont font(fontType);
	Vport vport;
	vport = *GlobalVport::global_ptr;
	vport.rect = rect;
	vport.rect.clip(GlobalVport::global_ptr->rect);
	CurrentVport current(&vport);
	font.printf(x, y + font.getBaseLine(), text + offset);
}

// Closes the barks of the game being left.
void BarkGump::load(BaseFile *file)
{
	BarkGump *bark = 0;
	NewGump *top = Dispatcher::base[0];
	while (top->newGumpList.hasType(0x43, (NewGump *&)bark))
	{
		if (bark->f_52 == 2)
		{
			bark->f_4e = 0;
			bark->killMyself();
			bark = 0;
		}
	}
	if (file)
		;
}

void BarkGump::save(BaseFile *)
{
}

InputElementGump::InputElementGump(NewGump *parent, char *message, Rect *previous) :
	NewGump(NewGumpId(0), parent, 0)
{
	f_1d |= 2;
	text = new char[strlen(message) + 1];
	if (!text)
		outOfMemory(__FILE__, 527);	// __LINE__
	strcpy(text, message);
	rect.moveto(0, 0);
	CacheFont font((FontType)0);
	int16_t indent = font.Font::width(StringHeader) + 2;
	while (*message == ' ')
		message++;
	font.Font::dim(message, rect.f_04, rect.f_06);
	rect.f_04 += indent;
	if (previous)
		moveto(previous->f_04, previous->f_02);
	if (this->parent->rect.width() < rect.f_04)
	{
		moveto(0, previous->f_06);
		if (this->parent->rect.height() < rect.f_06)
			traceGet((TraceLevel)50, 1, 1, "Keyword Overflow!");
	}
}

InputElementGump::~InputElementGump(void)
{
	if (text)
	{
		delete text;
		text = 0;
	}
}

void InputElementGump::draw(int16_t x, int16_t y)
{
	CacheFont font((FontType)6);
	Vport vport;
	vport = *GlobalVport::global_ptr;
	vport.rect.set(parent->rect.f_00, parent->rect.f_02, parent->rect.f_04, parent->rect.f_06);
	vport.rect.clip(GlobalVport::global_ptr->rect);
	CurrentVport current(&vport);
	font.printf(x, y + font.getBaseLine(), StringHeader);
	int16_t indent = font.Font::width(StringHeader) + 2;
	char *s = text;
	while (*s == ' ')
		s++;
	font.printf(x + indent, y + font.getBaseLine(), s);
}

void InputElementGump::commandMouseLeft(Event &event)
{
	if (!event.isRelease())
	{
		uint16_t keyword = Yamm::flimFlam(text);
		if (!keyword)
			halt(__FILE__, 594);	// __LINE__
		send(Event(EVENT_PRIVATE, 0, 0, keyword, this, parent, 0));
	}
}

InputGump::InputGump(uint16_t npc, uint16_t answers) :
	BarkGump(Dispatcher::base[Dispatcher::baseSP], 1, 0, 0, 30)
{
	f_1d |= 0x10;
	Rect *last = 0;
	Process *notifier = Kernel::getProcess(f_4e);
	notifier->result = 0;
	YammList list(answers, 2, 1);
	CacheFont font((FontType)6);
	uint16_t index = 0;
	char *s;
	int16_t indent = font.Font::width(StringHeader) + 2;
	uint16_t widest = 192;
	while (list.traverse(index))
	{
		s = list.resolveString(index);
		while (*s == ' ')
			s++;
		uint16_t w = font.Font::width(s) + indent;
		if (w > widest)
			widest = w;
	}
	rect = Rect(0, 0, widest, font.height((char)0) * 5);
	index = 0;
	while (list.traverse(index))
	{
		s = list.resolveString(index);
		*(char *)WorkString = 0;
		strcat(WorkString, s);
		last = &(new InputElementGump(this, WorkString, last))->rect;
	}
	if (last)
	{
		rect.f_06 = last->f_06;
		findPlace();
		refresh();
	}
	else
	{
		new BarkGump(Dispatcher::base[Dispatcher::baseSP], npc, "NULL INPUT GUMP!", 1, 100);
		send(Event(EVENT_PRIVATE, 0, 0, 0, this, this, 0));
	}
}

void InputGump::commandPrivate(Event &event)
{
	Process *notifier = Kernel::getProcess(f_4e);
	int32_t answer = event.data;
	notifier->result = answer;
	ItemRelativeGump::killMyself();
}

void InputGump::draw(int16_t, int16_t)
{
	if (!newGumpList.tail)
		traceGet((TraceLevel)50, 1, 1, "No Keywords!!");
}

void InputGump::killMyself(void)
{
	ItemRelativeGump::killMyself();
}

uint16_t ask(uint16_t npc, uint16_t answers)
{
	InputGump *gump = new InputGump(npc, answers);
	return gump->f_4e;
}

// Debug key: shows the numbers of a picked item.
void f6key(void)
{
	Item item;
	Dispatch(new TargetGump(&item, 0));
	trace((TraceLevel)50, 1, 1, "referent = %d (0x%x)", item.referent, item.referent);
	trace((TraceLevel)50, 1, 2, "type     = %d (0x%x)", item.getType(), item.getType());
	trace((TraceLevel)50, 1, 3, "frame    = %d (0x%x)", item.getFrame(), item.getFrame());
	trace((TraceLevel)50, 1, 4, "q        = %d (0x%x)", item.getQ(), item.getQ());
	trace((TraceLevel)50, 1, 4, "x        = %d (0x%x)", item.getX(), item.getX());
	trace((TraceLevel)50, 1, 5, "y        = %d (0x%x)", item.getY(), item.getY());
	trace((TraceLevel)50, 1, 6, "z        = %d (0x%x)", item.getZ(), item.getZ());
}
