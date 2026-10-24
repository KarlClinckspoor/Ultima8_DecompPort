#ifndef COMBIN_H
#define COMBIN_H

#include "u8port.h"

#include "item.h"

class Combinable : public Item
{
public:
	Combinable(Referent r) { referent = r; }

	Boolean combine(Referent);
	Boolean give(Referent);
};

#endif
