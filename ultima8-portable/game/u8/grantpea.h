#ifndef GRANTPEA_H
#define GRANTPEA_H

#include "u8port.h"

#include "process.h"

class GrantPeaceSpell : public Process
{
public:
	GrantPeaceSpell(void);
	virtual void process(void);
};

void createNpc(uint16_t);
void castGrantPeaceSpell(void);

#endif
