// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\XCACHE\FLEXHAND.C

#include "u8port.h"

#include "cexit.h"
#include "flexhand.h"

U8_INLINE Index::Index(void) { size = 0; offset = 0; }

FlexShapeHandler::FlexShapeHandler(int16_t numHandles, CacheNodeType type, const char *name) :
	CacheHandler(type)
{
	startHandle = reserve(numHandles);
	CacheNodePtr::attach(this, type);
	flexFile = new FlexFile((char *)name, (OpenMode)0, -1);
	if (flexFile == 0)
		outOfMemory(__FILE__, 16);	// __LINE__
}

FlexShapeHandler::~FlexShapeHandler(void)
{
	delete flexFile;
}

char *FlexShapeHandler::get(uint16_t handle)
{
	uint32_t offset = handles[handle];
	if (offset)
		CacheNodePtr(offset - NODE_HEADER).makeMru();
	else
	{
		Index index;
		int16_t record = handle - startHandle;
		flexFile->getIndex(record, index);
		if (index.size == 0)
			halt(__FILE__, 45);	// __LINE__
		int32_t node = CacheNodePtr::allocate(index.size, handle, (CacheNodeType)1);
		void *data = CacheNodePtr::getPtr(node);
		flexFile->readRecord(record, data, -1, 0);
	}
	return (char *)CacheNodePtr::getPtr(handles[handle]);
}
