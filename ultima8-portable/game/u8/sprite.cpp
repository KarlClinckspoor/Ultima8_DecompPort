// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: SPRITE.C

#include "u8port.h"

#include "item.h"
#include "kernel.h"
#include "wait.h"
#include "sprite.h"

int16_t createSprite(int16_t shape, int16_t first, int16_t last, int16_t end, int16_t repeat, int16_t delay, uint16_t x, uint16_t y, uint8_t z)
{
	Sprite *sprite = new Sprite(shape, first, last, end, repeat, delay, x, y, z);
	if (sprite->ref)
		return sprite->pid;
	sprite->pop(0);
	return 0;
}

int16_t createSprite(int16_t shape, int16_t first, int16_t last, int16_t delay, uint16_t x, uint16_t y, uint8_t z)
{
	Sprite *sprite = new Sprite(shape, first, last, last, 1, delay, x, y, z);
	if (sprite->ref)
		return sprite->pid;
	sprite->pop(0);
	return 0;
}

Sprite::Sprite(uint16_t shape, uint16_t first, uint16_t last, uint16_t end, int16_t times, uint16_t wait, uint16_t x, uint16_t y, uint8_t z)
{
	Kernel::setIdString(pid, "Sprite");
	Item item;
	if (item.create(shape, first))
	{
		setRef(item.referent);
		item.pop(x, y, z);
		item.setStatus(item.getStatus() | 0x80);
		if (isPhantom())
		{
			setRef(0);
			return;
		}
	}
	else
		setRef(0);
	firstFrame = first;
	frame = firstFrame;
	lastFrame = last;
	endFrame = end;
	forward = 1;
	turned = 0;
	repeat = times;
	delay = wait;
}

void Sprite::process(void)
{
	Item item(ref);

	if (turned)
	{
		turned = 0;
		if (forward)
		{
			if ((frame = lastFrame) < endFrame)
				frame++;
			else
				frame--;
			forward = 0;
			return;
		}
		repeat--;
		if (repeat)
		{
			if ((frame = firstFrame) < lastFrame)
				frame++;
			else
				frame--;
			forward = 1;
			return;
		}
		item.destroy();
		return;
	}
	item.setFrame(frame);
	if (forward)
	{
		if (firstFrame < lastFrame)
		{
			if (++frame > lastFrame)
				turned = 1;
		}
		else if (--frame < lastFrame)
			turned = 1;
	}
	else
	{
		if (lastFrame < endFrame)
		{
			if (++frame > endFrame)
				turned = 1;
		}
		else if (--frame < endFrame)
			turned = 1;
	}
	if (delay)
	{
		Wait *wait = new Wait(delay, ref);
		wait->then(this);
		wait->start();
	}
}
