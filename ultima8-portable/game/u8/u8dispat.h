#ifndef U8DISPAT_H
#define U8DISPAT_H

#include "u8port.h"

#include "u8process.h"
#include "dispatch.h"

class BaseFile;
class CacheFont;
class U8GumpShapeDrawer;

// The process that runs the gump dispatcher, and owns the main font, gump
// shape drawer, camera and mouse pointer.
class U8Dispatcher : public Process, public Dispatcher
{
public:
	CacheFont *font;
	U8GumpShapeDrawer *drawer;

	U8Dispatcher(int16_t);
	virtual void process(void);
	virtual void freeMemory(void);
	virtual void load(BaseFile *);
	virtual void save(BaseFile *);
	virtual void postLoad(void);
	virtual void uninit(void);
};

extern U8Dispatcher *theU8Dispatcher;

#endif
