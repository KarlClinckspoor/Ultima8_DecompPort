#ifndef FASTFLEX_H
#define FASTFLEX_H

#include "u8port.h"

#include "flex.h"
#include "cfile.h"

// A FlexFile that keeps its whole Index table in memory.
class FastFlex : public FlexFile
{
public:
	Index *indexes;

	FastFlex(void);
	FastFlex(char *, Index *, OpenMode, int16_t);
	virtual ~FastFlex(void);
	virtual void getIndex(int16_t, Index &);
	virtual void setIndex(int16_t, Index &);
	virtual void init(char *, Index *, OpenMode, int16_t);
	void create(char *, int16_t);
	void readInIndexes(void);
};

#endif
