// Usecode lifted to C-like text: the stack code's expressions rebuilt, and its branches turned
// back into if / else, while, break, continue and return. A routine it cannot lift (a shape it
// does not know) stays as assembly, so every class always has a text.
//
// This is a reading aid and the front of the compiler; the compiler's recompile-and-compare is
// what proves a lifted routine right.
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

class Usecode;

namespace Ucdecomp
{

struct Routine
{
	int start = 0;		// its first instruction's offset in the class's code
	int end = 0;		// one past its last
	QString name;		// the event it handles ("use"), or "func_01a2"
	bool lifted = false;	// text is C-like; otherwise it is assembly
	QString why;		// when not lifted: what stopped it
	QString text;
};

// A class's routines, from its flex record; false (with *error) if the code cannot be decoded.
bool decompile(const QByteArray &record, int classId, QVector<Routine> *out, QString *error);

// The whole class as one text: a heading, then each routine.
QString classText(const QByteArray &record, int classId, const Usecode *names, QString *error);

}
