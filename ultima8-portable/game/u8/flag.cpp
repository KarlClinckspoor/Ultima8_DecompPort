// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: FLAG.C

#include "u8port.h"

// #include <io.h>  (DOS only)
// #include <mem.h>  (DOS only)
#include <string.h>
#include "cfile.h"
#include "fexist.h"
#include "filespec.h"
#include "error.h"
#include "scratchm.h"
#include "flag.h"

// Inline in the shared headers when this file was built.
inline uint8_t BaseFile::is_valid(void) { return handle != -1; }
inline BaseFile::~BaseFile(void) { if (is_valid()) close(); }
inline FileSpec::FileSpec(char *path, int16_t attrib) { become(path, attrib); }

static char flagFileName_text[] = "flag.dat";
char *flagFileName = flagFileName_text;
char *GlobalFlag::flagSym = 0;
int16_t GlobalFlag::flagSymSize = 0;
char *GlobalFlag::bits;

int16_t GlobalFlag::get(int16_t index, int16_t size)
{
	int32_t word = *(int32_t *)(bits + (index >> 3));
	int32_t mask = ((1 << size) - 1) << (index & 7);
	int32_t value = (word & mask) >> (index & 7);
	return value;
}

void GlobalFlag::set(int16_t index, int16_t size, int16_t value)
{
	int32_t *word = (int32_t *)(bits + (index >> 3));
	int32_t mask = ~(((1 << size) - 1) << (index & 7));
	*word &= mask;
	*word |= value << (index & 7);
}

void GlobalFlag::destroy(void)
{
	if (bits)
	{
		delete bits;
		bits = 0;
	}
	if (flagSym)
	{
		delete flagSym;
		flagSym = 0;
	}
}

void GlobalFlag::load(char *dir)
{
	char *name;

	if (!bits)
	{
		bits = new char[0x400];
		if (!bits)
			outOfMemory(__FILE__, 50);	// __LINE__
	}
	memset(bits, 0, 0x400);
	name = fileSpec(0, dir, flagFileName, 0);
	if (FileExists(name))
	{
		BaseFile file(name, ReadWrite);
		file.read(bits, file.size());
	}
	FileSpec spec(fileSpec(0, UsecodeDir, "flagsym.dat", 0), 0);
	if (spec.exists())
	{
		BaseFile file(spec, ReadWrite);
		flagSymSize = file.size();
		if (!flagSym)
		{
			flagSym = new char[flagSymSize];
			if (!flagSym)
				return;
			file.read(flagSym, flagSymSize);
		}
	}
}

void GlobalFlag::save(char *dir)
{
	char *name = fileSpec(0, dir, flagFileName, 0);
	unlink(name);
	BaseFile file;
	file.forceOpen(name, ReadWrite);
	file.write(bits, 0x400);
}

// flagsym.dat holds, per flag, its name and two words: bit index and width.
char GlobalFlag::findFlag(char *name, int16_t &index, int16_t &size)
{
	char found = 0;

	if (flagSym)
	{
		char *p = flagSym;
		for (uint16_t pos = 0; pos < (uint16_t)flagSymSize; )
		{
			int16_t len = strlen(p) + 1;
			char *sym = p;
			p += len;
			if (!strcmp(sym, name))
			{
				index = ((int16_t *)p)[0];
				size = ((int16_t *)p)[1];
				found = 1;
				break;
			}
			p += 4;
			pos += len + 4;
		}
	}
	return found;
}
