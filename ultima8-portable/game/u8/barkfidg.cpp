// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: BARKFIDG.C

#include "u8port.h"

#include "npc.h"
#include "kernel.h"
#include "dispatch.h"
#include "bark.h"
#include "statrest.h"
#include "wait.h"
#include "barkfidg.h"

BarkFidget::BarkFidget(uint16_t npc, uint8_t x)
{
	setRef(npc);
	setProcessType(0x227);
	Kernel::setIdString(pid, "BarkFidget");
	f_2e = x;
	intFlags |= 1;
}

Boolean BarkFidget::hasBark(void)
{
	NewGump *top = Dispatcher::base[Dispatcher::baseSP];
	BarkGump *bark = 0;
	while (top->newGumpList.hasType(0x43, (NewGump *&)bark))
		if (bark->f_52 == 2 && bark->item == ref)
			return TRUE;
	return FALSE;
}

void BarkFidget::process(void)
{
	Npc npc(ref);
	if (npc.getLastAnimSet() != ANIM_STAND && npc.getLastAnimSet() != ANIM_TALK)
	{
		pop(0);
		return;
	}
	if (npc.isBusy())
		return;
	if (hasBark())
	{
		int16_t type = npc.getType();
		int16_t animPid = 0;
		if (AnimCache::isAnimInExistence(type, ANIM_TALK))
			animPid = npc.doAnim(ANIM_TALK, 8, 1, 0);
		if (animPid)
		{
			Process *anim;
			Wait *wait = new Wait(rndRange(1, 3) * 30, ref);
			anim = Kernel::getProcess(animPid);
			anim->then(wait)->then(this);
			wait->notifyOfDeath(pid);
			wait->start();
			return;
		}
		pop(0);
		return;
	}
	pop(0);
}

void BarkFidget::interruptHandler(uint32_t, uint32_t)
{
	if (dependencyFailed())
		pop(0);
}
