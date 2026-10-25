#ifndef SCRIT_H
#define SCRIT_H

#include "u8port.h"

class SearchCriteria
{
public:
	uint8_t data[32];

	SearchCriteria(int, ...);	// int: va_start needs a promoted type
	int16_t cmdlen(int16_t);
	void operator+=(SearchCriteria);
	operator char *(void) { return (char *)data; }
};

#endif
