// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: GAMETIME.C

#include "u8port.h"

#include "kernel.h"
#include "itemcach.h"
#include "camera.h"
#include "gametime.h"

#define TICKS_PER_SECOND	30
#define TICKS_PER_HOUR		27000L

GameTime *theGameTime = 0;
Animation *theAnimation = 0;

// Out of line: a virtual function, which the class's vtable needs in every build.
void GameTime::postLoad(void) { theGameTime = this; }
U8_INLINE void startSchedules(uint16_t hour) { new Scheduler(hour); }
// Out of line: a virtual function, which the class's vtable needs in every build.
void Animation::postLoad(void) { theAnimation = this; }

GameTime::GameTime(void)
{
	loops = 0;
	pause = 0;
	hourChanged = 0;
	setProcessType((ProcessType)0x245);
	Kernel::setIdString(pid, "GameTime");
	setDaemon();
}

void GameTime::process(void)
{
	if (hourChanged)
	{
		hourChanged = 0;
		startSchedules(theAnimation->getHours());
	}
	if (pause)
	{
		if (pause != -1)
			pause--;
	}
	else
		loops++;
}

Animation::Animation(void)
{
	f_3c = f_38 = f_40 = f_44 = 0;
	f_48 = -1;
	f_4c = f_4d = 0;
	Kernel::setIdString(pid, "Animation");
	setProcessType((ProcessType)0x230);
	setSafeInterrupt();
	setHertz(TICKS_PER_SECOND);
	setIntFlags(intFlags | 2);
	setDaemon();
}

void Animation::interruptHandler(uint32_t event, uint32_t)
{
	switch (event)
	{
	case 2:
		if (f_48 >= 0)
		{
			if (f_40 / TICKS_PER_HOUR != f_48 / TICKS_PER_HOUR)
			{
				f_44 = f_48 / TICKS_PER_HOUR;
				theGameTime->hourChanged = 1;
			}
			f_40 = f_48;
			f_48 = -1;
		}
		else
		{
			f_40++;
			if (f_40 % TICKS_PER_HOUR == 0)
			{
				f_44++;
				theGameTime->hourChanged = 1;
			}
		}
		if (!theGameTime->isPaused() && !f_4d)
		{
			f_38++;
			if (!f_4c)
				f_3c = f_38;
			Camera::needAnim++;
		}
		break;
	}
}

void Animation::lockTime(void)
{
	f_4c = 1;
}

void Animation::unlockTime(void)
{
	f_3c = f_38;
	f_4c = 0;
}

void initGameTime(void)
{
	if (!theGameTime)
		theGameTime = new GameTime;
	if (!theAnimation)
	{
		theAnimation = new Animation;
		theAnimation->connectToRealTime(1);
	}
}

int32_t TimeInGameLoops(void)
{
	return theGameTime->loops;
}

int32_t TimeInSeconds(void)
{
	return theAnimation->f_40 / TICKS_PER_SECOND;
}

int32_t TimeInMinutes(void)
{
	return theAnimation->f_40 / (60 * TICKS_PER_SECOND);
}

int32_t TimeInGameHours(void)
{
	return theAnimation->f_44;
}

void SetTimeInSeconds(int32_t seconds)
{
	theAnimation->setTicks(seconds * TICKS_PER_SECOND);
}

void SetTimeInMinutes(int32_t minutes)
{
	theAnimation->setTicks(minutes * (60 * TICKS_PER_SECOND));
}

void SetTimeInGameHours(int16_t hours)
{
	theAnimation->setTicks(hours * TICKS_PER_HOUR);
}
