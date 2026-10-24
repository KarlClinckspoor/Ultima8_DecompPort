#ifndef SHAPHAND_H
#define SHAPHAND_H

#include "u8port.h"

#include "fastflex.h"
#include "handler.h"

class Rect;

// A shape's frame table as it sits in the cache. A frame's offset is
// its cache offset once loaded; FRAME_ON_DISK marks a file offset.
struct FrameSlot
{
	uint32_t offset;
	uint16_t size;
};

struct ShapeHeader
{
	uint16_t maxWidth;
	uint16_t maxHeight;
	uint16_t numFrames;
};

struct ShapeIndex : public ShapeHeader
{
	FrameSlot frames[1];
};

// A loaded frame: who owns it and the table entry it replaced, then
// the frame as stored in the file.
struct FrameData
{
	uint16_t shape;
	uint16_t frame;
	uint32_t oldOffset;
};

struct FrameHeader
{
	uint16_t compression;
	int16_t width;
	int16_t height;
	int16_t xoff;
	int16_t yoff;
};

// These are the layouts of U8SHAPES.FLX records as they sit in the cache.
static_assert(sizeof(FrameSlot) == 6, "FrameSlot is 6 bytes");
static_assert(sizeof(ShapeHeader) == 6, "ShapeHeader is 6 bytes");
static_assert(sizeof(ShapeIndex) == 12, "ShapeIndex is the header and one slot");
static_assert(sizeof(FrameData) == 8, "FrameData is 8 bytes");
static_assert(sizeof(FrameHeader) == 10, "FrameHeader is 10 bytes");

#define FRAME_ON_DISK	0x80000000L
#define FRAME_FLAG	0x40000000L

class ShapeFlex : public FastFlex
{
public:
	ShapeFlex(void);
};

class ShapeHandler : public CacheHandler
{
public:
	ShapeHandler(void);
	virtual void cacheOut(uint32_t, uint16_t);
	virtual uint8_t verify(uint32_t);
	void getDim(uint16_t, uint16_t, int16_t &, int16_t &, int16_t &, int16_t &);
	void getInfo(uint16_t, int16_t &, int16_t &, int16_t &);
	void getRect(uint16_t, uint16_t, Rect &, int16_t, int16_t);
	char *loadFrame(uint16_t, uint16_t, uint32_t, uint32_t);
	char *get(uint16_t, uint16_t);
	char *get(uint16_t, uint16_t, uint16_t &);
	void flushType(uint16_t);
	void flushFrame(uint32_t, uint16_t);
	static uint16_t startHandle;
	static ShapeFlex *shapeFile;
};

class FrameHandler : public CacheHandler
{
public:
	FrameHandler(void);
	virtual void notifyMoved(uint32_t);
	virtual void cacheOut(uint32_t, uint16_t);
};

void initShapeHandler(void);
void uninitShapeHandler(void);

extern ShapeHandler *shapeHandler;
extern FrameHandler *frameHandler;

#endif
