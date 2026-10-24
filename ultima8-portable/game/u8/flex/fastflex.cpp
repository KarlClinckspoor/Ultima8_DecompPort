// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: FLEX\FASTFLEX.C

#include "u8port.h"

#include <stddef.h>
#include "cfile.h"
#include "cexit.h"
#include "fastflex.h"

// Inline in the shared headers when this file was built.
inline Index::Index(void) { size = 0; offset = 0; }
inline uint8_t BaseFile::is_valid(void) { return handle != -1; }
inline BaseFile::~BaseFile(void) { if (is_valid()) close(); }
inline SharedFile::SharedFile(void) {}
inline FlexFile::FlexFile(void) {}

FastFlex::FastFlex(char *name, Index *indexes, OpenMode mode, int16_t count)
{
	init(name, indexes, mode, count);
}

FastFlex::~FastFlex(void)
{
	close();
	if (indexes)
	{
		delete [] indexes;
		indexes = NULL;
	}
}

void FastFlex::create(char *name, int16_t count)
{
	FlexFile::create(name, count);
	readInIndexes();
}

// Opens the file and loads its table, into indexes if given.
void FastFlex::init(char *name, Index *indexes, OpenMode mode, int16_t count)
{
	this->indexes = indexes;
	FlexFile::init(name, mode, count);
	if (this->indexes == NULL)
	{
		this->indexes = new Index[totalIndexes];
		if (this->indexes == NULL)
			halt(__FILE__, 49);	// __LINE__
	}
	readInIndexes();
}

void FastFlex::getIndex(int16_t record, Index &index)
{
	index = indexes[record];
}

void FastFlex::setIndex(int16_t record, Index &index)
{
	indexes[record] = index;
	write((char *)&index, sizeof(Index), sizeof(Index) * record + sizeof(FlexHeader));
}

void FastFlex::readInIndexes(void)
{
	read((char *)indexes, sizeof(Index) * totalIndexes, sizeof(FlexHeader));
}
