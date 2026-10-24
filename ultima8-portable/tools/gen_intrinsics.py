#!/usr/bin/env python3
"""Writes game/u8/intrinsics.cpp, the table of usecode intrinsics (see intrinsics.h), from
tools/data/intrinsics.txt: each intrinsic's number, its U8.EXE map address and its Borland
name with parameter types."""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TYPES = {
    "short": "int16_t", "int": "int16_t", "unsigned short": "uint16_t",
    "unsigned int": "uint16_t", "unsigned char": "uint8_t", "char": "char",
    "long": "int32_t", "unsigned long": "uint32_t",
}

def port_type(t):
    t = t.strip()
    ref = ptr = ""
    if t.endswith(" far&"):
        t, ref = t[:-5], " &"
    elif t.endswith(" far*"):
        t, ptr = t[:-5], " *"
    return TYPES.get(t, t) + ptr + ref

HEADERS = ["item.h", "npc.h", "egg.h", "monegg.h", "book.h", "grave.h", "camera.h",
           "kernel.h", "u8point.h", "uprocess.h", "inter.h", "world.h", "dlitem.h",
           "contgump.h", "urandom.h", "grantpea.h", "dsfxman.h", "anim.h", "itemcach.h",
           "contrect.h", "pathfind.h", "motrak.h", "sprite.h", "type.h", "gametime.h",
           "pal.h", "flicplay.h", "statrest.h", "amusic.h", "balls.h", "u8process.h"]

rows = []
for line in open(os.path.join(ROOT, "tools", "data", "intrinsics.txt")):
    if line.startswith("#") or not line.strip():
        continue
    n, seg, off, name = line.rstrip("\n").split(" ", 3)
    rows.append((int(n), seg, off, name))

out = ["// Written by tools/gen_intrinsics.py from tools/data/intrinsics.txt: usecode's",
       "// intrinsics in their U8.EXE order (see intrinsics.h).",
       "",
       '#include "u8port.h"',
       ""]
out += ['#include "%s"' % h for h in HEADERS]
out += ['#include "error.h"',
        '#include "intrinsics.h"',
        "",
        "// main() was the last entry of unkcoff.dat, for relocating the others; no usecode",
        "// calls it.",
        "static uint32_t noIntrinsic(const char *)",
        "{",
        '\thalt(__FILE__, __LINE__, "usecode called main()");',
        "\treturn 0;",
        "}",
        "",
        "const IntrinsicEntry intrinsics[] =",
        "{"]
for n, seg, off, name in rows:
    m = re.match(r"(.*)\((.*)\)$", name)
    if not m:
        out.append('\t{0x%s, 0x%s, "%s", noIntrinsic},' % (seg, off, name))
        continue
    func, params = m.group(1), m.group(2)
    if "::" not in func:
        func = "::" + func
    types = [port_type(p) for p in params.split(",")] if params else []
    args = ", ".join([func] + types)
    out.append('\t{0x%s, 0x%s, "%s", INTRINSIC(%s)},' % (seg, off, name, args))
out += ["};",
        "",
        "const uint16_t numIntrinsics = sizeof(intrinsics) / sizeof(intrinsics[0]);",
        ""]
open(os.path.join(ROOT, "game", "u8", "intrinsics.cpp"), "w").write("\n".join(out))
print(len(rows), "intrinsics")
