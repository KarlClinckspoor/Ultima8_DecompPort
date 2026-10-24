#ifndef FERROR_H
#define FERROR_H

#include "u8port.h"

void Fatal(const char *, ...);
void Fatal(int16_t);
void Fatal(int16_t, int16_t);

extern char *ExitMessage;
extern char *ErrorGroupMessage;
extern char *ErrorSubgroupMessage;

#endif
