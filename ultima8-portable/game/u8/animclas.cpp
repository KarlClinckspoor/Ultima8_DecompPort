// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ANIMCLAS.C

#include "u8port.h"

#include "npc.h"
#include "scratchm.h"
#include "animcach.h"
#include "anode.h"
#include "astream.h"
#include "animclas.h"

StandingLeap::StandingLeap(uint16_t npc, char d, WorldPoint *point) :
	AnimPrimitive(npc, ANIM_JUMP, d, 0, 0, 0)
{
	totalDist = 0;
	zDiff = 0;
	jumpFrames = 0;
	if (npc == 1)
	{
		start = avatar.getLoc();
		jumpFrames = AnimCache::getAnimStreamHeader(1, ANIM_JUMP).frames;
		if (point)
		{
			target = *point;
			zDiff = target.z - avatar.getZ();
		}
		AnimNode node;
		for (int16_t i = 0; i < jumpFrames; i++)
		{
			AnimCache::getNode(1, ANIM_JUMP, avatar.getDir(), i, &node);
			totalDist += node.deltaDir * 4;
		}
	}
}

// Spreads the move from start to target over the frames, and the height
// difference over the frames after the fifth.
WorldPoint StandingLeap::getPoint(WorldPoint point, char d, AnimNode node)
{
	int16_t step = node.deltaDir * 4;
	int16_t dx;
	int16_t dy;
	char dz;

	if (!totalDist)
	{
		dx = x8add[d] * step;
		dy = y8add[d] * step;
	}
	else
	{
		int32_t x = ((int32_t)target.x - start.x) * step / totalDist;
		int32_t y = ((int32_t)target.y - start.y) * step / totalDist;
		int16_t frame = Npc::npcData[avatar.referent].animFrame;
		int32_t z = node.deltaZ;
		if (frame > 5)
			z += (int32_t)zDiff / (jumpFrames - 5);
		dx = x;
		dy = y;
		dz = z;
	}
	WorldPoint result(point.x + dx, point.y + dy, point.z + dz);
	return result;
}
