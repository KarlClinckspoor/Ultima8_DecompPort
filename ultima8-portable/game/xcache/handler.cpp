// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\XCACHE\HANDLER.C

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "cexit.h"
#include "handler.h"

uint32_t *CacheHandler::handles = 0;
int16_t CacheHandler::numHandles;

CacheHandler::~CacheHandler(void)
{
	CacheNodePtr::detach(type);
}

void CacheHandler::init(void)
{
	handles = new uint32_t[MAX_HANDLES];
	if (handles == 0)
		outOfMemory(__FILE__, 43);	// __LINE__
	memset(handles, 0, MAX_HANDLES * sizeof(uint32_t));
	numHandles = 0;
}

// Hands out the next run of count handles; returns its first.
int16_t CacheHandler::reserve(int16_t count)
{
	if (count + numHandles < MAX_HANDLES)
	{
		int16_t first = numHandles;
		numHandles += count;
		return first;
	}
	halt(__FILE__, 56);	// __LINE__
}

void initCacheHandler(void)
{
	CacheHandler::init();
}
