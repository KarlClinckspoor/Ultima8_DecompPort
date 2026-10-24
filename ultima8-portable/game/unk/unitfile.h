#ifndef UNITFILE_H
#define UNITFILE_H

#include "u8port.h"

#include "cfile.h"
#include "inter.h"
#include "itemfind.h"

enum SearchType
{
	SearchType_0
};

class UnitFile : public BaseFile
{
public:
	char *load(char *);
	char *load(uint16_t);
	int16_t getSize(uint16_t);
};

// Runs a usecode loop: each item found is written to a local
// in the running process's frame.
class Searcher
{
public:
	Data *data;
	uint16_t local;
	ItemFinder *finder;

	~Searcher(void);
	int16_t findFirst(char *, int16_t, int16_t, int16_t, int16_t, SearchType);
	int16_t findNext(void);
};

#endif
