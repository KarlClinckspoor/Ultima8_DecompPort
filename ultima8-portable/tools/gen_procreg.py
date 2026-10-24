#!/usr/bin/env python3
"""Writes game/u8/procreg.cpp: the table of every process class (every class derived from
Process) with the copy constructor kernel.dat loading rebuilds it with (see procsave.h).

Run it again after adding a process class. Abstract classes (with a pure virtual function
left) are never the class of a live process and are skipped."""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")

classes = {}   # name -> (base, header, body)
for dirpath, _, files in os.walk(GAME):
    for f in files:
        if not f.endswith(".h"):
            continue
        path = os.path.join(dirpath, f)
        text = open(path, encoding="latin-1").read()
        for m in re.finditer(r"\bclass\s+(\w+)\s*:\s*public\s+(\w+)[^{;]*\{", text):
            depth, i = 1, m.end()
            while depth and i < len(text):
                depth += {"{": 1, "}": -1}.get(text[i], 0)
                i += 1
            classes[m.group(1)] = (m.group(2), f, text[m.end():i])
        for m in re.finditer(r"\bclass\s+(Process)\s*\{", text):
            classes.setdefault("Process", (None, f, ""))

def is_process(name):
    seen = set()
    while name in classes and name not in seen:
        seen.add(name)
        base = classes[name][0]
        if base == "Process":
            return True
        name = base
    return False

def pure_virtuals(name):
    """Names of pure virtual functions declared along the chain and not overridden below."""
    chain = []
    while name in classes:
        chain.append(name)
        name = classes[name][0]
    if "Process" in classes:
        chain.append("Process")
    pure, done = set(), set()
    for c in chain:  # most derived first
        body = classes.get(c, (None, None, ""))[2] if c != "Process" else open(
            os.path.join(GAME, "glib", "u8process.h"), encoding="latin-1").read()
        for m in re.finditer(r"virtual\s+[\w\s\*&]*?\b(\w+)\s*\([^)]*\)\s*(=\s*0)?", body):
            fn = m.group(1)
            if m.group(2):
                if fn not in done:
                    pure.add(fn)
            else:
                done.add(fn)
        # plain redeclarations without virtual also override
        for m in re.finditer(r"^\s*(?!virtual)[\w\*&\s]+?\b(\w+)\s*\([^)]*\)\s*;", body, re.M):
            done.add(m.group(1))
    return pure

procs = sorted(c for c in classes if is_process(c) and not pure_virtuals(c))
headers = sorted({classes[c][1] for c in procs})

out = ["// Written by tools/gen_procreg.py: every process class, for kernel.dat (see procsave.h).",
       "",
       '#include "u8port.h"',
       "",
       "#include <new>",
       "#include <typeinfo>"]
out += ['#include "%s"' % h for h in headers]
out += ['#include "procsave.h"',
        "",
        "#define PROCESS_CLASS(X) { #X, &typeid(X), (uint16_t)sizeof(X), \\",
        "\t[](void *to, const void *from) { ::new (to) X(*static_cast<const X *>(from)); } }",
        "",
        "const ProcessClass processClasses[] =",
        "{"]
out += ["\tPROCESS_CLASS(%s)," % c for c in procs]
out += ["};", "",
        "const uint16_t numProcessClasses = sizeof(processClasses) / sizeof(processClasses[0]);", ""]
open(os.path.join(GAME, "u8", "procreg.cpp"), "w").write("\n".join(out))
print(len(procs), "process classes,", len(headers), "headers")
