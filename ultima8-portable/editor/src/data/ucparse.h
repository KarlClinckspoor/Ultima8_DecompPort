// Parses the C-like text Ucdecomp prints (one construct a line) back into the Ucast tree.
#pragma once

#include <QString>

#include "data/ucast.h"

namespace Ucparse
{

// A routine's body as printed by Ucast::render. False, with *error naming the line, if it is not
// text the printer could have made.
bool parseBlock(const QString &text, Ucast::Block *out, QString *error);

}
