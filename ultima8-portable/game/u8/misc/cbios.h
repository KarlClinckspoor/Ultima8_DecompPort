#ifndef CBIOS_H
#define CBIOS_H

#include "u8port.h"

struct CursorPosition
{
	char row;
	char column;
};

// Text-mode scan line counts for BIOS video function 12h/30h.
enum ScanCodes
{
	SCANS_200,
	SCANS_350,
	SCANS_400
};

void conputs(const char *);
int16_t congetkey(void);
void congetcursor(CursorPosition &, char);
void consetcursor(CursorPosition &, char);
ScanCodes congetscans(void);
void consetscans(ScanCodes);
void dosputs(const char *);

#endif
