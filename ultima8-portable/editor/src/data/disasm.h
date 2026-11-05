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
	int flagWidth = 0;		// its width in bits
	bool flagWrite = false;
	int opcode = -1;		// Code: the instruction's opcode
};

// One routine as a graph: its instructions cut into blocks that run straight through, joined
// by the branches between them (IDA's graph view).
struct Edge
{
	enum Kind { Next, Taken, NotTaken, Jump };	// falls through; a conditional branch taken or not; go to
	int to;		// the block
	Kind kind;
};

struct Block
{
	int first, last;	// indices of its lines (Code lines only)
	QVector<Edge> out;
	int offset = -1;	// of its first instruction
};

// The blocks of the routine whose heading is lines[head] (a Head line), in code order; the
// first is where it starts.
QVector<Block> flowGraph(const QVector<Line> &lines, int head);
// The index of the Head line of the routine that holds offset, or -1.
int routineHead(const QVector<Line> &lines, int offset);

// The class's code as lines; false (and why) when it cannot be decoded.
bool disassemble(const Usecode &usecode, int classId, QVector<Line> *out, QString *error);

// Where a class's routines are called or spawned from in other classes: (class, offset) pairs,
// computed once for the whole file.
QVector<QPair<int, int>> callers(const Usecode &usecode, int classId);

}
