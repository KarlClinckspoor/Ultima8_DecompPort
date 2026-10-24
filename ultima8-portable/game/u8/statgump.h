#ifndef STATGUMP_H
#define STATGUMP_H

#include "u8port.h"

#include "irgump.h"
#include "sbutton.h"

class Event;
class NewGump;

// The avatar's health and mana bars.
class AvatarStatusGump : public ItemRelativeGump, public GumpShape
{
public:
	AvatarStatusGump(NewGump *, uint16_t);
	virtual ~AvatarStatusGump(void);
	virtual void commandMouseLeft(Event &);
	virtual void draw(int16_t, int16_t);
};

void closeStatusGump(void);
void openStatusGump(void);

#endif
