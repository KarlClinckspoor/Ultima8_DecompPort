// Code generation: the Ucast tree back to usecode instructions.
//
// This first version takes its choices from what the decompiler recorded in the tree (which
// opcode made a value, what a statement hid), so it measures how much of the shipped code the
// tree can give back, byte for byte. Rules for trees made by hand come next.
#pragma once

#include <QByteArray>
#include <QString>

#include "data/ucast.h"

namespace Ucgen
{

// The routine's code: `locals` (when hasFrame), the statements, then `end`. Branches are relative,
// so the bytes do not depend on where the routine sits. Empty with *error set if it cannot.
QByteArray generate(const Ucast::Block &body, bool hasFrame, int frame, QString *error);

}
