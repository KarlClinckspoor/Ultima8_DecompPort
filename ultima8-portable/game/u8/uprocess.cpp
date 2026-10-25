// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: UPROCESS.C

#include "u8port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <conio.h>  (DOS only)
// #include <dos.h>  (DOS only)
#include "cfile.h"
#include "filespec.h"
#include "fexist.h"
#include "fastflex.h"
#include "unitfile.h"
#include "error.h"
#include "scratchm.h"
#include "system.h"
#include "item.h"
#include "type.h"
#include "npc.h"
#include "yamm.h"
#include "kernel.h"
#include "uprocess.h"
#include "intrinsics.h"
#include "farptr.h"

// Inline in the shared headers when this file was built.
U8_INLINE uint8_t BaseFile::is_valid(void) { return handle != -1; }
U8_INLINE BaseFile::~BaseFile(void) { if (is_valid()) close(); }
U8_INLINE Index::Index(void) { size = 0; offset = 0; }


Overload *RoutineIndex::overloadTable = 0;
int32_t RoutineIndex::unkMemoryUsed = 0;
int32_t RoutineIndex::maxUnkMemory = 0x19000L;
FastFlex *usecodeFile = 0;
uint8_t failedInit = 0;
uint8_t unkInitted = 0;
UnitIndex RoutineIndex::unitIndex[MAX_UNITS];
void **cTable;
uint16_t numCFunctions;

// Turns a C prototype from the link map ("name(int, char far*)") into the
// name the usecode file uses for it.  Returns 0 if the line has no name.
// (The host never reads the link map; see initCFunctions.)
uint8_t mangleName(char *mangled, char *name)
{
	char *paren;
	char *space;
	char *comma;
	char *close;
	char *end;
	uint8_t last;

	paren = strstr(name, "(");
	if (paren == 0)
	{
		char *cr = strstr(name, "\r");
		if (cr)
		{
			*cr = 0;
			strcpy(mangled, name);
			return 1;
		}
		return 0;
	}
	*paren = 0;
	strcpy(mangled, name);
	name = paren + 1;
	if (*name == ')')
		return 1;
	name = paren + 1;
	last = 0;
	while (!last)
	{
		while (!*name || *name == ' ')
			name++;
		space = strstr(name, " ");
		comma = strstr(name, ",");
		close = strstr(name, ")");
		if (space == 0)
			space = (char *)-1L;
		if (comma == 0)
			comma = (char *)-1L;
		if (close == 0)
			close = (char *)-1L;
		if (space < comma && space < close)
			end = space;
		else if (comma < space && comma < close)
			end = comma;
		else
		{
			if (close == (char *)-1L)
				return 1;
			end = close;
			last++;
		}
		if ((comma != 0 && comma != (char *)-1L) || (close != 0 && close != (char *)-1L))
		{
			if (comma == 0 || comma == (char *)-1L)
				comma = close;
			*comma = 0;
			if (!strcmp(name, "char far*"))
			{
				name += 10;
				strcat(mangled, "@s");
				continue;
			}
			*comma = ',';
		}
		if (end)
			*end = 0;
		if (!strcmp(name, "unsigned"))
		{
			name += 8;
			continue;
		}
		if (!strcmp(name, "int"))
		{
			name += 3;
			strcat(mangled, "@i");
			continue;
		}
		if (!strcmp(name, "short"))
		{
			name += 5;
			strcat(mangled, "@i");
			continue;
		}
		if (!strcmp(name, "char"))
		{
			name += 4;
			strcat(mangled, "@i");
			continue;
		}
		if (!strcmp(name, "long"))
		{
			name += 4;
			strcat(mangled, "@d");
			continue;
		}
		if (!strstr(name, "*") && !strstr(name, "&"))
		{
			strcat(mangled, "@$");
			strcat(mangled, name);
			strcat(mangled, "$");
		}
		name += strlen(name) + 1;
	}
	return 1;
}

// Replaces each "$type$" in a mangled name with "i".
void enumMangle(char *out, const char *in)
{
	int16_t i = 0;
	int16_t j = 0;

	while (in[i])
	{
		if (in[i] != '$')
		{
			out[j] = in[i];
			i++;
			j++;
		}
		else
		{
			out[j] = 'i';
			j++;
			i++;
			while (in[i++] != '$')
				;
		}
	}
	out[j] = 0;
}

// Builds cTable, the C function each intrinsic number calls.
//
// The DOS build read the addresses from the link map (u8.map) when it was newer than its
// cache, unkcoff.dat in the usecode folder, else from the cache: one far address per
// intrinsic, the last that of main, in the map's segment numbering, relocated to selectors
// here. The shipped game has the cache. The host's functions have no such addresses, so the
// table of intrinsics (intrinsics.cpp) keeps each one's address in U8.EXE's map, and the
// cache's addresses pick the entries; with no cache the intrinsics take their U8.EXE order.
void initCFunctions(void)
{
	FileSpec coffSpec(0, UsecodeDir, "unkcoff", ".dat");

	if (coffSpec.exists())
	{
		BaseFile coffFile(coffSpec, ReadWrite);
		uint16_t size = coffFile.size();
		uint32_t *addresses;
		uint16_t i;

		numCFunctions = size / 4;
		addresses = new uint32_t[numCFunctions];
		cTable = new void *[numCFunctions];
		if (addresses == 0 || cTable == 0)
			outOfMemory(__FILE__, 366);	// __LINE__
		coffFile.read((char *)addresses, size);
		for (i = 0; i < numCFunctions; i++)
			cTable[i] = (void *)findIntrinsic(addresses[i] >> 16, (uint16_t)addresses[i]);
		delete[] addresses;
	}
	else
	{
		uint16_t i;

		numCFunctions = numIntrinsics;
		cTable = new void *[numCFunctions];
		for (i = 0; i < numCFunctions; i++)
			cTable[i] = (void *)&intrinsics[i];
	}
}

// Opens the usecode file for the avatar's language.
void initUnk(void)
{
	initYamm();
	if (!*UsecodeDir)
		return;
	usecodeFileName[0] = avatar.getLanguageChar();
	if (!FileExists(fileSpec(0, UsecodeDir, usecodeFileName, 0)))
		return;
	usecodeFile = new FastFlex(fileSpec(0, UsecodeDir, usecodeFileName, 0), 0, ReadOnly, -1);
	Index index;
	usecodeFile->getIndex(1, index);
	RoutineIndex::overloadTable = (Overload *)new char[index.size];
	if (RoutineIndex::overloadTable == 0)
		outOfMemory(__FILE__, 408);	// __LINE__
	usecodeFile->readRecord(1, RoutineIndex::overloadTable);
	initCFunctions();
	unkInitted = 1;
}

void reloadUnk(void)
{
}

// Starts the item's usecode for an event, if its unit handles that event.
uint8_t spawnUnk(Item *item, uint32_t event, uint16_t number, uint16_t &pid, uint32_t arg)
{
	UnkProcess *process;
	uint16_t unit;

	if (failedInit)
		return 0;
	// Permanent NPCs (status bit 0x80 clear) and unk eggs have units of their own.
	if (item->isNpc() && !(Boolean)((item->getStatus() & 0x80) ? 1 : 0))
		unit = item->referent + 1024;
	else if (item->getFamily() == UNKEGG_FAMILY)
		unit = item->getUnkEggType() + 1151;
	else
		unit = item->getType();
	if (RoutineIndex::overloadTable && (RoutineIndex::overloadTable[unit].events & event))
	{
		process = new UnkProcess(0, item, 2, unit, number, 0, (char *)&arg, 4);
		if (process->isPhantom())
			pid = 0;
		else
			pid = process->pid;
		return 1;
	}
	return 0;
}

UnkProcess::UnkProcess(uint32_t *dest, void *thisPtr, uint16_t size, uint16_t unitNumber, uint16_t entry, uint8_t isOffset, char *args, int16_t argSize) :
	routine((char *)thisPtr, size, RoutineIndex::getCode(unitNumber, entry, isOffset, -1), this)
{
	Kernel::setIdString(pid, RoutineIndex::overloadTable[unitNumber].name);
	// Far pointers into this process's stack name it by pid (farptr.h).
	u8_far_register(FAR_STACK + pid, &routine, sizeof(routine));
	finished = 0;
	thisSize = size;
	thisOffset = 0;
	unit = unitNumber;
	resultDest = dest;
	// The stack holds the this data (copied by Data), the arguments, then a
	// pointer back to the this data.
	if (args)
	{
		char *thisSp = routine.sp;
		uint16_t length = argSize;

		routine.sp -= length;
		memcpy(routine.sp, args, argSize);
		if (size)
		{
			char *p = thisSp;

			routine.sp -= 4;
			u8_far_store(routine.sp, u8_far_from_ptr(p));
			thisOffset = routine.sp - routine.buffer;
		}
	}
	killed = new char;
	if (killed == 0)
		outOfMemory(__FILE__, 565);	// __LINE__
	*killed = 0;
	process();
}

void UnkProcess::process(void)
{
	int16_t status;

	running = 1;
	status = routine.interpret(result);
	if (status == 1)
	{
		if (resultDest)
			*resultDest = result;
		finished = 1;
		running = 0;
		pop(result);
		return;
	}
	if (status != 4)
		running = 0;
}

void UnkProcess::freeMemory(void)
{
	u8_far_unregister(FAR_STACK + pid);
	Process::freeMemory();
	RoutineIndex::markUnused(unit);
	if (!finished)
		Yamm::clean(pid);
	if (running)
	{
		if (killed)
		{
			*killed = 1;
			return;
		}
	}
	else
		delete killed;
}

void UnkProcess::save(BaseFile *file)
{
	uint16_t offset;

	Process::save(file);
	file->write((char *)&thisOffset, 2);
	file->write((char *)&thisSize, 1);
	if (killed)
		file->write(killed, 1);
	// DOS saved the offset part of ip, which under Phar Lap was the offset into the unit's
	// code (each block had a selector of its own, at offset 0).
	offset = (uint16_t)(routine.ip - RoutineIndex::codeBase(unit));
	file->write((char *)&offset, 2);
}

void UnkProcess::load(BaseFile *file)
{
	int16_t spOffset;
	uint16_t offset;

	Process::load(file);
	spOffset = routine.sp - routine.buffer;
	file->read((char *)&thisOffset, 2);
	file->read((char *)&thisSize, 1);
	if (killed)
	{
		killed = new char;
		file->read(killed, 1);
	}
	resultDest = 0;
	u8_far_register(FAR_STACK + pid, &routine, sizeof(routine));
	if (thisOffset)
		u8_far_store(&routine.buffer[thisOffset], u8_far_from_ptr(&routine.buffer[200 - thisSize]));
	routine.sp = &routine.buffer[spOffset];
	routine.top = &routine.buffer[200];
	file->read((char *)&offset, 2);
	routine.ip = RoutineIndex::getCode(unit, offset, 1, -1);
	routine.process = this;
}

RoutineIndex::RoutineIndex(void)
{
	memset(unitIndex, 0, sizeof(unitIndex));
}

// Returns the unit's cache entry, or 0 with slot set to a free entry.
UnitIndex *RoutineIndex::findUnit(uint16_t unit, UnitIndex *&slot)
{
	uint16_t i;
	int16_t unused = -1;
	UnitIndex *index = unitIndex;

	for (i = 0; i < MAX_UNITS; i++, index++)
	{
		if (index->unit == unit)
			return index;
		else if (index->unit == 0)
		{
			if (slot == 0)
				slot = index;
		}
		else if (index->useCount == 0)
		{
			if (unused == -1)
				unused = i;
		}
	}
	if (slot == 0)
	{
		if (unused == -1)
			halt(__FILE__, 748);	// __LINE__
		cacheOut(unused);
		slot = &unitIndex[unused];
	}
	return 0;
}

int16_t RoutineIndex::getNumFree(void)
{
	int16_t count = 0;
	UnitIndex *index = unitIndex;
	uint16_t i;

	for (i = 0; i < MAX_UNITS; i++, index++)
		if (index->unit == 0 || index->useCount == 0)
			count++;
	return count;
}

RoutineIndex::~RoutineIndex(void)
{
	int16_t i;

	for (i = 0; i < MAX_UNITS; i++)
		if (unitIndex[i].code)
			delete[] unitIndex[i].code;
}

// The code of a unit's routine: entry is an offset into the unit when
// isOffset is set, else an event number.
char *RoutineIndex::getCode(uint16_t unit, uint16_t entry, uint8_t isOffset, int32_t offset)
{
	UnitIndex *slot = 0;
	UnitIndex *index;
	char *code;

	index = findUnit(unit, slot);
	if (index == 0)
	{
		UnitFile file;
		uint16_t size = file.getSize(unit);

		while (unkMemoryUsed + size > maxUnkMemory && cacheOut(-1))
			;
		slot->unit = unit;
		slot->size = size;
		slot->code = file.load(unit);
		u8_far_register(FAR_CODE + unit, slot->code, size);
		index = slot;
		unkMemoryUsed += size;
	}
	index->useCount++;
	code = index->code;
	if (isOffset)
		code += entry;
	else
	{
		int32_t start;
		memcpy(&start, index->code + entry * 4, 4);
		code += start;
	}

	if (offset != -1)
		code += (uint16_t)offset;
	return code;
}

// The unit's code while it is cached; 0 when it is not.
char *RoutineIndex::codeBase(uint16_t unit)
{
	UnitIndex *index;
	uint16_t i;

	for (i = 0, index = unitIndex; i < MAX_UNITS; i++, index++)
		if (index->unit == unit)
			return index->code;
	return 0;
}

void RoutineIndex::markUnused(uint16_t unit)
{
	UnitIndex *slot;
	UnitIndex *index;

	index = findUnit(unit, slot);
	if (index == 0)
		halt(__FILE__, 829);	// __LINE__
	index->useCount--;
}

// Frees a unit's cached code; -1 picks the last unused one.
uint8_t RoutineIndex::cacheOut(int16_t which)
{
	int16_t i;

	if (which == -1)
	{
		for (i = MAX_UNITS - 1; i >= 0; i--)
			if (unitIndex[i].useCount <= 0 && unitIndex[i].code)
				break;
		if (i < 0)
			return 0;
	}
	else
	{
		if (unitIndex[which].useCount)
			return 0;
		i = which;
	}
	u8_far_unregister(FAR_CODE + unitIndex[i].unit);
	delete[] unitIndex[i].code;
	unitIndex[i].code = 0;
	unkMemoryUsed -= unitIndex[i].size;
	unitIndex[i].size = 0;
	unitIndex[i].useCount = 0;
	unitIndex[i].unit = 0;
	return 1;
}

void resetRef(uint16_t ref, uint16_t type)
{
	Kernel::resetRef(ref, (ProcessType)type);
}

void setRef(uint16_t pid, uint16_t type, uint16_t ref)
{
	Process *process;

	process = Kernel::findValidProcess(pid, (ProcessType)type);
	if (process)
		process->setRef(ref);
}
