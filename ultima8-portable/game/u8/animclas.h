#ifndef ANIMCLAS_H
#define ANIMCLAS_H

#include "u8port.h"

#include "animprim.h"

// The Avatar's jump, aimed so its frames carry it from start to target.
class StandingLeap : public AnimPrimitive
{
public:
	int16_t totalDist;		// ground covered by all the frames together
	int16_t zDiff;
	int16_t jumpFrames;
	WorldPoint target;
	WorldPoint start;

	StandingLeap(uint16_t, char, WorldPoint *);
	virtual WorldPoint getPoint(WorldPoint, char, AnimNode);
};

#endif
