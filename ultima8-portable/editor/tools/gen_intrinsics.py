#!/usr/bin/env python3
"""Writes src/data/intrinsics.inc: usecode's intrinsics by number, from the game's own table
(../game/u8/intrinsics.cpp, in U8.EXE's order, which is the order of UNKCOFF.DAT and so the
numbers usecode calls them by). For each: its name as the link map gave it, whether it is a
member function (its object's far pointer comes first), and how its arguments lie on the
usecode stack: as Borland pushed them, pointers, references and longs as 4 bytes, the rest
(int, short, char, enums) as 2.

    python3 tools/gen_intrinsics.py      (from the editor folder)
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', '..', 'game', 'u8', 'intrinsics.cpp')
OUT = os.path.join(HERE, '..', 'src', 'data', 'intrinsics.inc')
MEMBERS = {'Item', 'Npc', 'Egg', 'MonsterEgg', 'Book', 'Scroll', 'Grave', 'Plaque'}

entries = re.findall(r'\{0x[0-9A-Fa-f]+, 0x[0-9A-Fa-f]+, "([^"]+)"', open(SRC).read())
lines = []
for name in entries:
    m = re.match(r'(?:(\w+)::)?([~\w]+)\((.*)\)$', name)
    cls = m.group(1) if m else None
    params = [p.strip() for p in m.group(3).split(',')] if m and m.group(3).strip() else []
    kinds = ''
    for p in params:
        if p.endswith('*') or p.endswith('&'):
            kinds += 'p'
        elif p in ('long', 'unsigned long'):
            kinds += 'l'
        elif p.startswith('unsigned'):
            kinds += 'u'
        else:
            kinds += 'i'	# int, short, char, enums: a word, used signed
    lines.append('\t{"%s", %s, "%s"},' % (name, 'true' if cls in MEMBERS else 'false', kinds))
with open(OUT, 'w') as f:
    f.write('// Written by tools/gen_intrinsics.py from game/u8/intrinsics.cpp. Do not edit.\n')
    f.write('// {name, a member function (its this first), arguments: p pointer, l long, u unsigned, i int}\n')
    f.write('\n'.join(lines) + '\n')
print(len(lines), 'intrinsics')
