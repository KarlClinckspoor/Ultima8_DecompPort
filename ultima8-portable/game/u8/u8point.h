#ifndef U8POINT_H
#define U8POINT_H

#include "u8port.h"

#include "ctmpoint.h"

class BaseFile;

enum PointerModes
{
	PointerModes_0
};

class U8MousePointer : public TMMousePointer
{
public:
	U8MousePointer(int16_t, int16_t);
	virtual int16_t getFrame(int16_t, int16_t);
	static int16_t calcErection(int16_t, int16_t);
	static void pushMode(PointerModes, uint8_t);
	static void popMode(void);
	static int16_t getDir(void);
	static void save(BaseFile *);
	static void load(BaseFile *);
	static int16_t centerX;
	static int16_t centerY;
	static int16_t shape;
	static int16_t maxShapes;
	static int16_t modes[5];
	static int16_t sp;
	static int16_t erection;
};

extern U8MousePointer *theU8Mouse;

#endif
