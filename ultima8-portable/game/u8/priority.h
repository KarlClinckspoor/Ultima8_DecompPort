#ifndef PRIORITY_H
#define PRIORITY_H

#include "u8port.h"

// A process priority test: a level in the low 5 bits and which of
// less (0x40), equal (0x20) and greater (0x80) match in the high 3.
class PriorityClass
{
public:
	uint8_t valid;

	PriorityClass(uint8_t v) { valid = v; }
	uint8_t isValid(void);
	uint8_t contains(uint8_t);
};

#endif
