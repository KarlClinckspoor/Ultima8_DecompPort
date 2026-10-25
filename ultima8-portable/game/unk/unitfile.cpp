// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: ..\UNK\UNITFILE.C

#include "u8port.h"

#include "cexit.h"
#include "filespec.h"
#include "scratchm.h"
#include "fastflex.h"
#include "uprocess.h"
#include "unitfile.h"

// Every unit record starts with a 12-byte header that load() skips.
#define UNIT_HEADER	12

U8_INLINE Index::Index(void) { size = 0; offset = 0; }

char *UnitFile::load(char *name)
{
	int32_t magic, size, count;

	open(fileSpec(0, UsecodeDir, name, ".uni"), (OpenMode)2);
	read((char *)&magic, 4);
	read((char *)&size, 4);
	read((char *)&count, 4);
	uint16_t length = size - UNIT_HEADER;
	char *data = new char[length];
	if (data == 0)
		outOfMemory(__FILE__, 81);	// __LINE__
	read(data, length);
	return data;
}

char *UnitFile::load(uint16_t unit)
{
	Index index;
	usecodeFile->getIndex(unit + 2, index);
	if (index.size <= UNIT_HEADER)
		halt(__FILE__, 99);	// __LINE__
	char *data = new char[index.size - UNIT_HEADER];
	if (data == 0)
		outOfMemory(__FILE__, 104);	// __LINE__
	usecodeFile->readRecord(unit + 2, data, index.size - UNIT_HEADER, UNIT_HEADER);
	return data;
}

int16_t UnitFile::getSize(uint16_t unit)
{
	Index index;
	usecodeFile->getIndex(unit + 2, index);
	return index.size - UNIT_HEADER;
}
