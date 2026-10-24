#ifndef FLAG_H
#define FLAG_H

#include "u8port.h"

// Usecode global flags: a 1024-byte bit array saved as flag.dat, with
// flagsym.dat naming each flag's bit position and width.
class GlobalFlag
{
public:
	static int16_t get(int16_t, int16_t);
	static void set(int16_t, int16_t, int16_t);
	static void destroy(void);
	static void load(char *);
	static void save(char *);
	static char findFlag(char *, int16_t &, int16_t &);
	static char *flagSym;
	static int16_t flagSymSize;
	static char *bits;
};

extern char *flagFileName;

#endif
