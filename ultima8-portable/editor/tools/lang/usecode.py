"""Ultima VIII usecode (EUSECODE.FLX and its kin): read the code, swap its strings, write it back.

A flex file: a 0x80-byte header (record count at 0x54, a checksum at 0x60: the signed sum of the
bytes from 0x80 to 0x80 before the end), then 8 bytes a record (offset, size). Record 1 is the
overload table; record c + 2 is usecode class c: a 12-byte header (three numbers that all move
with the size), then the code, which begins with 32 event offsets (u32, relative to the code).

The interpreter is game/u8/inter.cpp; what moves when a string changes length:
  51, 52 branch          rel16 from the end of the instruction (16-bit: it wraps at 64K)
  75, 76 list loops      rel16 in operand bytes 3-4, from the end of the instruction
  11 call far            u16 class, u16 offset
  57 spawn               u16 class, u16 offset in operand bytes 3-6
  58 spawn inline        u16 class, u16 routine start, s16 delta to the code it runs
  the event table        u32 offsets
"""
import struct

OPLEN = {0x00:1,0x01:1,0x02:1,0x03:2,0x08:0,0x09:3,0x0a:1,0x0b:2,0x0c:4,0x0e:2,0x0f:3,0x10:4,0x11:4,
 0x12:0,0x13:0,0x14:0,0x15:0,0x16:0,0x17:0,0x18:1,0x19:1,0x1a:1,0x1b:1,0x38:2,0x3e:1,0x3f:1,0x40:1,
 0x41:1,0x42:2,0x43:1,0x44:2,0x45:2,0x4b:1,0x4c:1,0x4d:1,0x4e:3,0x4f:3,0x50:0,0x51:2,0x52:2,0x53:0,
 0x54:2,0x55:0,0x56:0,0x57:6,0x58:8,0x59:0,0x5a:1,0x5b:2,0x5d:0,0x5e:0,0x5f:0,0x60:0,0x61:0,0x62:1,
 0x63:1,0x64:1,0x65:1,0x66:1,0x67:1,0x68:0,0x69:1,0x6a:0,0x6b:0,0x6c:2,0x6d:0,0x6e:1,0x6f:1,0x70:3,
 0x73:0,0x74:1,0x75:4,0x76:4,0x77:0,0x78:0,0x79:0}
for _op in list(range(0x1c, 0x38)) + [0x39, 0x3a, 0x3b, 0x3c, 0x3d]:
    OPLEN.setdefault(_op, 0)


def flex(d):
    """[(offset, size)] for every record."""
    n = struct.unpack_from('<I', d, 0x54)[0]
    return [struct.unpack_from('<II', d, 0x80 + 8 * i) for i in range(n)]


def checksum(d):
    return sum((b - 256 if b > 127 else b) for b in d[0x80:len(d) - 0x80]) & 0xffffffff


def write_flex(original, records, with_checksum):
    """A flex file with the original's header and these records, laid out in the order the
    original's are (EUSECODE.FLX keeps records 0 and 1 at the end)."""
    head = bytearray(original[:0x80])
    struct.pack_into('<I', head, 0x54, len(records))
    index = bytearray(8 * len(records))
    body = bytearray()
    at = 0x80 + 8 * len(records)
    old = flex(original)
    place = lambda i: old[i][0] if i < len(old) and old[i][1] else float('inf')
    for i in sorted(range(len(records)), key=lambda i: (place(i), i)):
        r = records[i]
        if r:
            struct.pack_into('<II', index, 8 * i, at + len(body), len(r))
            body += r
    out = bytearray(head + index + body)
    if with_checksum:
        struct.pack_into('<I', out, 0x60, checksum(out))
    return bytes(out)


def decode(code, start=0x80):
    """Yields (offset, opcode, length) for the instructions of a class's code."""
    at = start
    while at < len(code):
        op = code[at]
        if op == 0x0d:		# push string: u16 length, the characters, a NUL
            size = 3 + struct.unpack_from('<H', code, at + 1)[0] + 1
        elif op == 0x5c:	# debug unit: u16, then a 9-byte name
            size = 12
        elif op in OPLEN:
            size = 1 + OPLEN[op]
        else:
            raise ValueError('bad opcode %02x at %x' % (op, at))
        yield at, op, size
        at += size
    if at != len(code):
        raise ValueError('the last instruction runs past the end')


def class_names(d):
    o, s = flex(d)[1]
    t = d[o:o + s]
    return {i: t[i * 13 + 4:i * 13 + 13].split(b'\0')[0].decode('latin1') for i in range(s // 13)}


def classes(d):
    """{class: record} for every class that has code."""
    return {i - 2: d[o:o + s] for i, (o, s) in enumerate(flex(d)) if i >= 2 and s}


def strings_of(record):
    code = record[12:]
    return [code[at + 3:at + size - 1] for at, op, size in decode(code) if op == 0x0d]


def rebuild(d, newstrings):
    """The file with new strings: {class: [bytes, one for each of its strings, in order]}.
    Classes left out keep theirs. Everything that points into code is moved to match."""
    recs = classes(d)
    maps, built = {}, {}
    for c, rec in recs.items():
        code = rec[12:]
        ins = list(decode(code))
        strs = newstrings.get(c)
        si = 0
        new = bytearray(code[:0x80])
        m = {}
        for at, op, size in ins:
            m[at] = len(new)
            if op == 0x0d:
                s = strs[si] if strs is not None else code[at + 3:at + size - 1]
                si += 1
                if b'\0' in s or len(s) > 0xffff:
                    raise ValueError('class %d: string %d cannot be stored' % (c, si - 1))
                new += bytes([0x0d]) + struct.pack('<H', len(s)) + s + b'\0'
            else:
                new += code[at:at + size]
        m[len(code)] = len(new)
        if strs is not None and si != len(strs):
            raise ValueError('class %d has %d strings, %d given' % (c, si, len(strs)))
        maps[c] = m
        built[c] = (new, ins)

    def where(c, off):
        if off not in maps[c]:
            raise ValueError('class %d: offset %x is not an instruction' % (c, off))
        return maps[c][off]

    out = {}
    for c, (new, ins) in built.items():
        code = recs[c][12:]
        m = maps[c]
        for e in range(32):
            off = struct.unpack_from('<I', code, e * 4)[0]
            if off:
                struct.pack_into('<I', new, e * 4, where(c, off))
        for at, op, size in ins:
            na = m[at]
            if op in (0x51, 0x52, 0x75, 0x76):
                pos = 1 if op in (0x51, 0x52) else 3
                rel = struct.unpack_from('<h', code, at + pos)[0]
                target = (at + size + rel) & 0xffff
                nrel = (where(c, target) - (na + size)) & 0xffff
                struct.pack_into('<H', new, na + pos, nrel)
            elif op in (0x11, 0x57):
                pos = 1 if op == 0x11 else 3
                unit, off = struct.unpack_from('<HH', code, at + pos)
                if unit in maps:
                    struct.pack_into('<HH', new, na + pos, unit, where(unit, off))
            elif op == 0x58:
                unit, base, delta = struct.unpack_from('<HHh', code, at + 1)
                if unit in maps:
                    target = where(unit, base + delta)
                    nb = maps[unit].get(base, base)
                    struct.pack_into('<HHh', new, na + 1, unit, nb, target - nb)
        if len(new) + 12 > 0xffff:
            raise ValueError('class %d would be %d bytes; a class must stay under 64K' % (c, len(new) + 12))
        a, size, count = struct.unpack_from('<III', recs[c], 0)
        delta = len(new) + 12 - size
        out[c] = struct.pack('<III', a + delta, size + delta, count + delta) + bytes(new)
    records = [d[o:o + s] for o, s in flex(d)]
    for c, b in out.items():
        records[c + 2] = b
    return write_flex(d, records, struct.unpack_from('<I', d, 0x60)[0] != 0)
