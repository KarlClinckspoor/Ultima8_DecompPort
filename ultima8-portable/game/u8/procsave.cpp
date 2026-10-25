// Saving and loading process objects; see procsave.h.

#include "u8port.h"

#include <typeinfo>
#include "u8process.h"
#include "ferror.h"
#include "procsave.h"

static const ProcessClass *findByType(const std::type_info &type)
{
	for (uint16_t i = 0; i < numProcessClasses; i++)
		if (*(const std::type_info *)processClasses[i].type == type)
			return &processClasses[i];
	return 0;
}

static const ProcessClass *findByName(const char *name)
{
	for (uint16_t i = 0; i < numProcessClasses; i++)
		if (strcmp(processClasses[i].name, name) == 0)
			return &processClasses[i];
	return 0;
}

char *SaveProcess(Process *p, uint16_t &size)
{
	const ProcessClass *c = findByType(typeid(*p));
	char *saved;
	size_t nameLength;

	if (c == 0)
	{
		char message[200];
		snprintf(message, sizeof message, "kernel: no saved form for process class %s", typeid(*p).name());
		Fatal(message);
	}
	nameLength = strlen(c->name) + 1;
	size = (uint16_t)(nameLength + c->size);
	saved = new char[size];
	memcpy(saved, c->name, nameLength);
	memcpy(saved + nameLength, (void *)p, c->size);
	return saved;
}

Process *RestoreProcess(const char *saved, uint32_t size)
{
	size_t nameLength = strnlen(saved, size);
	const ProcessClass *c;
	void *memory;

	if (nameLength >= size)
		return 0;
	c = findByName(saved);
	if (c == 0 || size != nameLength + 1 + c->size)
		return 0;
	// The global operator new, as Kernel::load used: Process::operator new would leave the size
	// for a constructor to collect, and the copy constructor that rebuilds it does not.
	memory = ::operator new(c->size);
	c->rebuild(memory, saved + nameLength + 1);
	return (Process *)memory;
}

uint16_t ProcessClassSize(Process *p)
{
	const ProcessClass *c = findByType(typeid(*p));

	return c ? c->size : 0;
}

// An anchor in the program's constant data; constants and code lie within a few megabytes of
// it, in the same order in every run of a build.
static const char imageAnchor[] = "Ultima VIII";
#define IMAGE_REACH 0x10000000L

uint32_t ImageOffset(const void *p)
{
	intptr_t delta;

	if (p == 0)
		return 0;
	delta = (intptr_t)p - (intptr_t)imageAnchor;
	if (delta <= -IMAGE_REACH || delta >= IMAGE_REACH)
		return 0;
	return (uint32_t)(delta + IMAGE_REACH) | 0x80000000u;
}

const void *ImageAddress(uint32_t offset)
{
	if (!(offset & 0x80000000u))
		return 0;
	return imageAnchor + ((intptr_t)(offset & 0x7fffffffu) - IMAGE_REACH);
}
