#ifndef CCURRVP_H
#define CCURRVP_H

#include "u8port.h"

#include "cglobvp.h"

// Makes a viewport the global one until it goes out of scope.
class CurrentVport : public GlobalVport
{
public:
	Vport *saved;

	CurrentVport(Vport *);
	~CurrentVport(void);
};

#endif
