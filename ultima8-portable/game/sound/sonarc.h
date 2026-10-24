#ifndef SONARC_H
#define SONARC_H

#include "u8port.h"

// Advances ptr by offset bytes. It moved the base of ptr's selector in the DOS build, so
// the pointer's value stayed the same; with plain pointers it moves the pointer itself.
void incPtr(char *&, int16_t);
int32_t decompress(char *, int32_t, char *, int16_t);

// DCMP8.ASM (game/sound/dcmp8.cpp): decodes one Sonarc block.
int16_t DCMP8(char *, char *);

#endif
