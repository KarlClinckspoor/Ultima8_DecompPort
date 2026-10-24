#ifndef ANODE_H
#define ANODE_H

#include "u8port.h"

// One frame of an animation in one direction.
class AnimNode
{
public:
	uint16_t frame : 11;
	uint16_t special : 1;		// run the type's special animation code
	uint16_t flagsHi : 4;
	int16_t deltaZ : 8;
	uint16_t sfx : 8;
	int16_t deltaDir : 9;			// distance moved, in steps of 4 world units
	uint16_t onGround : 1;
	uint16_t attackRange : 3;
	uint16_t flipped : 1;
	uint16_t callUsecode : 1;
	uint16_t flag80 : 1;

	void initialize(void);
	void operator=(AnimNode &);
};

#endif
