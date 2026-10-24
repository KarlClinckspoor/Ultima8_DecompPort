#ifndef HANDLER_H
#define HANDLER_H

#include "u8port.h"

#include "cache.h"

#define MAX_HANDLES	4000

// Owns a type of cache node: reserves a run of handles and loads
// the data behind them on demand.
class CacheHandler
{
public:
	CacheNodeType type;

	CacheHandler(CacheNodeType t) { type = t; }
	~CacheHandler(void);
	virtual void notifyMoved(uint32_t) {}
	virtual void cacheOut(uint32_t, uint16_t) {}
	virtual uint8_t verify(uint32_t) { return 1; }
	static void init(void);
	static int16_t reserve(int16_t);
	static uint32_t *handles;
	static int16_t numHandles;
};

void initCacheHandler(void);

#endif
