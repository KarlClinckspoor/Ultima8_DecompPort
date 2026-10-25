// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -vi-
// name: DEBUGUMP.C

#include "u8port.h"
#include "farptr.h"

// #include <alloc.h>  (DOS only)
// #include <conio.h>  (DOS only)
#include <ctype.h>
// #include <dos.h>  (DOS only)
#include <stdio.h>
#include <string.h>
#include "newgump.h"
#include "basecam.h"
#include "cdesc.h"
#include "crect.h"
#include "ctracker.h"
#include "debugger.h"
#include "dispatch.h"
#include "error.h"
#include "filespec.h"
#include "flag.h"
#include "inter.h"
#include "system.h"
#include "textgump.h"
#include "world.h"
#include "yamm.h"
#include "debugump.h"

// The shared headers declare these; each file that uses them defines them.
U8_INLINE FileSpec::FileSpec(char *path, int16_t attrib) { become(path, attrib); }
U8_INLINE NewGumpId::NewGumpId(uint16_t i) { instance = i; f_02 = ~instance; f_04 = 0; }
U8_INLINE void Point::set(int16_t x, int16_t y) { f_00 = x; f_02 = y; }
U8_INLINE void Rect::set(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { Point::set(x1, y1); f_04 = x2; f_06 = y2; }
U8_INLINE Rect::Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2) { set(x1, y1, x2, y2); }

// Menu and hot key commands.
#define CMD_LOAD_UNIT		0
#define CMD_VIEW_FILE		1
#define CMD_RUN			2
#define CMD_RUN_TO_CURSOR	3
#define CMD_TRACE_INTO		4
#define CMD_STEP_OVER		5
#define CMD_RUN_TO_RETURN	6
#define CMD_BREAKPOINT		7
#define CMD_SEARCH_AGAIN	9
#define CMD_GOTO_LINE		10
#define CMD_WATCH		11
#define CMD_INSPECT		12
#define CMD_CLEAR_WATCH		13
#define CMD_CHANGE_FLAG		14
#define CMD_FIND		15
#define CMD_BREAK		16

#define NUM_WATCHES		10

U8_INLINE void WindowGump::setColor(char c) { color = c; }
U8_INLINE void *Yamm::resolve(uint16_t offset) { return (char *)buffer + offset; }
U8_INLINE YammList::YammList(uint16_t id, uint16_t elementSize, int16_t strings)
{
	list = id;
	size = elementSize;
	isString = strings;
}

extern char UsecodeDir[];

uint8_t DebugGump::init = 0;

// Symbol type codes used in a routine's symbol table.
static struct
{
	char type;
	char *name;
} symbolTypes[9] = {
	{0, "STT_ERROR"},
	{'$', "STT_STRUCTURE"},
	{'b', "STT_BYTE"},
	{'i', "STT_INTEGER"},
	{'d', "STT_LONG"},
	{'p', "STT_POINTER"},
	{'s', "STT_STRING"},
	{'%', "STT_NAMED_TYPE"},
	{'l', "STT_LIST"}
};

char *DebugGump::watches[NUM_WATCHES];
uint8_t DebugGump::fromInterpreter;
ViewFile *DebugScroller::viewFile;

void initDebugger(void)
{
}

void SourceDebugger::atBreakPoint(void)
{
	DebugGump *gump = new DebugGump(NewGumpId(0), 0, Rect(0, 0, 79, 49), this, 1);
	if (stricmp(debugger->currentFile(), gump->scroller->getSourceFile()))
	{
		gump->scroller->setFile(debugger->currentFile());
		gump->scroller->cursorY = debugger->currentLine - 1;
	}
	((World *)dispatcher->getBase())->flags = 0;
	Dispatch(gump);
}

void SourceDebugger::takeControl(void)
{
	Dispatch(new DebugGump(NewGumpId(0), 0, Rect(0, 0, 79, 49), this, 0));
}

DebugScroller::DebugScroller(NewGumpId id, NewGump *parent, Rect r, Debugger *d, WindowFrame *frame) :
	Scroller(id, parent, r, 0x1e, frame)
{
	SetArea(__FILE__, 140);	// __LINE__
	cursorX = 4;
	cursorY = 0;
	registerHotKeys(0x13c, 0x148, 0x149, 0x150, 0x151, 0x14d, 0x14b, 0x147, 0x14f, 0);
	owner = d;
	maxX = 0x100;
	maxY = viewFile->numLines;
	cursorGotoOrigin();
}

void DebugScroller::cursorGotoOrigin(void)
{
	cursorX = 4;
	cursorY = owner->currentLine - 1;
	trackCursor();
}

void DebugScroller::scrollTo(int16_t line)
{
	cursorX = 0;
	cursorY = line;
	trackCursor();
	refresh();
}

void DebugScroller::commandKeyboard(Event &event)
{
	SetArea(__FILE__, 177);	// __LINE__
	int16_t height = rect.height();
	int16_t width = rect.width();
	int16_t oldX = scrollX;
	int16_t oldY = scrollY;
	switch (event.data)
	{
	case 0x13c:	// F2
		if (owner->isBreakPointAt(getSourceFile(), cursorY))
			owner->removeBreakPoint(getSourceFile(), cursorY);
		else
			owner->addBreakPoint(getSourceFile(), cursorY);
		refresh();
		break;
	case 0x148:	// up
		if (cursorY > 0)
			cursorY--;
		break;
	case 0x149:	// page up
		cursorY -= height;
		scrollY -= height;
		break;
	case 0x150:	// down
		cursorY++;
		break;
	case 0x151:	// page down
		cursorY += height;
		scrollY += height;
		break;
	case 0x14d:	// right
		cursorX++;
		break;
	case 0x14b:	// left
		cursorX--;
		break;
	case 0x147:	// home
		cursorX = 0;
		break;
	case 0x14f:	// end
		cursorX = parseLine(WorkString, viewFile->getLine(cursorY), 80) + 4;
		break;
	}
	trackCursor();
	if (scrollX != oldX || scrollY != oldY)
		refresh();
}

// Copies a source line with tabs expanded; returns its length.
int16_t DebugScroller::parseLine(char *dest, char *src, int16_t length)
{
	char *start = dest;
	memset(dest, ' ', length - 1);
	int16_t column = 0;
	while (*src && column < length)
	{
		if (*src == '\t')
		{
			dest += 4;
			column += 4;
			src++;
		}
		else
		{
			*dest++ = *src++;
			column++;
		}
	}
	start[length - 1] = 0;
	return dest - start;
}

void DebugScroller::draw(int16_t, int16_t)
{
	char marker;
	int16_t height;
	int16_t numLines;
	int16_t current;
	int16_t brk;
	int16_t brkLine;
	int16_t row;
	int16_t line;
	char text[80];

	SetArea(__FILE__, 291);	// __LINE__
	memset(text, ' ', sizeof text);
	text[sizeof text - 1] = 0;
	height = rect.height();
	numLines = viewFile->numLines;
	current = owner->currentLine;
	brk = owner->getNextBreak(getSourceFile(), scrollY);
	if (brk != -1)
		brkLine = owner->breaks[brk].line;
	else
		brkLine = -1;
	line = scrollY;
	for (row = 0; row < height; row++)
	{
		gotoxy(0, row);
		if (line < numLines)
		{
			parseLine(text, viewFile->getLine(line), rect.f_04);
			if (current - 1 == line)
				marker = 0x10;
			else
				marker = ' ';
			if (line == brkLine)
			{
				setColor(0x4f);
				if (owner->numBreaks - 1 > brk)
					brkLine = owner->breaks[++brk].line;
			}
			else
				setColor(0x1e);
		}
		else
		{
			marker = ' ';
			memset(text, ' ', sizeof text);
			text[sizeof text - 1] = 0;
		}
		print(" %c  %s", marker, text);
		line++;
	}
	trackCursor();
}

// Keeps the cursor inside the file and scrolls to keep it in view.
void DebugScroller::trackCursor(void)
{
	int16_t height = rect.height();
	int16_t width = rect.width();
	if (cursorY < 0)
		cursorY = 0;
	if (cursorY > maxY)
		cursorY = maxY;
	if (cursorX < 4)
		cursorX = 4;
	if (cursorX > maxX)
		cursorX = maxX;
	if (scrollY < 0)
		scrollY = 0;
	if (scrollY > maxY)
		scrollY = maxY;
	if (cursorY < scrollY)
		scrollY = cursorY;
	if (cursorY >= scrollY + height)
		scrollY = cursorY - height + 15;
	if (cursorX < scrollX)
		scrollX = cursorX;
	if (cursorX >= scrollX + width)
		scrollX = cursorX - width + 1;
	int16_t x = get_dx() + cursorX - scrollX;
	int16_t y = get_dy() + cursorY - scrollY;
	moveCursor(x, y);
}

uint8_t DebugScroller::setFile(char *file)
{
	scrollY = cursorY = scrollX = 0;
	cursorX = 4;
	if (viewFile->setFile(file))
	{
		maxY = viewFile->numLines;
		return 1;
	}
	return 0;
}

DebugGump::DebugGump(NewGumpId id, NewGump *parent, Rect r, Debugger *d, uint8_t fromInter) :
	WindowGump(id, parent, r, 0, 1, 0)
{
	theBaseCamera->textVideoMode();
	clear();
	if (!init)
	{
		memset(watches, 0, sizeof watches);
		init++;
	}
	fromInterpreter = fromInter;
	f_2e = 0;
	f_1d |= 0x10;
	sourceFrame = new WindowFrame(NewGumpId(0), this,
		Rect(rect.f_00, rect.f_02 + 1, rect.f_04, rect.f_06 - 15), doubleFrame, 0x1f);
	scroller = new DebugScroller(NewGumpId(0), this,
		Rect(rect.f_00 + 1, rect.f_02 + 2, rect.f_04 - 1, rect.f_06 - 16), d, sourceFrame);
	inputFrame = new WindowFrame(NewGumpId(0), this,
		Rect(rect.f_00, rect.f_06 - 14, rect.f_04, rect.f_06), singleFrame, 0x70);
	input = new WindowGump(NewGumpId(0), this,
		Rect(rect.f_00 + 1, rect.f_06 - 13, rect.f_04 - 1, rect.f_06 - 1), 0x70, 1, inputFrame);
	if (!sourceFrame || !scroller || !inputFrame || !input)
		outOfMemory(__FILE__, 457);	// __LINE__
	input->clear();
	registerHotKeys(0x13d, 0x166, 0x142, 0x141, 0x161, 0x13, 9, 0xe, 0x17, 0x11, 0x164, 6, 0);
	buildMenu();
}

DebugGump::~DebugGump(void)
{
	theBaseCamera->graphicVideoMode();
}

// Reads a line in the input window, after an optional prompt.
void DebugGump::getInput(char *buffer, char *prompt)
{
	char *p;
	scroller->deactivate();
	input->activate();
	input->clear();
	moveCursor(get_dx() + input->rect.f_00, get_dy() + input->rect.f_02);
	if (prompt)
		printf(prompt);
	gets(buffer);
	for (p = buffer; *p && *p != '\r' && *p != '\n'; p++)
		;
	*p = 0;
	input->deactivate();
	scroller->activate();
	refresh();
}

void DebugGump::message(char *text)
{
	scroller->deactivate();
	input->activate();
	input->clear();
	moveCursor(get_dx() + input->rect.f_00, get_dy() + input->rect.f_02);
	printf(text);
	input->deactivate();
	scroller->activate();
	refresh();
}

// Prints the variable `name` of the current routine, or a global flag of that name.
// Row -1 inspects it in full; other rows show it as a watch.
void DebugGump::dumpSymbol(char *, char *data, char *symbols, char *name, int16_t row)
{
	char count;
	int16_t i;
	int16_t bitNumber;
	int16_t numBits;
	char symName[80];

	if (*name == '*')
	{
		switch (toupper(name[1]))
		{
		case 'M':
			printf("coreleft = %ld\n", coreleft());
			return;
		case 'Y':
			Yamm::heyMan();
			printf("Press any key to continue.");
			getch();
			return;
		}
		return;
	}
	count = *symbols++;
	for (i = 0; i < count; i++)
	{
		int16_t isParam = *symbols++;
		int16_t type = *symbols++;
		char offset = *symbols++;
		char size = *symbols++;
		int16_t length = strlen(symbols);
		strncpy(symName, symbols, 80);
		symbols += length + 1;
		int32_t value = 0;
		if (strcmp(symName, name))
			continue;
		switch (type)
		{
		case 'b':
			if (isParam == 0)
				value = *(char *)(data + offset);
			else
				value = *(char *)((char *)u8_ptr_from_far(u8_far_load(data + 6)) - offset);
			break;
		case 'i':
		case 'l':
		case 's':
		case 'z':
			if (isParam == 0)
				value = *(int16_t *)(data + offset);
			else
				value = *(int16_t *)((char *)u8_ptr_from_far(u8_far_load(data + 6)) - offset);
			break;
		case 'd':
		case 'p':
			if (isParam == 0)
				value = *(int32_t *)(data + offset);
			else
				value = *(int32_t *)((char *)u8_ptr_from_far(u8_far_load(data + 6)) - offset);
			break;
		case '$':
			if (isParam == 0)
				value = (int32_t)u8_far_from_ptr(data + offset);
			else
				value = (int32_t)u8_far_from_ptr((char *)u8_ptr_from_far(u8_far_load(data + 6)) - offset);
			break;
		}
		if (row == -1)
		{
			input->clear();
			input->gotoxy(0, 0);
			input->print("%s", name);
			input->gotoxy(0, 1);
			for (int16_t j = 0; j < 9; j++)
			{
				if (symbolTypes[j].type == type)
				{
					input->print("%s", symbolTypes[j].name);
					break;
				}
			}
			input->gotoxy(0, 2);
			if (type == 's')
			{
				input->print("%s", Yamm::resolve(value));
				return;
			}
			if (type == 'l' || type == 'z')
			{
				uint16_t index = 0;
				YammList list(value, 2, 0);
				while (list.traverse(index))
				{
					if (type == 'z')
						input->print("%s", Yamm::resolve(*(uint16_t *)Yamm::resolve(index + 2)));
					else
						for (int16_t k = 2; k < size + 2; k++)
							input->print("%02x ", *(char *)Yamm::resolve(index + k) & 0xff);
					input->print(".");
				}
				return;
			}
			if (type == '$')
			{
				for (int16_t k = 0; k < size; k++)
					input->print("%02x ", *(char *)(value + k) & 0xff);
				return;
			}
			input->print("%ld (0x%lx)", value, value);
			return;
		}
		else
		{
			input->gotoxy(0, row);
			input->print("%s = ", name);
			if (type == 's')
			{
				input->print("%s", Yamm::resolve(value));
				return;
			}
			if (type == 'l' || type == 'z')
			{
				uint16_t index = 0;
				YammList list(value, 2, 0);
				while (list.traverse(index))
				{
					if (type == 'z')
						input->print("%s", Yamm::resolve(*(uint16_t *)Yamm::resolve(index + 2)));
					else
						for (int16_t k = 2; k < size + 2; k++)
							input->print("%02x ", *(char *)Yamm::resolve(index + k) & 0xff);
					input->print(".");
				}
				return;
			}
			if (type == '$')
			{
				for (int16_t k = 0; k < size; k++)
					input->print("%02x ", *(char *)(value + k) & 0xff);
				return;
			}
			input->print("%ld (0x%lx)", value, value);
			return;
		}
	}
	if (GlobalFlag::findFlag(name, bitNumber, numBits))
	{
		input->clear();
		input->gotoxy(0, 0);
		input->print("%s", name);
		input->gotoxy(0, 1);
		input->print("GLOBAL FLAG, bit number %d, num bits %d", bitNumber, numBits);
		input->gotoxy(0, 2);
		input->print("%d", GlobalFlag::get(bitNumber, numBits));
	}
}

void DebugGump::draw(int16_t, int16_t)
{
	if (debugger->currentInfo)
	{
		Info *info = debugger->getCurrentInfo();
		if (info)
		{
			int16_t row = 0;
			for (int16_t i = 0; i < NUM_WATCHES; i++)
				if (watches[i])
				{
					dumpSymbol(info->process, info->data, info->symbols, watches[i], row);
					row++;
				}
		}
	}
}

void DebugGump::commandKeyboard(Event &event)
{
	switch (event.data)
	{
	case 0x13d:	// F3
		event.data = CMD_LOAD_UNIT;
		break;
	case 0x166:	// ctrl-F9
		event.data = CMD_RUN;
		break;
	case 0x142:	// F8
		event.data = CMD_STEP_OVER;
		break;
	case 0x141:	// F7
		event.data = CMD_TRACE_INTO;
		break;
	case 9:
	case 0x161:
		event.data = CMD_INSPECT;
		break;
	case 0x17:
	case 0x164:
		event.data = CMD_WATCH;
		break;
	case 0x13:
		event.data = CMD_FIND;
		break;
	case 0xe:
		event.data = CMD_SEARCH_AGAIN;
		break;
	case 0x11:
		event.data = CMD_CLEAR_WATCH;
		break;
	case 6:
		event.data = CMD_CHANGE_FLAG;
		break;
	}
	doMenuCommand(event);
}

void DebugGump::commandPrivate(Event &event)
{
	switch (event.from->newGumpId.getInstance())
	{
	case 1:
		doMenuCommand(event);
	}
}

void DebugGump::doMenuCommand(Event &event)
{
	Info *info = debugger->getCurrentInfo();
	int16_t i;

	switch (event.data)
	{
		int16_t bitNumber;
		int16_t numBits;
		int16_t value;
		int16_t max;
		int16_t old;
		char *format;
		int16_t skip;
		char buffer[80];
		char prompt[256];

	case CMD_RUN:
		debugger->steppingOver = 0;
		debugger->tracing = 0;
		theBaseCamera->graphicVideoMode();
		killMyself();
		return;
	case CMD_STEP_OVER:
		debugger->stepOver();
		killMyself();
		return;
	case CMD_TRACE_INTO:
		debugger->steppingOver = 0;
		debugger->tracing = 1;
		killMyself();
		return;
	case CMD_LOAD_UNIT:
		getInput(buffer, 0);
		if (!scroller->setFile(buffer))
			message("unable to open file");
		refresh();
		return;
	case CMD_BREAK:
		// DOS: int 3, a breakpoint for a debugger running the game.
		refresh();
		return;
	case CMD_INSPECT:
		getInput(buffer, 0);
		dumpSymbol(info->process, info->data, info->symbols, buffer, -1);
		return;
	case CMD_WATCH:
		getInput(buffer, 0);
		for (i = 0; i < NUM_WATCHES; i++)
		{
			if (!watches[i])
			{
				watches[i] = new char[strlen(buffer) + 1];
				strcpy(watches[i], buffer);
				return;
			}
		}
		return;
	case CMD_CLEAR_WATCH:
		for (i = 0; i < NUM_WATCHES; i++)
		{
			if (watches[i])
			{
				delete watches[i];
				watches[i] = 0;
			}
		}
		return;
	case CMD_FIND:
		getInput((char *)searchBuffer, "Search For:");
		for (i = 0; i < DebugScroller::viewFile->getNumLines(); i++)
		{
			if (strstr(DebugScroller::viewFile->getLine(i), (char *)searchBuffer))
			{
				scroller->scrollTo(i);
				return;
			}
		}
		return;
	case CMD_SEARCH_AGAIN:
		for (i = scroller->cursorY + 1; i < DebugScroller::viewFile->getNumLines(); i++)
		{
			if (strstr(DebugScroller::viewFile->getLine(i), (char *)searchBuffer))
			{
				scroller->scrollTo(i);
				return;
			}
		}
		return;
	case CMD_CHANGE_FLAG:
		getInput(buffer, "Flag name:");
		if (GlobalFlag::findFlag(buffer, bitNumber, numBits))
		{
			max = 1 << (numBits - 1);
			old = GlobalFlag::get(bitNumber, numBits);
			sprintf(prompt, "Old Value: (%d)   [0x%04x]  New Value: (0-%d) [0x0-0x%04x]:", old, old, max, max);
			getInput(buffer, prompt);
			if (strlen(buffer) == 0)
				return;
			skip = 0;
			if (buffer[1] == 'x' || buffer[1] == 'X')
			{
				format = "%x";
				skip = 2;
			}
			else
				format = "%d";
			sscanf(buffer + skip, format, &value);
			if (value >= 0 && value <= max)
			{
				GlobalFlag::set(bitNumber, numBits, value);
				message("Done!");
				return;
			}
			message("Out of Range!");
			return;
		}
		message("This flag does not exist!");
	}
}

void DebugGump::buildMenu(void)
{
	TextMenuBarGump &bar = *new TextMenuBarGump(1, this)
		+ (*new TextMenuItemGump("File", 0, 1)
			+ (*new TextMenuItemGump("Load Unit... F3", CMD_LOAD_UNIT, 1)
			+ *new TextMenuItemGump("View File...", CMD_VIEW_FILE, 1)
			+ *new TextMenuItemGump("Break to TDP", CMD_BREAK, 1)))
		+ (*new TextMenuItemGump("Run", 0, 1)
			+ (*new TextMenuItemGump("Run              Ctl-F9", CMD_RUN, 1)
			+ *new TextMenuItemGump("Run to cursor        F4", CMD_RUN_TO_CURSOR, 1)
			+ *new TextMenuItemGump("Trace into           F7", CMD_TRACE_INTO, 1)
			+ *new TextMenuItemGump("Step over            F8", CMD_STEP_OVER, 1)
			+ *new TextMenuItemGump("Run until return Alt-F8", CMD_RUN_TO_RETURN, 1)
			+ *new TextMenuLineGump
			+ *new TextMenuItemGump("Breakpoint           F2", CMD_BREAKPOINT, 1)))
		+ (*new TextMenuItemGump("Search", 0, 1)
			+ (*new TextMenuItemGump("Find...", CMD_FIND, 1)
			+ *new TextMenuItemGump("Search again", CMD_SEARCH_AGAIN, 1)
			+ *new TextMenuLineGump
			+ *new TextMenuItemGump("Go to line number...", CMD_GOTO_LINE, 1)))
		+ (*new TextMenuItemGump("Data", 0, 1)
			+ (*new TextMenuItemGump("Watch", CMD_WATCH, 1)
			+ *new TextMenuItemGump("Inspect...", CMD_INSPECT, 1)
			+ *new TextMenuItemGump("Clear Watch", CMD_CLEAR_WATCH, 1)
			+ *new TextMenuItemGump("Change Flag", CMD_CHANGE_FLAG, 1)));
	bar.moveto(0, 0);
	bar.rect.f_04 = 79;
	bar.refresh();
}

ViewFile::ViewFile(void)
{
	text = 0;
	numLines = 0;
	memset(name, 0, sizeof name);
}

uint8_t ViewFile::setFile(char *file)
{
	char *p = strstr(file, "\r");
	if (p)
		*p = 0;
	if (text)
	{
		farfree(text);
		text = 0;
	}
	numLines = 0;
	if (!file)
		return 1;
	FileSpec spec(file, 0);
	if (!spec.dir[0])
		strcpy(spec.dir, UsecodeDir);
	if (!spec.ext[0])
		strcpy(spec.ext, ".unk");
	strcpy(name, spec.name);
	char *path = (char *)spec.get(1);
	if (!spec.exists())
		return 0;
	if (filename && !strcmp(path, filename))
		return 1;
	open(path, ReadOnly);
	loadFile();
	close();
	return 1;
}

void ViewFile::loadFile(void)
{
	int32_t length = size();
	text = (char *)farmalloc(length + 1);
	if (!text)
		outOfMemory(__FILE__, 1030);	// __LINE__
	read(text, length);
	// DOS passed the text's linear address, read back from its descriptor.
	numLines = parse(lines, text, length, 0);
	if (numLines >= 4000)
		halt(__FILE__, 1062);	// __LINE__
}

char *ViewFile::getLine(uint16_t line)
{
	return text + lines[line];
}
