// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: STATREST.C

#include "u8port.h"

#include "u8process.h"
#include "kernel.h"
#include "npc.h"
#include "main.h"
#include "statrest.h"

inline void Avatar::setEffectiveStr(int16_t s) { effectiveStr = s; }

StatRestorer::StatRestorer(void)
{
	Kernel::setIdString(pid, "StatRestorer");
	setProcessType((ProcessType)0x222);
	setHertz(1);
	flags |= PROC_SUSPENDED;
	setDaemon();
	ticks = 0;
	intFlags |= 2;
	connectToRealTime(1);
}

// Once a second: mana every 20 seconds; hit points and hunger every 35.
void StatRestorer::process(void)
{
	int16_t value;
	uint8_t current, max;

	if (!inGameMode)
		return;
	if (ticks % 20 == 0)
	{
		current = avatar.getMana();
		max = avatar.getInt();
		if ((value = current) < max * 2)
			avatar.setMana(value + 1);
	}
	if (ticks == 35)
	{
		current = avatar.getHp();
		max = avatar.getStr();
		if (current < max * 2)
			avatar.setHp(current + 1);
		int16_t hunger = avatar.effectiveStr;
		if (hunger < 200)
			avatar.setEffectiveStr(hunger + 1);
		ticks = 0;
	}
	flags |= PROC_SUSPENDED;
	intFlags |= 2;
}

void StatRestorer::interruptHandler(uint32_t event, uint32_t)
{
	if (!inGameMode)
		return;
	switch (event)
	{
	case 2:
		intFlags &= ~2L;
		ticks++;
		flags &= ~PROC_SUSPENDED;
		break;
	}
}

// Eating lowers the hunger counter and heals a quarter of the drop as hit points.
void FeedAvatar(int16_t amount)
{
	int16_t hunger = avatar.effectiveStr;

	if (hunger == 0)
		return;
	if (amount > hunger)
		amount = hunger;
	int16_t heal = (hunger >> 2) - ((hunger - amount) >> 2);
	uint8_t str = avatar.getStr();
	if (heal && avatar.getHp() < str * 2)
	{
		uint8_t hp = avatar.getHp() + heal;
		avatar.setHp(hp > str * 2 ? str * 2 : hp);
	}
	avatar.setEffectiveStr(hunger - amount);
}

// Strength fades as hunger climbs from 150 to 200.
char Avatar::getEffectiveStr(void)
{
	if (effectiveStr >= 150)
		return getStr() * (200 - effectiveStr) / 50;
	return getStr();
}

inline void Process::receiveMessage(Message, uint16_t, int32_t)
{
}

inline void Process::postLoad(void)
{
}

inline uint8_t ProcessInterrupt::checkInterrupt(uint32_t, uint32_t &)
{
	return 0;
}
