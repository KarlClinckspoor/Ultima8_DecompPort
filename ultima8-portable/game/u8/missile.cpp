// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: MISSILE.C

#include "u8port.h"

#include <stdlib.h>
#include "item.h"
#include "type.h"
#include "gravity.h"
#include "missile.h"

MissileTracker::MissileTracker(WorldPoint from, WorldPoint to, int16_t speed, int16_t gravity)
{
	int16_t frames;
	int16_t dx, dy, dz;

	this->gravity = gravity;
	if (this->gravity < 0)
		this->gravity = 0;
	if (this->gravity == 0)
		maxRange = 3000;
	else
		maxRange = speed * (speed * 2 / this->gravity);

	dx = to.x - from.x;
	dy = to.y - from.y;
	dz = to.z - from.z;
	range = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
	range += abs(dx) + abs(dy);
	range >>= 1;

	frames = (range + (speed >> 1)) / speed;
	if (frames)
	{
		speedZ = (dz + (this->gravity * frames * (frames - 1) >> 1)) / frames;
		// Too steep: take more frames.
		if (speedZ > speed >> 2)
		{
			if (this->gravity && (speed >> 2) / this->gravity > frames)
				frames = (speed >> 2) / this->gravity;
			else if (speed > 3 && dz / (speed >> 2) > frames)
				frames = dz / (speed >> 2);
			speedZ = (dz + (this->gravity * frames * (frames - 1) >> 1)) / frames;
		}
		speedX = (dx + (frames >> 1)) / frames;
		speedY = (dy + (frames >> 1)) / frames;
	}
	else
	{
		speedX = speedY = 0;
		speedZ = dz >= 0 ? speed >> 2 : -(speed >> 2);
	}
	// BCC compiles this compare against the BP register instead of its speed >> 2 temp.
	inRange = speedZ <= speed >> 2 && range <= maxRange;
}

void MissileTracker::fire(uint16_t item)
{
	Item(item).hurl(speedX, speedY, speedZ, gravity);
}

// Throws a test copy of the flight and checks it lands within 50 of target.
uint8_t MissileTracker::isPathClear(uint16_t item, WorldPoint target)
{
	Process *p = Kernel::findValidProcess(item, (ProcessType)0x203);

	if (p)
		p->fail(0);
	p = new GravityTracker(item, speedX, speedY, speedZ, gravity, 200, &target);
	p->f_10 |= 4;
	p->process();
	WorldPoint loc = Item(item).getLoc();
	int16_t distance = abs(loc.z - target.z) + (abs(loc.x - target.x) + abs(loc.y - target.y));
	return distance < 50;
}
