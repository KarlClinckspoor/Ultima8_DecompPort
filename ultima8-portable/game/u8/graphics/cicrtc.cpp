// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\CICRTC.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
#include "cicrtc.h"

uint16_t CrtcStatusPort;
uint16_t CrtcAddressRegister;
uint16_t CrtcDataRegister;
uint16_t BiosSegment;

void InitializeCrtc(void)
{
	uint16_t *crtcPort = (uint16_t *)MK_FP(BiosSegment, 0x63);
	CrtcAddressRegister = *crtcPort;
	CrtcDataRegister = CrtcAddressRegister + 1;
	CrtcStatusPort = CrtcDataRegister + 5;
}
