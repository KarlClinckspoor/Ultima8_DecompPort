#ifndef OCCLUDE_H
#define OCCLUDE_H

#include "u8port.h"

// Highest roof z over each 32-unit cell of the 9x9 loaded regions.
#define ZBUFFER_SIZE	144

class Zbuffer
{
public:
	uint8_t (*buffer)[ZBUFFER_SIZE];

	Zbuffer(void);
	~Zbuffer(void);
	void alloc(void);
	void init(void);
	void make(void);
	uint8_t isHidden(uint16_t);
};

#endif
