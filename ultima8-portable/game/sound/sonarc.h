#ifndef SONARC_H
#define SONARC_H

#include "u8port.h"

void incPtr(char *, int16_t);
int32_t decompress(char *, int32_t, char *, int16_t);

extern "C" int16_t DCMP8(char *, char *);

#endif
