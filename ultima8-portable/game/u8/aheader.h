#ifndef AHEADER_H
#define AHEADER_H

#include "u8port.h"

#include "item.h"
#include "animcach.h"

// Offsets of a shape's animations in anim.dat, one per AnimSet.
class AnimHeader
{
public:
	uint32_t data[64];

	AnimHeader(void) { clear(); }
	void clear(void);
	Boolean hasAnim(AnimSet anim) { return data[anim] != 0; }
	void setAnim(AnimSet anim, int32_t offset) { data[anim] = offset; }
	void clearAnim(AnimSet anim) { data[anim] = 0; }
	Boolean isData(void);
};

#endif
