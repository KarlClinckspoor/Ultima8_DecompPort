// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\CICRTC.C

#include "u8port.h"

#include "cicrtc.h"

uint16_t CrtcStatusPort;
uint16_t CrtcAddressRegister;
uint16_t CrtcDataRegister;
uint16_t BiosSegment;

// The CRTC's address port came from the word at 0040:0063 of the BIOS data area: 3D4h on a
// colour VGA. The ports are the VGA's own now.
void InitializeCrtc(void)
{
	CrtcAddressRegister = 0x3D4;
	CrtcDataRegister = CrtcAddressRegister + 1;
	CrtcStatusPort = CrtcDataRegister + 5;
}
