#ifndef CMEMHNDP_H
#define CMEMHNDP_H

#include "u8port.h"

class MemHandle
{
public:
	uint32_t f_00;
	uint8_t f_04;
	uint32_t f_05;

	MemHandle(void);
	MemHandle(char *);
	MemHandle(uint32_t, uint8_t);
	~MemHandle(void);
	void alloc(uint32_t, uint8_t);
	void alloc(uint32_t, uint8_t, int16_t);
	void alloc(uint32_t, uint8_t, char *);
	void free(int16_t);
	void copy(MemHandle &);
	char *get_ptr(void);
	void init(char *, uint8_t);
	void init(MemHandle &);
	void take_ownership(MemHandle &);
	int16_t is_valid(void);
};

#endif
