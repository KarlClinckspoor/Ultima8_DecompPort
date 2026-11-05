// Usecode's instructions, events and intrinsics, as the interpreter (u8/inter.cpp) and the
// spawner (u8/uprocess.cpp) see them. Shared by the usecode reader, the language builder and
// the usecode machine.
#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <vector>

namespace Ucode
{

struct Instruction
{
	int at;		// its offset in the class's code
	uint8_t op;
	int size;	// with its operands
};

// Operand bytes after an opcode; -1 when it is not one. 0x0d (push string) and 0x5c (debug
// unit) are sized by decode.
int operandBytes(uint8_t op);

// The instructions of a class's code (the record after its 12-byte header), from 0x80, past
// the 32 event offsets.
bool decode(const QByteArray &code, std::vector<Instruction> *out, QString *error);

// The events the engine sends (item/item.cpp's #defines): "look", "use", ...; empty for the
// slots never sent.
QString eventName(int event);
enum { EventCount = 32, EventLook = 0, EventUse = 1 };

// Usecode's intrinsics, by number (the order of game/u8/intrinsics.cpp, which is UNKCOFF.DAT's).
struct Intrinsic
{
	const char *name;	// "Item::getQuality()"
	bool member;		// its object's far pointer comes first in the arguments
	const char *args;	// one letter an argument: p pointer or reference, l long, u unsigned, i int
};
int intrinsicCount();
const Intrinsic *intrinsic(int number);	// null if there is none
// The argument bytes it takes (with the object pointer).
int intrinsicArgBytes(int number);

}
