#ifndef PROCSAVE_H
#define PROCSAVE_H

#include "u8port.h"

// Saving and loading process objects for kernel.dat (see Kernel::save).
//
// The DOS build saved a process as the bytes of its object and read it back the same way;
// its vtable pointer (a near pointer into U8.EXE's data segment) was the same in every run.
// A host vtable pointer is an address that changes from run to run, so a saved object starts
// with its class's name, and loading rebuilds it through that class's copy constructor, which
// sets the vtable pointers of the object and of its members (List and friends). The copy
// constructors are the compiler's own, copying every field as the DOS build's bytes did.

class Process;

// The process's class name and object bytes in one block for a flex record; delete[] it.
char *SaveProcess(Process *p, uint16_t &size);
// Rebuilds a process from such a block, in memory from the global operator new, without
// running its constructor (the kernel places it). 0 when the class or size is unknown.
Process *RestoreProcess(const char *saved, uint32_t size);
// sizeof the process's class.
uint16_t ProcessClassSize(Process *p);

// One process class: what procreg.cpp lists.
struct ProcessClass
{
	const char *name;
	const void *type;	// its std::type_info
	uint16_t size;
	// Copy-constructs the class at to from the bytes at from.
	void (*rebuild)(void *to, const void *from);
};

extern const ProcessClass processClasses[];
extern const uint16_t numProcessClasses;

#endif
