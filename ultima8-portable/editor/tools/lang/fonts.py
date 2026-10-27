"""Accented letters for U8FONTS.FLX.

A font is a shape whose frame n is character n. A shape: max width, max height and frame count
(u16 each), then 6 bytes a frame (u24 offset, a byte, u16 length). A frame: 8 unused bytes,
then compression, width, height, x and y hot spot (u16 each), one u16 a row (the row's data,
relative to that word), then skip/run pairs (u8/skipdraw.h). Every glyph of a font has the same
height, accents included, so a new accented letter is the base letter with the accent rows of
a letter that already has the accent; a capital is squeezed down to start where the small
letters do, as Origin drew N with a tilde and A with a diaeresis.
"""
import struct
from usecode import flex, write_flex

# The fonts that show dialogue, books, scrolls and conversations (u8/bark.cpp, u8/msgproc.cpp,
# item/book.cpp, u8/u8dispat.cpp). The gravestone and plaque fonts (10, 11) are capitals only.
TEXT_FONTS = [0, 1, 5, 6, 7, 8, 9, 14, 15]

# new code point (CP850): base letter, a letter with the accent, that letter's base
SMALL = {0xC6: ('a', 0xA4, 'n'), 0xE4: ('o', 0xA4, 'n')}		# a o with tilde
CAPITAL = {0x90: ('E', 0x82, 'e'), 0xB5: ('A', 0xA0, 'a'), 0xB6: ('A', 0x83, 'a'),
	0xB7: ('A', 0x85, 'a'), 0xC7: ('A', 0xA4, 'n'), 0xD2: ('E', 0x88, 'e'), 0xD6: ('I', 0xA1, 'i'),
	0xE0: ('O', 0xA2, 'o'), 0xE2: ('O', 0x93, 'o'), 0xE5: ('O', 0xA4, 'n'), 0xE9: ('U', 0xA3, 'u')}

# Text to the bytes these fonts draw: CP437 where it has the letter, CP850 for the rest.
ENCODING = {'á': 0xA0, 'à': 0x85, 'â': 0x83, 'ã': 0xC6, 'é': 0x82, 'ê': 0x88, 'í': 0xA1, 'ó': 0xA2,
	'ô': 0x93, 'õ': 0xE4, 'ú': 0xA3, 'ü': 0x81, 'ç': 0x87, 'Ç': 0x80, 'É': 0x90, 'Á': 0xB5,
	'Â': 0xB6, 'À': 0xB7, 'Ã': 0xC7, 'Ê': 0xD2, 'Í': 0xD6, 'Ó': 0xE0, 'Ô': 0xE2, 'Õ': 0xE5,
	'Ú': 0xE9}


def frames(shape):
    n = struct.unpack_from('<H', shape, 4)[0]
    return [shape[6 + f * 6] | shape[7 + f * 6] << 8 | shape[8 + f * 6] << 16 for f in range(n)]


def decode(shape, offset):
    h = offset + 8
    comp, w, ht, xo, yo = struct.unpack_from('<Hhhhh', shape, h)
    pix = [[None] * w for _ in range(max(ht, 0))]
    for r in range(max(ht, 0)):
        e = h + 10 + r * 2
        p = e + struct.unpack_from('<H', shape, e)[0]
        x = 0
        while x < w:
            x += shape[p]
            p += 1
            if x >= w:
                break
            run = shape[p]
            p += 1
            fill = False
            if comp & 1:
                fill = run & 1
                run >>= 1
            for _ in range(run):
                if x >= w:
                    break
                pix[r][x] = shape[p]
                if not fill:
                    p += 1
                x += 1
            if fill:
                p += 1
    return dict(w=w, h=ht, xo=xo, yo=yo, pix=pix)


def encode(g):
    w, h = g['w'], g['h']
    rows = []
    for row in g['pix']:
        out = bytearray()
        x = 0
        while x < w:
            s = x
            while x < w and row[x] is None:
                x += 1
            if x >= w:
                out.append(x - s)
                break
            skip = x - s
            s = x
            while x < w and row[x] is not None and x - s < 255:
                x += 1
            out += bytes([skip, x - s]) + bytes(row[s:x])
        rows.append(bytes(out))
    head = bytes(8) + struct.pack('<Hhhhh', 0, w, h, g['xo'], g['yo'])
    offsets, body = bytearray(), bytearray()
    for r, data in enumerate(rows):
        offsets += struct.pack('<H', 18 + 2 * h + len(body) - (18 + 2 * r))
        body += data
    return head + bytes(offsets) + bytes(body)


def _top(g):
    return next((r for r, row in enumerate(g['pix']) if any(v is not None for v in row)), g['h'])


def _bottom(g):
    return next((r for r in range(g['h'] - 1, -1, -1) if any(v is not None for v in g['pix'][r])), -1)


def _shift(row, dx, w):
    out = [None] * w
    for x, v in enumerate(row):
        if 0 <= x + dx < w:
            out[x + dx] = v
    return out


def _accent(target, ref, ref_base):
    """target with the rows of ref above where ref_base starts (the row it starts on merged)."""
    w = target['w']
    t = _top(ref_base)
    dx = (w - ref['w']) // 2
    pix = [list(r) for r in target['pix']]
    for r in range(t + 1):
        acc = _shift(ref['pix'][r], dx, w)
        if r < t:
            pix[r] = acc
        else:
            pix[r] = [b if b is not None else a for a, b in zip(acc, pix[r])]
    return dict(target, pix=pix)


def _squeeze(g, newtop):
    """The capital moved down to start at newtop, losing rows that repeat their neighbour."""
    t, b = _top(g), _bottom(g)
    rows = [list(r) for r in g['pix'][t:b + 1]]
    while len(rows) > b - newtop + 1:
        mid = len(rows) / 2
        same = [i for i in range(1, len(rows) - 2) if rows[i] == rows[i + 1]]
        if same:
            i = min(same, key=lambda i: abs(i - mid))
        else:
            i = min(range(1, len(rows) - 2),
                key=lambda i: (sum(a != c for a, c in zip(rows[i], rows[i + 1])), abs(i - mid)))
        del rows[i]
    pix = [[None] * g['w'] for _ in range(g['h'])]
    for k, r in enumerate(rows):
        pix[newtop + k] = r
    return dict(g, pix=pix)


def add_portuguese(fontsflx):
    """U8FONTS.FLX with the letters ENCODING needs in every text font."""
    records = [fontsflx[o:o + s] for o, s in flex(fontsflx)]
    for f in TEXT_FONTS:
        shape = bytearray(records[f])
        offs = frames(shape)
        glyph = lambda c: decode(shape, offs[c if isinstance(c, int) else ord(c)])
        new = {}
        for code, (base, ref, ref_base) in SMALL.items():
            new[code] = _accent(glyph(base), glyph(ref), glyph(ref_base))
        for code, (cap, ref, ref_base) in CAPITAL.items():
            if any(v is not None for row in glyph(code)['pix'] for v in row):
                continue		# the font has it already
            squeezed = _squeeze(glyph(cap), _top(glyph(ref_base)))
            new[code] = _accent(squeezed, glyph(ref), glyph(ref_base))
        for code, g in new.items():
            data = encode(g)
            at = len(shape)
            shape[6 + code * 6:12 + code * 6] = bytes([at & 0xff, (at >> 8) & 0xff, at >> 16, 0]) + struct.pack('<H', len(data))
            shape += data
        records[f] = bytes(shape)
    return write_flex(fontsflx, records, struct.unpack_from('<I', fontsflx, 0x60)[0] != 0)
