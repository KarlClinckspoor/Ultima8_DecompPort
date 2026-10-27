#!/usr/bin/env python3
"""Makes a new language for Ultima VIII out of a table of translated strings.

  u8lang.py extract ENGLISH [--compare FRENCH GERMAN] -o table.json
      Lists the strings of ENGLISH/USECODE/EUSECODE.FLX, once each, to translate. With
      --compare, the strings that are the same in every language Origin shipped (names of
      files, debugging, words of power) are marked to keep as they are.

  u8lang.py build ENGLISH table.json OUTPUT
      Copies the ENGLISH game folder to OUTPUT with the strings of the table, the letters the
      table needs added to the fonts, and the recorded voices left out (their lines then show
      as text). The new folder still says it is English, so buttons, menus and the intro stay
      English and it plays in DOS U8.EXE as in the port.

The table: {"keep": [[class, n], ...], "strings": [{"en": ..., "tr": ..., "cls": ...}]}.
Strings with the same English get the same translation everywhere, so code that compares a
string with another still finds them equal. Classes whose names hold GRAVE or PLAQUE are
engraved in fonts with no accents, so their accents are dropped.
"""
import argparse
import glob
import json
import os
import shutil
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fonts
import usecode

MARKUP = '~*%\t'
QUOTES = {'’': "'", '‘': "'", '“': '"', '”': '"', '—': '-', '–': '-',
	'…': '...', ' ': ' '}


def find(folder, name):
    """A file in a DOS game folder, whatever its case on disk."""
    for part in name.split('/'):
        hit = [e for e in os.listdir(folder) if e.lower() == part.lower()]
        folder = os.path.join(folder, hit[0] if hit else part)
    return folder


def usecode_file(folder):
    """The usecode of a language folder: its own (FUSECODE, GUSECODE) before the English one."""
    names = sorted(os.listdir(find(folder, 'USECODE')), key=lambda n: n.upper().startswith('E'))
    for n in names:
        if n.upper().endswith('USECODE.FLX'):
            return os.path.join(find(folder, 'USECODE'), n)
    raise SystemExit('no usecode in %s' % folder)


def extract(args):
    e = open(find(args.english, 'USECODE/EUSECODE.FLX'), 'rb').read()
    others = [usecode.classes(open(usecode_file(f), 'rb').read()) for f in args.compare or []]
    names = usecode.class_names(e)
    keep, strings, seen = [], [], set()
    for c, rec in sorted(usecode.classes(e).items()):
        mine = usecode.strings_of(rec)
        theirs = []
        for o in others:
            s = usecode.strings_of(o[c]) if c in o else None
            theirs.append(s if s is not None and len(s) == len(mine) else None)
        for n, s in enumerate(mine):
            if others and all(t is None or t[n] == s for t in theirs):
                keep.append([c, n])
                continue
            text = s.decode('cp437')
            if text not in seen:
                seen.add(text)
                strings.append({'cls': '%d %s' % (c, names.get(c, '')), 'en': text, 'tr': ''})
    json.dump({'keep': keep, 'strings': strings}, open(args.output, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print('%d strings to translate, %d kept as they are' % (len(strings), len(keep)))


def encode(text, engraved):
    text = ''.join(QUOTES.get(ch, ch) for ch in text)
    if engraved:
        text = ''.join(ch for ch in unicodedata.normalize('NFD', text) if unicodedata.category(ch) != 'Mn')
    out = bytearray()
    for ch in text:
        if ch in fonts.ENCODING:
            out.append(fonts.ENCODING[ch])
        elif ord(ch) < 128:
            out.append(ord(ch))
        else:
            raise ValueError('the fonts have no %r (in %r)' % (ch, text))
    return bytes(out)


def problems(item):
    en, tr = item['en'], item['tr']
    p = []
    if len(en) - len(en.lstrip(' ')) != len(tr) - len(tr.lstrip(' ')) or len(en) - len(en.rstrip(' ')) != len(tr) - len(tr.rstrip(' ')):
        p.append('spaces at the ends differ')
    for m in MARKUP:
        if en.count(m) != tr.count(m) and not (m == '*' and ('GRAVE' in item['cls'] or 'PLAQUE' in item['cls'])):
            p.append('%r: %d in the English, %d here' % (m, en.count(m), tr.count(m)))
    return p


def build(args):
    table = json.load(open(args.table, encoding='utf-8'))
    tr, warned = {}, 0
    for it in table['strings']:
        if it['tr']:
            tr[it['en']] = it
            for p in problems(it):
                warned += 1
                print('warning: %s: %r: %s' % (it['cls'], it['en'][:40], p))
    keep = {tuple(k) for k in table['keep']}
    e = open(find(args.english, 'USECODE/EUSECODE.FLX'), 'rb').read()
    new, missing = {}, 0
    for c, rec in usecode.classes(e).items():
        out = []
        for n, s in enumerate(usecode.strings_of(rec)):
            it = tr.get(s.decode('cp437'))
            if (c, n) in keep or it is None:
                missing += (c, n) not in keep
                out.append(s)
            else:
                out.append(encode(it['tr'], 'GRAVE' in it['cls'] or 'PLAQUE' in it['cls']))
        new[c] = out
    code = usecode.rebuild(e, new)
    if os.path.exists(args.output):
        raise SystemExit('%s exists; give a new folder' % args.output)
    shutil.copytree(args.english, args.output, ignore=shutil.ignore_patterns('*.orig'))
    gamedat = find(args.output, 'GAMEDAT')
    if os.path.isdir(gamedat):
        shutil.rmtree(gamedat)
        os.mkdir(gamedat)
    for root, _, files in os.walk(args.output):
        for f in files:
            os.chmod(os.path.join(root, f), 0o644)
    open(find(args.output, 'USECODE/EUSECODE.FLX'), 'wb').write(code)
    fp = find(args.output, 'STATIC/U8FONTS.FLX')
    font_data = fonts.add_portuguese(open(fp, 'rb').read())
    open(fp, 'wb').write(font_data)
    for p in glob.glob(os.path.join(find(args.output, 'SOUND'), '*')):
        name = os.path.basename(p).upper()
        if name[:1] == 'E' and name[1:2].isdigit() and name.endswith('.FLX'):
            os.remove(p)	# recorded voices: they match the English text only
    biggest = max(len(r) for r in usecode.classes(code).values())
    print('%s: %d strings translated, %d left in English, largest class %d bytes, %d warnings'
        % (args.output, len(tr), missing, biggest, warned))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    x = sub.add_parser('extract')
    x.add_argument('english')
    x.add_argument('--compare', nargs='*')
    x.add_argument('-o', '--output', required=True)
    b = sub.add_parser('build')
    b.add_argument('english')
    b.add_argument('table')
    b.add_argument('output')
    args = ap.parse_args()
    extract(args) if args.cmd == 'extract' else build(args)


if __name__ == '__main__':
    main()
