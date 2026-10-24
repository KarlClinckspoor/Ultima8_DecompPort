#ifndef ERROR_H
#define ERROR_H

#include "u8port.h"

enum TraceLevel : int16_t
{
	TraceLevel_0,
	TRACE_ALWAYS = 255
};

void halt(char *, int16_t);
void halt(char *, int16_t, char *, ...);
void outOfMemory(char *, int16_t);
void outOfMemory(char *, int16_t, char *, ...);
void unableToOpenFile(char *, int16_t, char *);
void trace(TraceLevel, char *, ...);
void trace(TraceLevel, int16_t, int16_t, char *, ...);
void traceGet(TraceLevel, char *, ...);
void traceGet(TraceLevel, int16_t, int16_t, char *, ...);

extern int16_t traceLevel;

#endif
