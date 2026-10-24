// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ANODE.C

#include "u8port.h"

// #include <mem.h>  (DOS only)
#include "anode.h"

void AnimNode::initialize(void)
{
	memset(this, 0, sizeof(AnimNode));
}

void AnimNode::operator=(AnimNode &node)
{
	memcpy(this, &node, sizeof(AnimNode));
}
