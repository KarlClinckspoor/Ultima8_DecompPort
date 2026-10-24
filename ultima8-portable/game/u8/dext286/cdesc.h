#ifndef CDESC_H
#define CDESC_H

#include "u8port.h"

#include "cytable.h"

// The DOS build kept its descriptor-table classes (Desc, LocalDescriptorTable,
// GlobalDescriptorTable, HoldPointers) and its selector helpers here. With no selectors on
// the host only the screen helpers remain.

// The video BIOS data area once gave the CRTC port; the VGA's is fixed now.
void GetBiosSegment(void);
// Points VgaScreen at the screen memory the platform shows.
void SetVgaScreenDescriptor(void);
// Blends two byte arrays (cpalette's fades): see GRAPHICS\MERGEARY.C.
int16_t MERGEBYTEARRAY(char *, char *, int16_t);
// Its state (init, error, delta, vector, major, iterations in DOS).
extern int16_t mergeInit;
extern char *mergeError;
extern char *mergeDelta;
extern char *mergeVector;
extern int16_t mergeMajor;
extern int16_t mergeIterations;

// The colour text screen, B800:0000: 80x25 character and attribute pairs. The debug gumps
// draw into it; nothing shows it on the host.
extern uint8_t *TextScreen;

#endif
