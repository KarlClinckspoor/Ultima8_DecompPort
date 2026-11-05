# Lessons from the decompilation

The decompilation in `source/` is Jamie Blanks's work. It rebuilds U8.EXE byte for byte with
Borland C++ 3.1 (see [BUILD.md](BUILD.md)). These notes come from working on top of it: porting
it to modern systems (`ultima8-portable/`), then writing an editor (`ultima8-portable/editor/`)
and a language tool (`ultima8-portable/editor/tools/lang/`) that read and write the game's
files. They cover what the decompiled source tells you, what it does not, and how to check
what you learn from it. [ultima8-portable/PORT-LESSONS.md](ultima8-portable/PORT-LESSONS.md)
covers the port itself.

## 1. The rebuilt executable is the documentation

The C answers most questions, but some it cannot answer, because they are settled by the
compiler and not by the source. A source that rebuilds the original byte for byte comes with
the means to ask: `out/U8.EXE` and its map (`out/U8.MAP`) say exactly where every function
and every table is, and the code Borland made from each line.

Questions the C could not answer, and the binary did:

- **What a `void` function "returned".** Borland left whatever the last call put in AX, and
  usecode, which declares its intrinsics on its own terms, read it: `Item::ask` returns the pid
  of the answer menu's process, and the conversation waits on that pid. In the C it is a
  `void` function. Only the code after the call in U8.EXE shows what AX held.
- **What a saved block of memory held.** `KERNEL.DAT` saves 157 bytes of the data segment as
  they lie. The C names the variables it meant to save, but the block also holds the
  real-time list's count, and without it no timer runs after a load. The map shows what
  else lies in a range of the data segment.
- **Where data is.** The weapon, armour and monster statistics are not in any data file. They
  are initialised arrays in U8.EXE's data segment (`u8/wpncache.cpp`, `u8/armordat.cpp`,
  `u8/mondata.cpp`), and an editor that changes them has to patch the executable. The source
  gives the layout; the map gives the place.

Keep the DOS build working when you change anything shared with it. As long as it still
matches, these questions can still be asked.

## 2. The source is faithful to Borland, not to C

The decompiled source is right for its compiler: it compiles to the original bytes. It is
not written to the C or C++ standard, and much of what it relies on is invisible in the text:

| What the source relies on | Where it bit |
| --- | --- |
| `int` is 16 bits, so arithmetic wraps at 16 bits, and `unsigned op int` is unsigned | comparisons, shifts and divisions that change meaning on a 32-bit `int` (`tools/check_int16.py` lists them) |
| `char` is signed | text and table lookups with high characters |
| a `void` function's value is AX | intrinsics usecode waits on |
| string literals are writable | the language letter written into `"eusecode.flx"` |
| an unexpanded inline is emitted in every file that uses it ("virtual segments") | files that call header inlines with no definition in sight; it links at one optimisation level and not at another |
| `rand()` is Borland's generator, 15 bits | the host's reaches 2^31 and overran Grant Peace's sound table |
| `printf` formats are Borland's (`%u` and `%x` are 16 bits, `%Fs` a far pointer) | every print of a 16-bit value |
| structs are packed to one byte | every structure read from or written to a file |
| a far pointer is a selector and an offset, and a block starts at offset 0 | handles kept as selectors, offsets kept as the low word of a pointer, usecode's own far pointers |

The lesson: no single line reveals any of these. Read the source as a description of what the
compiler did, list what that compiler did differently from yours, and then look for each kind
of difference everywhere at once with a tool (`check_int16.py`, `-Wtype-limits
-Wsign-compare`, `-Werror=return-type`). Do not rely on finding them one crash at a time.

## 3. What is not in the decompilation still has to be done

The decompilation leaves its helper libraries as binaries: the Phar Lap DOS extender, the
Miles sound libraries (`AIL16`) and the Miles XMIDI driver the installer copies in
(`AILXMI.DLL`). They are "not very high value", as the README says, for rebuilding U8.EXE. A
port has to replace each of them: the extender's selectors and descriptor tables became plain
pointers, and the XMIDI driver became a sequencer written for the port, with MT-32 and AdLib
synth drivers. Count these early when estimating a port: what was left as a binary is what
the decompilation cannot tell you.

## 4. Names make it navigable; the intrinsics table makes it usable

The shipped build carried debug data, so most file and function names are original
(`Grave::read`, `Plaque::read`, `TeleporterProcess`, `ItemCache::loadMapLevel`). The usecode
intrinsics table (`u8/intrinsics.cpp`) gives each intrinsic's number and its C++ signature
(`"Grave::read(short,char far*)"`). With those, a question like "which font does a
gravestone use?" is answered by searching for a name rather than by reading assembly:
`Grave::read` opens a `GraveGump` with font 11, `Plaque::read` with font 10.

Use the names as the index to the game. Every file format the editor and the language tool
handle was found by reading the function that reads that format, not by guessing from the
bytes.

## 5. Read formats from the reader, and the rules hiding in them

| Format | Read from | What the code said that the bytes did not |
| --- | --- | --- |
| Usecode (`EUSECODE.FLX`) | `u8/inter.cpp` (the interpreter) | which operands point into code: branches are relative to the end of the instruction, list loops keep theirs in operand bytes 3 and 4, calls and spawns name a class and an offset, an inline spawn names a routine and a delta. Branches are 16 bits and wrap, and some classes are over 32K. |
| Flex files | `FlexFile` | the checksum at 0x60, where a file has one, is the signed sum of the bytes from 0x80 to 0x80 before the end; records need not be stored in index order (EUSECODE keeps records 0 and 1 at the end) |
| Fonts (`U8FONTS.FLX`) | `CacheFont` | a font is a shape and frame *n* is character *n*; every glyph of a font has the same height, so accents sit inside the cell |
| Gravestones and plaques | `GraveGump::draw` | `*` breaks a line, lines are centred and not wrapped, and the text is clipped to a rectangle from `GUMPAGE.DAT` |
| Speech (`SOUND/E<npc>.FLX`) | `SpeechFlex::search`, `playSpeech` | a voice plays only when the text starts with a recorded phrase, and no text is shown while it does |
| The language | `Avatar::getLanguageChar`, the gumps | it is byte 54 of `AVATAR.DAT` in the saved game; 15 letters are defined, but the yes/no buttons, the statistics labels and the quit menu are tables for three languages only |
| The starting save | `ItemCache::save`/`load` | `U8SAVE.000` was saved with map 3 loaded, and a new game takes map 3 from the item cache, not from `FIXED.DAT` and `NONFIXED.DAT` |

Two of these decided a design. The language table shows that a new language number would
need art for every button. So the Portuguese version keeps the English number and swaps only
the text. The speech rule shows that translated text and English voices cannot play together.
So the voices are left out, as Origin did for French and German.

## 6. Use the original data as the test

The game shipped with more data than it needs, and it makes the best test of anything that
writes the game's files:

- **Identity.** Rebuild a file with nothing changed and compare it with the original: every
  map of `FIXED.DAT` and `NONFIXED.DAT`, every class of `EUSECODE.FLX`. This is what showed
  that EUSECODE stores records 0 and 1 at the end.
- **Origin's own translations.** French and German share the English code. Putting the French
  strings into the English usecode with the relocating rebuilder gives FUSECODE.FLX byte for
  byte for 512 of the 516 classes; the other four are classes whose code Origin changed for
  French. That confirmed every relocation rule in the table above before a single Portuguese
  string went in. It also caught the 16-bit wrap: class 1034 is 47K, and its branches past
  32K only resolve modulo 64K.
- **What Origin kept.** 906 string positions are identical in English, French and German:
  file names, debugging messages, words of power. Origin left them alone, so the translation
  does too.

## 7. Expect leftovers in the data

The data holds things the code never reaches, or reaches oddly. Know about them before you
trust a count or a search:

- the English usecode contains whole German and French conversation lines, identical in all
  three languages, left over from development;
- spell names are translated in some places and identical in all three languages in others,
  in what look like debugging menus;
- empty usecode records, and fonts with fewer than 256 frames: the carved fonts of the
  gravestones and plaques stop at character 124 and have no accented letters, so German
  gravestones cannot have umlauts either;
- the English, French and German starting saves differ in more than the language byte:
  `NONFIXED.DAT` and `ITEMCACH.DAT` are different. Build a new language from the English
  save, not by patching one byte into another language's.

## 8. When the port misbehaves, suspect an assumption first

Every fault found while bringing the port up was a correct line of the original running
under a different assumption, not a bug in the original: a sound table indexed with
`rand() % n`, where the host's `rand()` is 31 bits and Borland's 15; a `NewGumpId` check that
relied on 16-bit wrap-around; restored processes built with a class's own `operator new`
where the original used the global one. So, when the port goes wrong, ask first what the
original took for granted at that line. Section 2 is the list to check against, and
[ultima8-portable/PORTING.md](ultima8-portable/PORTING.md) has the full list of traps.

## In short

- A byte-exact rebuild is a tool, not only a result: keep it, and ask it what the C cannot
  say.
- List what the original compiler did and your compiler does not. Then search for each item
  everywhere, with tools.
- Find a format in the code that reads it, and copy its rules, including the odd ones.
- Test a writer of game files against the original data, identity first, then against any
  second version the game shipped (other languages, other saves).
