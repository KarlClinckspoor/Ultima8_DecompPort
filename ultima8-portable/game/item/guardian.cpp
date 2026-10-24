// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\ITEM\GUARDIAN.C

#include "u8port.h"

#include <string.h>
// #include <mem.h>  (DOS only)
#include "cache.h"
#include "item.h"
#include "itemcach.h"
#include "glob.h"
#include "shaphand.h"
#include "cfile.h"
#include "fexist.h"
#include "filespec.h"
#include "cexit.h"
#include "scratchm.h"
#include "statrest.h"
#include "uprocess.h"
#include "type.h"


uint8_t WorldPoint::operator==(WorldPoint &p)
{
	if (x == p.x && y == p.y && z == p.z)
		return TRUE;
	return FALSE;
}

#define GUARDIAN_BARK_EVENT	0x15

// Runs the Avatar's guardian bark usecode one time in three.
int16_t guardianBark(int16_t num)
{
	uint16_t pid;
	if (urandom(3) > 0)
		return 0;
	Item avatar(1);
	int32_t arg = num;
	if (spawnUnk(&avatar, 1L << GUARDIAN_BARK_EVENT, GUARDIAN_BARK_EVENT, pid, arg))
		return pid;
	return 0;
}
