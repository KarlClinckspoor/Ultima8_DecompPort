// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GRAPHICS\INITSYS.C

#include "u8port.h"

// #include <dos.h>  (DOS only)
// #include <phapi.h>  (DOS only)
#include "cexit.h"
#include "cicrtc.h"
#include "cvport.h"
#include "cdesc.h"

uint16_t TextSelector;

void GetBiosSegment(void)
{
	if (DosGetBIOSSeg((SEL *)&BiosSegment))
		Fatal("Failed to get Bios Segment Descriptor!");
}

void SetVgaScreenDescriptor(void)
{
	if ((VgaScreenDescriptor = MapSegment(0xA000, 0xFA00)) == 0)
		Fatal("Failed to get Screen Descriptor.");
	if (DosMapRealSeg(0xB800, 8000, &TextSelector))
		Fatal("Failed to get Screen Descriptor.");
}

uint16_t MapSegment(uint16_t segment, uint16_t size)
{
	uint16_t sel;

	return DosMapRealSeg(segment, size, &sel) ? 0 : sel;
}
