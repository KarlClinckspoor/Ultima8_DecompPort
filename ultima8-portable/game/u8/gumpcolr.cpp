// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d
// name: GUMPCOLR.C

#include "u8port.h"

// Colour remap for gump shapes; GumpColorMap selects the active table.
uint8_t defaultGumpColorMap[32] = {0, 131, 25, 22, 100, 19, 53, 255, 15, 69, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint8_t *GumpColorMap = defaultGumpColorMap;
