#!/usr/bin/env python3
"""First, mechanical pass of the port: copies the decompiled Borland C++ sources from
source/ into ultima8-portable/game/ with

  - lower-case file names, .C/.CPP as .cpp (all of them were built as C++, BCC -P), .H as .h
  - the 16-bit integer types spelled as fixed-width types (int, unsigned, long, ...)
  - far/near/huge/pascal/cdecl/register dropped
  - #include paths flattened and DOS-only system headers dropped, "u8port.h" first

Everything after this pass (asm blocks, segments, DOS calls) was ported by hand; the
script is kept to document how the files were made, and is not meant to be run again
over the hand-edited tree.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(os.path.dirname(ROOT), "source")
DST = os.path.join(ROOT, "game")

DROP_HEADERS = {"dos.h", "conio.h", "alloc.h", "mem.h", "dir.h", "io.h", "fcntl.h",
                "phapi.h", "bios.h", "process.h", "malloc.h", "sys\\stat.h", "sys/stat.h",
                "share.h", "values.h", "dpmi.h", "graphics.h"}
KEEP_SYSTEM = {"stdio.h", "stdlib.h", "string.h", "ctype.h", "stdarg.h", "math.h",
               "limits.h", "time.h", "setjmp.h", "assert.h", "errno.h", "signal.h",
               "stddef.h", "float.h"}
DROP_WORDS = {"far", "near", "huge", "_far", "_near", "_huge", "__far", "__near", "__huge",
              "pascal", "_pascal", "__pascal", "cdecl", "_cdecl", "__cdecl", "register",
              "_loadds", "__loadds", "_export", "__export", "_saveregs", "__saveregs"}
INT_WORDS = {"unsigned", "signed", "short", "int", "long", "char"}


def tokens(text):
    """Splits C++ source into (kind, text) pieces: code, comment, string, char, pp."""
    i, n = 0, len(text)
    out = []
    code_start = 0

    def flush(end):
        if end > code_start:
            out.append(("code", text[code_start:end]))

    while i < n:
        c = text[i]
        if text.startswith("//", i):
            flush(i)
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(("comment", text[i:j]))
            i = code_start = j
        elif text.startswith("/*", i):
            flush(i)
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(("comment", text[i:j]))
            i = code_start = j
        elif c == '"' or c == "'":
            flush(i)
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            j += 1
            out.append(("string", text[i:j]))
            i = code_start = j
        else:
            i += 1
    flush(n)
    return out


def map_int_run(words):
    """A run of integer keywords, e.g. ['unsigned', 'long'], as one fixed-width type."""
    s = set(words)
    if "char" in s:
        if "unsigned" in s:
            return "uint8_t"
        if "signed" in s:
            return "int8_t"
        return "char"
    unsigned = "unsigned" in s
    if "long" in s:
        return "uint32_t" if unsigned else "int32_t"
    return "uint16_t" if unsigned else "int16_t"


IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def convert_code(code):
    out = []
    pos = 0
    pending = []          # integer keywords waiting to be joined
    pending_text = ""     # their original text, whitespace included
    pending_start = None

    def emit_pending():
        nonlocal pending, pending_text
        if pending:
            out.append(map_int_run(pending))
        pending = []
        pending_text = ""

    for m in IDENT.finditer(code):
        word = m.group(0)
        between = code[pos:m.start()]
        if word in INT_WORDS and (not pending or between.strip() == ""):
            if not pending:
                out.append(between)
            pending.append(word)
            pos = m.end()
            continue
        if pending and word == "double" and between.strip() == "":
            # long double
            out.append(" ".join(pending))
            pending = []
        emit_pending()
        out.append(between)
        if word in INT_WORDS:
            pending.append(word)
        elif word in DROP_WORDS:
            # keep spacing sane: drop the word, leave one blank where it was
            out.append("")
        else:
            out.append(word)
        pos = m.end()
    emit_pending()
    out.append(code[pos:])
    text = "".join(out)
    # "far *" leftovers make "char  *"; tidy double blanks inside a line
    text = re.sub(r"(?<=\S)  +(?=[*&\w(])", " ", text)
    return text


def convert_include(line, here):
    m = re.match(r'(\s*#\s*include\s*)([<"])([^>"]+)[>"](.*)', line)
    if not m:
        return line
    name = m.group(3).replace("/", "\\")
    base = name.split("\\")[-1].lower()
    if m.group(2) == "<":
        if base in DROP_HEADERS or name.lower() in DROP_HEADERS:
            return "// " + line.strip() + "  (DOS only)"
        if base in KEEP_SYSTEM:
            return line
        # Borland headers of the game's own (<phapi.h> handled above)
        return '%s"%s"%s' % (m.group(1), base, m.group(4))
    return '%s"%s"%s' % (m.group(1), base, m.group(4))


def escape_high_bytes(s):
    return "".join("\\x%02x" % ord(ch) if ord(ch) > 0x7f else ch for ch in s)


def convert_file(text, is_header):
    lines_out = []
    pieces = tokens(text)
    body = []
    for kind, t in pieces:
        if kind == "code":
            body.append(convert_code(t))
        elif kind == "string":
            body.append(escape_high_bytes(t))
        else:
            body.append(t)
    text = "".join(body)
    for line in text.split("\n"):
        if re.match(r"\s*#\s*include", line):
            line = convert_include(line, None)
        lines_out.append(line.rstrip())
    text = "\n".join(lines_out)
    if is_header:
        # after the include guard's #define, or at the top
        m = re.search(r"#ifndef\s+\w+\s*\n#define\s+\w+[^\n]*\n", text)
        if m:
            text = text[:m.end()] + '\n#include "u8port.h"\n' + text[m.end():]
        else:
            text = '#include "u8port.h"\n' + text
    else:
        # after the leading // flags: and // name: comments
        lines = text.split("\n")
        k = 0
        while k < len(lines) and lines[k].startswith("//"):
            k += 1
        lines.insert(k, '\n#include "u8port.h"')
        text = "\n".join(lines)
    if not text.endswith("\n"):
        text += "\n"
    return text


def main():
    count = 0
    for dirpath, dirnames, filenames in os.walk(SRC):
        rel = os.path.relpath(dirpath, SRC)
        if rel.split(os.sep)[0] in ("EUSECODE",):
            continue
        for f in filenames:
            stem, ext = os.path.splitext(f)
            ext = ext.upper()
            if ext in (".C", ".CPP"):
                new = stem.lower() + ".cpp"
                header = False
            elif ext == ".H":
                new = stem.lower() + ".h"
                header = True
            else:
                continue
            subdir = rel.lower().replace("_lib", "lib")
            if subdir == "polib/inc":
                subdir = "polib"
            outdir = os.path.join(DST, subdir) if subdir != "." else DST
            os.makedirs(outdir, exist_ok=True)
            with open(os.path.join(dirpath, f), encoding="latin-1") as fh:
                text = fh.read()
            text = text.replace("\r\n", "\n")
            with open(os.path.join(outdir, new), "w", encoding="latin-1", newline="\n") as fh:
                fh.write(convert_file(text, header))
            count += 1
    print("converted", count, "files into", DST)


if __name__ == "__main__":
    sys.exit(main())
