// Calling usecode's intrinsics; see intrinsics.h.

#include "u8port.h"

#include "error.h"
#include "uprocess.h"
#include "intrinsics.h"

// Entries for addresses unkcoff.dat names but the table has not, made on first use.
static IntrinsicEntry missing[16];
static uint16_t numMissing;
static uint32_t missingAddress[16];

static uint32_t unresolved(const char *)
{
	halt(__FILE__, __LINE__, "usecode called an intrinsic this port does not have");
	return 0;
}

const IntrinsicEntry *findIntrinsic(uint16_t segment, uint16_t offset)
{
	uint32_t address = (uint32_t)segment << 16 | offset;
	uint16_t i;

	for (i = 0; i < numIntrinsics; i++)
		if (intrinsics[i].segment == segment && intrinsics[i].offset == offset)
			return &intrinsics[i];
	for (i = 0; i < numMissing; i++)
		if (missingAddress[i] == address)
			return &missing[i];
	trace(TRACE_ALWAYS, "\r\nUnresolved c symbol at %04x:%04x", segment, offset);
	if (numMissing == 16)
		return &missing[15];
	missingAddress[numMissing] = address;
	missing[numMissing].segment = segment;
	missing[numMissing].offset = offset;
	missing[numMissing].name = "?";
	missing[numMissing].call = unresolved;
	return &missing[numMissing++];
}

// The DOS interpreter copied the arguments onto the CPU stack, so the function worked on a
// copy; this does the same, with room behind it for a function that reads more than was
// pushed.
uint32_t callIntrinsic(uint16_t n, const char *args, int16_t argBytes)
{
	char copy[256];
	const IntrinsicEntry *entry = (const IntrinsicEntry *)cTable[n];

	memset(copy, 0, sizeof copy);
	if (argBytes > 0)
		memcpy(copy, args, (size_t)(argBytes < 200 ? argBytes : 200));
	return entry->call(copy);
}
