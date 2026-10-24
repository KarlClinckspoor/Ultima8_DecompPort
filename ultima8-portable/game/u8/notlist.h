#ifndef NOTLIST_H
#define NOTLIST_H

#include "u8port.h"

#include "mlist.h"

class NotifyList : public List
{
public:
	void notify(uint16_t);
};

#endif
