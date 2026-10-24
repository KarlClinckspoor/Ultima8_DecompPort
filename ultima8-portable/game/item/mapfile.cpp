// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\ITEM\MAPFILE.C

#include "u8port.h"

#include <stddef.h>
#include "cfile.h"
#include "cexit.h"
#include "filespec.h"
#include "scratchm.h"
#include "glob.h"
#include "itemcach.h"
#include "mapfile.h"

// Inline in the shared headers when this file was built.
inline Index::Index(void) { size = 0; offset = 0; }

uint32_t mapNames = 0;

int16_t RegionFile::numItems(uint16_t region)
{
	Index index;
	getIndex(region, index);
	return index.size / sizeof(OneItem);
}

MapFile::MapFile(void) :
	StepFlexFile(::fileSpec(NULL, GamedatDir, nonfixedFileDesc, NULL), 256, 1)
{
}

void MapFile::restart(void)
{
	if (handle != 6)
		close();
	init(::fileSpec(NULL, GamedatDir, nonfixedFileDesc, NULL), (OpenMode)2, -1);
}

void MapFile::resetChecksum(void)
{
	FlexHeader header;
	getHeader(&header);
	header.checksum = getChecksum();
	setHeader(&header);
}

FixedMapFile::FixedMapFile(void) :
	FastFlex(fileSpec(NULL, StaticDir, fixedFileDesc, NULL), NULL, (OpenMode)0, 256)
{
}
