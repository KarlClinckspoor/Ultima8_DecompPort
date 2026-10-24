// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: FANIM.C

#include "u8port.h"

#include "animcach.h"
#include "wpncache.h"

void initAnimCache(void)
{
	AnimCache::init();
}

void uninitAnimCache(void)
{
	AnimCache::uninit();
}
