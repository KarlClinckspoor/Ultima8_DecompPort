#ifndef FLEXHAND_H
#define FLEXHAND_H

#include "u8port.h"

#include "flex.h"
#include "handler.h"

class FlexShapeHandler : public CacheHandler
{
public:
	uint16_t startHandle;
	FlexFile *flexFile;

	FlexShapeHandler(int16_t, CacheNodeType, const char *);
	~FlexShapeHandler(void);
	char *get(uint16_t);
};

#endif
