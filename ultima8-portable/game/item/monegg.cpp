// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\ITEM\MONEGG.C

#include "u8port.h"

#include "npc.h"
#include "type.h"
#include "motrak.h"
#include "statrest.h"
#include "monegg.h"

// Hatch chance in percent, by probability index.
uint8_t chances[8] = {5, 10, 20, 35, 50, 65, 80, 100};

void MonsterEgg::hatch(void)
{
	Npc npc(0);
	int16_t count = getNumMonsters();
	int16_t shape = getShapeType();
	int16_t x = getX();
	int16_t y = getY();
	int16_t z = getZ();
	int16_t nx, ny;
	int16_t prob = getProb();
	int16_t i, j, k;

	if (urandom(100) >= chances[prob])
		return;
	if (count == 0)
		return;
	if (npc.create(shape, 0))
	{
		npc.pop(x, y, z);
		npc.cSetActivity((Activity)getActivity());
		npc.setStatus(npc.getStatus() | 0x80);
	}
	// Place the rest on the first free spot of a 4x4 grid around the egg.
	for (i = 1; i < count; i++)
		for (j = 0; j < 4; j++)
		{
			nx = x + j * 32 * 2 - 128;
			for (k = 0; k < 4; k++)
			{
				ny = y + k * 32 * 2 - 128;
				if (canExistAt(shape, nx, ny, z, 1, 0, 0))
				{
					if (npc.create(shape, 0))
					{
						npc.pop(nx, ny, z);
						npc.cSetActivity((Activity)getActivity());
						npc.setStatus(npc.getStatus() | 0x80);
						k = 4;
						j = 4;
					}
					else
						return;
				}
			}
		}
}

int16_t MonsterEgg::getMonId(void)
{
	return getMapArray() >> 3;
}

void MonsterEgg::setMonId(int16_t id)
{
	int16_t v = getMapArray() & 7;
	v |= id << 3;
	setMapArray(v);
}

int16_t MonsterEgg::getActivity(void)
{
	return getMapArray() & 7;
}

void MonsterEgg::setActivity(int16_t activity)
{
	int16_t v = getMapArray() & 0xf8;
	v |= activity;
	setMapArray(v);
}

int16_t MonsterEgg::getShapeType(void)
{
	return getQ() & 0x7ff;
}

void MonsterEgg::setShapeType(int16_t shape)
{
	int16_t v = getQ() & 0xf800;
	v |= shape & 0x7ff;
	setQ(v);
}

int16_t MonsterEgg::getNumMonsters(void)
{
	return getNpcArray() & 7;
}

void MonsterEgg::setNumMonsters(int16_t count)
{
	int16_t v = getNpcArray() & 0xf8;
	v |= count;
	setNpcArray(v);
}

int16_t MonsterEgg::getProb(void)
{
	int16_t prob = getQ() >> 11;
	return prob;
}

void MonsterEgg::setProb(int16_t prob)
{
	int16_t v = getQ() & 0x7ff;
	v |= prob << 11;
	setQ(v);
}
