// Usecode as text: a class's code an instruction a line, with its routines and events named,
// intrinsics by name, strings, flags, and calls and branches as targets to follow. The
// operands are read as the interpreter reads them (u8/inter.cpp, the editor's ucvm.cpp).
#pragma once

#include <QString>
#include <QVector>

class Usecode;

namespace Ucode
{

struct Line
{
	enum Kind { Head, Code };
	Kind kind = Code;
	int at = -1;			// the instruction's offset in the class's code (Head: the routine's)
	int event = -1;			// Head: the event the routine handles, or -1
	QString op;				// "push", "call", ...
	QString args;			// the operands, in words
	QString note;			// what it means: an intrinsic's name, a string, a flag
	int jump = -1;			// a branch's target in this class
	int callClass = -1;		// a call or spawn's class and offset
	int callOffset = -1;
	int flagBit = -1;		// a global flag read or written
};

// The class's code as lines; false (and why) when it cannot be decoded.
bool disassemble(const Usecode &usecode, int classId, QVector<Line> *out, QString *error);

// Where a class's routines are called or spawned from in other classes: (class, offset) pairs,
// computed once for the whole file.
QVector<QPair<int, int>> callers(const Usecode &usecode, int classId);

}
