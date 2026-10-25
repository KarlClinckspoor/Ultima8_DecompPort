// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\XCACHE\SHAPHAND.C

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "cexit.h"
#include "crect.h"
#include "filespec.h"
#include "scratchm.h"
#include "shaphand.h"

#define NUM_SHAPE_HANDLES	2048

uint16_t ShapeHandler::startHandle;
ShapeFlex *ShapeHandler::shapeFile;
ShapeHandler *shapeHandler;
FrameHandler *frameHandler;

U8_INLINE Index::Index(void) { size = 0; offset = 0; }
U8_INLINE int16_t isEmpty(Index &index) { return index.size == 0; }

ShapeFlex::ShapeFlex(void) :
	FastFlex(fileSpec(0, StaticDir, shapeFileName, 0), 0, (OpenMode)0, -1)
{
}

ShapeHandler::ShapeHandler(void) :
	CacheHandler((CacheNodeType)0)
{
	startHandle = reserve(NUM_SHAPE_HANDLES);
	CacheNodePtr::attach(this, (CacheNodeType)0);
	shapeFile = new ShapeFlex;
	if (shapeFile == 0)
		outOfMemory(__FILE__, 490);	// __LINE__
}

void ShapeHandler::getDim(uint16_t shape, uint16_t frame, int16_t &width, int16_t &height, int16_t &xoff, int16_t &yoff)
{
	FrameHeader *header = (FrameHeader *)get(shape, frame);
	if (header == 0)
	{
		trace((TraceLevel)50, "nullShape t%d f%d!\n\r", shape, frame);
		header = (FrameHeader *)get(0, 0);
	}
	width = header->width;
	height = header->height;
	xoff = header->xoff;
	yoff = header->yoff;
}

void ShapeHandler::getInfo(uint16_t shape, int16_t &maxWidth, int16_t &maxHeight, int16_t &numFrames)
{
	if (handles[shape] == 0)
	{
		char *data = get(shape, 0);
		if (data == 0)
		{
			trace((TraceLevel)50, "nullShape t%d f0!\n\r", shape);
			shape = 0;
			get(0, 0);
		}
	}
	ShapeIndex *index = (ShapeIndex *)CacheNodePtr::getPtr(handles[shape]);
	maxWidth = index->maxWidth;
	maxHeight = index->maxHeight;
	numFrames = index->numFrames;
}

void ShapeHandler::getRect(uint16_t shape, uint16_t frame, Rect &rect, int16_t x, int16_t y)
{
	int16_t width, height, xoff, yoff;
	getDim(shape, frame, width, height, xoff, yoff);
	rect.f_00 = x - xoff;
	rect.f_02 = y - yoff;
	rect.f_04 = rect.f_00 + width - 1;
	rect.f_06 = rect.f_02 + height - 1;
}

// Reads one frame into the cache and points the shape's frame table at it.
char *ShapeHandler::loadFrame(uint16_t shape, uint16_t frame, uint32_t shapeOffset, uint32_t frameOffset)
{
	ShapeIndex *index = (ShapeIndex *)CacheNodePtr::getPtr(shapeOffset);
	int32_t size = index->frames[frame].size;
	if (size == 0)
	{
		index->frames[frame].offset = 0;
		return 0;
	}
	CacheNodePtr(handles[shape] - NODE_HEADER).makeMru();
	uint32_t node = CacheNodePtr::allocate(size, NO_HANDLE, (CacheNodeType)4);
	if (node == 0 || handles[shape] == 0)
		halt(__FILE__, 573);	// __LINE__
	CacheNodePtr(node - NODE_HEADER).makeMru();
	CacheNodePtr(handles[shape] - NODE_HEADER).makeMru();
	Index fileIndex;
	shapeFile->getIndex(shape, fileIndex);
	int32_t diskOffset = frameOffset & ~FRAME_ON_DISK;
	char *data = (char *)CacheNodePtr::getPtr(node);
	shapeFile->readRecord(shape, data, size, diskOffset);
	index = (ShapeIndex *)CacheNodePtr::getPtr(handles[shape]);
	((FrameData *)data)->oldOffset = index->frames[frame].offset;
	index->frames[frame].offset = node;
	((FrameData *)data)->shape = shape;
	((FrameData *)data)->frame = frame;
	return data;
}

// Returns a frame's header, loading the shape's frame table and the
// frame itself as needed.
char *ShapeHandler::get(uint16_t shape, uint16_t frame)
{
	uint32_t shapeOffset = handles[shape];
	if (shapeOffset)
	{
		ShapeIndex *index = (ShapeIndex *)CacheNodePtr::getPtr(shapeOffset);
		if (index->numFrames <= frame)
			return 0;
		uint32_t frameOffset = index->frames[frame].offset;
		if (frameOffset == 0)
			return 0;
		frameOffset &= ~FRAME_FLAG;
		if (frameOffset & FRAME_ON_DISK)
		{
			char *data = loadFrame(shape, frame, shapeOffset, frameOffset);
			if (data)
				data += sizeof(FrameData);
			return data;
		}
		if (frameOffset == 0 || shapeOffset == 0)
			halt(__FILE__, 631);	// __LINE__
		CacheNodePtr(frameOffset - NODE_HEADER).makeMru();
		CacheNodePtr(shapeOffset - NODE_HEADER).makeMru();
		return (char *)CacheNodePtr::getPtr(frameOffset + sizeof(FrameData));
	}
	else
	{
		Index fileIndex;
		shapeFile->getIndex(shape, fileIndex);
		if (isEmpty(fileIndex))
			return 0;
		ShapeHeader header;
		shapeFile->readRecord(shape, &header, sizeof(ShapeHeader), 0);
		int16_t size = header.numFrames * sizeof(FrameSlot) + sizeof(ShapeHeader);
		int32_t node = CacheNodePtr::allocate(size, shape, (CacheNodeType)0);
		ShapeIndex *index = (ShapeIndex *)CacheNodePtr::getPtr(node);
		memcpy(index, &header, sizeof(ShapeHeader));
		size -= sizeof(ShapeHeader);
		shapeFile->readRecord(shape, index->frames, size, sizeof(ShapeHeader));
		FrameSlot *entry = index->frames;
		for (uint16_t i = 0; i < header.numFrames; i++)
			(entry++)->offset |= FRAME_ON_DISK;
		if (index->numFrames <= frame)
			return 0;
		uint32_t frameOffset = index->frames[frame].offset;
		char *data = loadFrame(shape, frame, node, frameOffset);
		if (data)
			data += sizeof(FrameData);
		return data;
	}
}

// Returns a frame's header and its size on disk.
char *ShapeHandler::get(uint16_t shape, uint16_t frame, uint16_t &size)
{
	size = 0;
	char *data = get(shape, frame);
	if (data == 0)
		return 0;
	ShapeIndex *index = (ShapeIndex *)CacheNodePtr::getPtr(handles[shape]);
	size = index->frames[frame].size;
	return data;
}

void ShapeHandler::cacheOut(uint32_t, uint16_t)
{
}

// Checks that every loaded frame of the shape at this node names the
// shape and frame it belongs to.
uint8_t ShapeHandler::verify(uint32_t nodeOffset)
{
	CacheNode *node = (CacheNode *)CacheNodePtr::getPtr(nodeOffset);
	uint16_t shape = node->handle;
	ShapeIndex *index = (ShapeIndex *)CacheNodePtr::getPtr(nodeOffset + NODE_HEADER);
	uint16_t numFrames = index->numFrames;
	for (uint16_t i = 0; i < numFrames; i++)
	{
		index = (ShapeIndex *)CacheNodePtr::getPtr(nodeOffset + NODE_HEADER);
		uint32_t frameOffset = index->frames[i].offset;
		frameOffset &= ~FRAME_FLAG;
		if (!(frameOffset & FRAME_ON_DISK))
		{
			FrameData *data = (FrameData *)CacheNodePtr::getPtr(frameOffset);
			if (data->shape != shape || data->frame != i)
				return 0;
		}
	}
	return 1;
}

void ShapeHandler::flushType(uint16_t shape)
{
	uint32_t shapeOffset = handles[shape];
	if (shapeOffset == 0)
		return;
	ShapeIndex *index = (ShapeIndex *)CacheNodePtr::getPtr(shapeOffset);
	uint16_t numFrames = index->numFrames;
	for (uint16_t i = 0; i < numFrames; i++)
	{
		index = (ShapeIndex *)CacheNodePtr::getPtr(shapeOffset);
		if (!(index->frames[i].offset & FRAME_ON_DISK))
			flushFrame(shapeOffset, i);
	}
	CacheNodePtr(shapeOffset - NODE_HEADER).free();
}

void ShapeHandler::flushFrame(uint32_t shapeOffset, uint16_t frame)
{
	ShapeIndex *index = (ShapeIndex *)CacheNodePtr::getPtr(shapeOffset);
	if (index->numFrames <= frame)
		return;
	uint32_t frameOffset = index->frames[frame].offset;
	if (frameOffset == 0)
		return;
	if (frameOffset < 0x400)
		halt(__FILE__, 792);	// __LINE__
	frameOffset &= ~FRAME_FLAG;
	if (!(frameOffset & FRAME_ON_DISK))
	{
		CacheNodePtr node(frameOffset - NODE_HEADER);
		CacheNodePtr::deallocate(node);
	}
}

FrameHandler::FrameHandler(void) :
	CacheHandler((CacheNodeType)4)
{
	CacheNodePtr::attach(this, (CacheNodeType)4);
}

// A frame moved within the cache: repoint its frame table entry.
void FrameHandler::notifyMoved(uint32_t offset)
{
	FrameData *data = (FrameData *)CacheNodePtr::getPtr(offset);
	uint32_t shapeOffset = handles[data->shape];
	if (shapeOffset == 0)
		halt(__FILE__, 986);	// __LINE__
	ShapeIndex *index = (ShapeIndex *)CacheNodePtr::getPtr(shapeOffset);
	uint32_t flag = index->frames[data->frame].offset & FRAME_FLAG;
	index->frames[data->frame].offset = offset | flag;
}

// A frame left the cache: put back the table entry it replaced.
void FrameHandler::cacheOut(uint32_t offset, uint16_t)
{
	FrameData *data = (FrameData *)CacheNodePtr::getPtr(offset);
	uint32_t shapeOffset = handles[data->shape];
	if (shapeOffset == 0)
		halt(__FILE__, 999);	// __LINE__
	ShapeIndex *index = (ShapeIndex *)CacheNodePtr::getPtr(shapeOffset);
	index->frames[data->frame].offset = data->oldOffset;
}

void initShapeHandler(void)
{
	shapeHandler = new ShapeHandler;
	frameHandler = new FrameHandler;
	if (shapeHandler == 0 || frameHandler == 0)
		outOfMemory(__FILE__, 1016);	// __LINE__
}

void uninitShapeHandler(void)
{
	delete shapeHandler;
}
