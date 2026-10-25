// CALLDTOR.ASM: Process::callDestructor.
//
// Runs the process's virtual destructor without freeing it (Borland's destructor flag 2):
// Process::terminate goes on to unlink the process, and the kernel frees its memory later.
#include "u8port.h"
#include "u8process.h"

void Process::callDestructor(void)
{
	this->~Process();
}
