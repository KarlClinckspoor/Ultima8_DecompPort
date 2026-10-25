#!/usr/bin/env python3
"""Reads the port's recordings (SESSION.U8R; see platform/matoya/record.c).

    u8rec.py info SESSION      the header, the length in game time, counts of each operation
    u8rec.py dump SESSION      every operation, one a line, with the game time it comes at

SESSION is a session folder or its SESSION.U8R.
"""
import os
import signal
import sys

OPS = {1: "PUMP", 2: "EVENTS", 3: "VALUE", 4: "CHECK", 5: "END"}
EVENT_KINDS = {0: "key", 1: "move", 2: "button", 3: "focus-lost"}
VALUES = {1: "file-time", 2: "disk-free", 3: "day-of-week", 4: "time-now"}


def read_session(path):
    if os.path.isdir(path):
        path = os.path.join(path, "SESSION.U8R")
    with open(path, "rb") as f:
        data = f.read()
    header = {}
    args = []
    at = 0
    lines = 0
    while True:
        end = data.index(b"\n", at)
        line = data[at:end].decode("utf-8", "replace").rstrip("\r")
        at = end + 1
        lines += 1
        if lines == 1:
            if line != "U8REC 1":
                sys.exit(f"{path}: not a recording")
            continue
        if line == "end":
            break
        key, _, value = line.partition("=")
        if key == "arg":
            args.append(value)
        else:
            header[key] = value
    header["args"] = " ".join(args)
    return header, data, at


def operations(data, at):
    """Yields (offset, op, idle, fields) for each operation."""

    def number():
        nonlocal at
        v = 0
        shift = 0
        while True:
            b = data[at]
            at += 1
            v |= (b & 0x7F) << shift
            if not b & 0x80:
                return v
            shift += 7

    while at < len(data):
        start = at
        op = data[at]
        at += 1
        idle = number()
        if op == 1:
            fields = {"step": number()}
        elif op == 2:
            fields = {"step": number()}
            events = []
            for _ in range(number()):
                b = data[at]
                at += 1
                kind = EVENT_KINDS.get(b & 0x7F, f"kind{b & 0x7F}")
                e = {"kind": kind, "pressed": bool(b & 0x80)}
                if kind == "key":
                    e["key"] = number()
                    e["mod"] = number()
                elif kind == "move":
                    e["x"] = number()
                    e["y"] = number()
                elif kind == "button":
                    e["button"] = number()
                events.append(e)
            fields["events"] = events
        elif op == 3:
            tag = number()
            fields = {"value": VALUES.get(tag, f"tag{tag}"), "is": number()}
        elif op == 4:
            fields = {"ms": number(), "digest": int.from_bytes(data[at:at + 8], "little")}
            at += 8
        elif op == 5:
            z = number()
            fields = {"code": (z >> 1) ^ -(z & 1)}
        else:
            sys.exit(f"unknown operation {op} at byte {start}")
        yield start, op, idle, fields


def main():
    if hasattr(signal, "SIGPIPE"):
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    if len(sys.argv) != 3 or sys.argv[1] not in ("info", "dump"):
        sys.exit(__doc__)
    header, data, at = read_session(sys.argv[2])
    quantum = int(header.get("quantum", "1000"))
    game_us = 0
    pumps = 0
    counts = {}
    for start, op, idle, fields in operations(data, at):
        pumps += idle
        counts[OPS[op]] = counts.get(OPS[op], 0) + 1
        if op in (1, 2):
            pumps += 1
            game_us += fields["step"] * quantum
        if sys.argv[1] == "dump":
            shown = dict(fields)
            if "digest" in shown:
                shown["digest"] = f"{shown['digest']:016x}"
            print(f"{game_us / 1e6:10.3f}s  byte {start:8}  {OPS[op]:6}  idle {idle:4}  {shown}")
    if sys.argv[1] == "info":
        for key, value in header.items():
            print(f"{key}: {value}")
        print(f"game time: {game_us / 1e6:.3f} s, {pumps} pumps, {len(data) - at} bytes")
        for name in OPS.values():
            print(f"  {name}: {counts.get(name, 0)}")
        if "END" not in counts:
            print("  (no END: the session crashed or was killed, or is still running)")


if __name__ == "__main__":
    main()
