// Usecode as assembly text that assembles back to exactly the same bytes.
//
// This is the exact layer under the C-like language (decompiler, compiler): every class the game
// ships prints to text and assembles to the same record, byte for byte. Anything the later layers
// cannot structure stays in this form.
//
// A class is
//
//     class 330                      ; STALAG
//     header 2252 170                ; the record header's words 0 and 2, as distances above its size
//     event 5 L0123                  ; an event's routine (events left out are 0)
//     code
//     L0123:                         ; a label: a branch, call, spawn or event target
//         locals 2                   ; one instruction a line: mnemonic, operands
//         push_string "Hello\x00"    ; ';' starts a comment; offsets and notes are comments
//         call_intrinsic 4, 258      ;
//         branch_false L0140
//         end
//
// Operands are decimal or 0x hex. Branches, own-class calls and spawns, and events name labels,
// which move when code is added; calls into other classes keep their numbers (they need the
// whole file, a later step). Strings are quoted with \" \\ and \xNN.
#pragma once

#include <QByteArray>
#include <QString>

class Usecode;

namespace Ucasm
{

// A class's flex record (its 12 byte header, then the event table and instructions) as text;
// empty with *error set if it cannot be decoded.
QString print(const QByteArray &record, int classId, const Usecode *names, QString *error);

// Text to a flex record. *classId is the class the text names.
QByteArray assemble(const QString &text, int *classId, QString *error);

// The mnemonic of an opcode ("push_string", or "op_27" for one the game never uses).
QString mnemonic(uint8_t opcode);

}
