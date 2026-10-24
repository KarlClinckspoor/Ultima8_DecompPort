#ifndef SYSTEM_H
#define SYSTEM_H

#include "u8port.h"

class SimpleVirtualString
{
public:
	char *string;
	int16_t size;

	SimpleVirtualString(void);
	SimpleVirtualString(int16_t);
	~SimpleVirtualString(void);
	void free(void);
	char *alloc(int16_t);
	void concatenate(const char *);
	void empty(void);
	char *operator=(const char *);
	char *operator+=(const char *);
	void swap(SimpleVirtualString &);
	char *assemble(const char *, ...);
	int16_t operator--(void);
	operator char *(void);
};

extern SimpleVirtualString WorkString;

// Prints a caller's arguments into WorkString, or empties it.
#define FORMAT_WORKSTRING(format, args) \
	if (format && *format && format != (char *)WorkString) \
	{ \
		va_start(args, format); \
		vsprintf(WorkString, format, args); \
	} \
	else \
		*(char *)WorkString = 0

#endif
