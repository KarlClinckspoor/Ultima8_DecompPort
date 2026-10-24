#ifndef ASTREAM_H
#define ASTREAM_H

#include "u8port.h"

class AnimNode;

// The 4-byte header of one animation in anim.dat.
class AnimStreamHeader
{
public:
	uint16_t frames : 8;
	uint16_t twoStep : 1;
	uint16_t attack : 1;
	uint16_t looping : 1;
	uint16_t unstoppable : 1;
	uint16_t looping2 : 1;
	uint16_t flag20 : 1;
	uint16_t flag40 : 1;
	uint16_t hanging : 1;
	uint16_t repeat : 4;
	uint16_t unk4 : 6;
	uint16_t unk10 : 5;
	uint16_t destroyActor : 1;

	AnimStreamHeader(void);
	uint8_t getFrames(void) { return frames; }
};

// One animation: its header and its frames for all 8 directions.
class AnimStream
{
public:
	AnimNode *nodes;
	AnimStreamHeader header;

	AnimStream(void) { nodes = 0; }
	~AnimStream(void) { free(); }
	void allocate(void);
	void setAnimNode(char, uint8_t, AnimNode *);
	void free(void);
};

#endif
