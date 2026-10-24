// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: GRANTPEA.C

#include "u8port.h"

#include <stdlib.h>
#include "kernel.h"
#include "npc.h"
#include "type.h"
#include "itemfind.h"
#include "motrak.h"
#include "targgump.h"
#include "dispatch.h"
#include "pal.h"
#include "dsfxman.h"
#include "sprite.h"
#include "statrest.h"
#include "scratchm.h"
#include "combatbr.h"
#include "grantpea.h"

// One monster type's 13-byte record in DataTable::data.
struct MonsterRecord
{
	uint8_t pad_00[9];
	uint16_t damageType;
	uint16_t defenseType;
};

#define monsterRecord	((MonsterRecord *)DataTable::data)
#define DEFENSE_UNDEAD	0x10

// Puts a new npc of a type at a free spot near the avatar.
void createNpc(uint16_t type)
{
	Npc npc(0);
	int16_t tries;
	int16_t x, y;
	uint8_t z;

	for (tries = 0; tries < 10; tries++)
	{
		x = avatar.getX() + rndRange(-250, 250);
		y = avatar.getY() + rndRange(-250, 250);
		z = avatar.getZ();
		if (canExistAt(type, x, y, z, 1, 0, 0))
			break;
	}
	if (tries != 10 && npc.create(type, 0))
	{
		npc.pop(x, y, z);
		npc.setStatus(npc.getStatus() | 0x80);
	}
}

GrantPeaceSpell::GrantPeaceSpell(void)
{
	Kernel::setIdString(pid, "GrantPeaceSpell");
	setProcessType((ProcessType)0x21d);
	setRef(1);
}

// Kills the target, or every undead of its kind near the avatar. Undead of type 411
// near a type 289 item switch it to frame 1 with a sprite on top.
void GrantPeaceSpell::process(void)
{
	uint8_t hit = 0;
	Npc target(0);
	int16_t monster;
	int16_t undeadType = 0;
	char dir;

	Dispatch(new TargetGump(&target, 0));
	if (target.isNpc())
	{
		if (!target.isAvatar())
		{
			monster = DataTable::getMonsterRecord(target.getType());
			if (monster && (monsterRecord[monster].defenseType & DEFENSE_UNDEAD))
				undeadType = target.getType();
			if (undeadType)
			{
				AreaItemFinder finder;
				AreaItemFinder khumash;

				khumash.init(avatar.getX() - 0x800U, avatar.getY() - 0x800U,
				              avatar.getX() + 0x800, avatar.getY() + 0x800, "$", 1);
				while (khumash.isValid())
				{
					if (khumash.Item::getType() == 289)
						break;
					khumash.findNext();
				}
				finder.init(avatar.getX() - 0x300U, avatar.getY() - 0x300U,
				            avatar.getX() + 0x300, avatar.getY() + 0x300, "$", 1);
				while (finder.isValid())
				{
					if (finder.isNpc())
					{
						Npc npc(finder.referent);
						if (!npc.isDead() && npc.getType() == undeadType && monster &&
						    (monsterRecord[monster].defenseType & DEFENSE_UNDEAD))
						{
							dir = avatar.getDirToItem(npc.referent);
							npc.receiveHit(1, 8, npc.getHp(), 0xa8);
							if (undeadType == 411 && khumash.isValid())
							{
								khumash.setFrame(1);
								createSprite(480, 0, 9, 9, 1, 1, khumash.getX() + 14,
								             khumash.getY() + 14, khumash.getZ() + 8);
								npc.hurl(x8add[dir] * rndRange(5, 25), y8add[dir] * rndRange(5, 25),
								         rndRange(10, 25), 2);
							}
							else
								npc.hurl(x8add[dir] * rndRange(5, 10), y8add[dir] * rndRange(5, 10),
								         rndRange(5, 10), 2);
							hit = 1;
						}
					}
					finder.findNext();
				}
			}
			else if (!target.isDead() && !target.isImmortal() && !target.isInvincible() && OneIn(10))
			{
				target.receiveHit(1, 8, target.getHp(), 0xa8);
				hit = 1;
			}
			if (hit)
			{
				int16_t sounds[3] = {91, 94, 96};
				int16_t sound = sounds[(int32_t)rand() * 3 / 0x8000L];
				LightningBolt();
				playSFX(sound, 250, 1);
			}
		}
	}
	pop(0);
}

void castGrantPeaceSpell(void)
{
	Process *spell = new GrantPeaceSpell;
	avatar.doAnim((AnimSet)0x1b, 8, 10000, 1);
	Process *cast = Kernel::getProcess(avatar.doAnim((AnimSet)0x1d, 8, 1, 0));
	avatar.doAnim((AnimSet)0x1c, 8, 2, 0);
	cast->then(spell);
}
