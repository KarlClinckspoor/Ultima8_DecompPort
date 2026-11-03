# Changes

Each release's notes. The newest is first; `ultima8-portable/tools/release/notes.sh VERSION`
prints one section, and the release on GitHub carries it. A release is made by writing its
section here and putting its number in `VERSION` (see RELEASING.md).

## Unreleased

- **Editor: the Flags tab.** Every global flag the usecode uses, with its value in the saved
  game (changes are saved to its `FLAG.DAT`), the classes that read and write it, where in the
  usecode, with the strings around each use, and names and notes of your own.
- **Editor: item names.** The Item window and the map's tooltip give an item's name as the
  game shows it when looked at, by running the item's look routine from the usecode in a copy
  of the game's interpreter, against its quality, frame and the saved game's flags.

## 0.2.0

The editor can now make the game in another language, with Brazilian Portuguese included. The
packages are as in 0.1.0 and need the same: your own copy of the game, Windows 10 or 11, or
Linux with glibc 2.38 or newer.

- **Editor: File > Apply a Translation.** Makes a translated copy of the English game folder
  from a translation table: the usecode's strings swapped (every branch and call moved to
  match), the letters the language needs added to the fonts from a small font patch, and, by
  choice, the English voices left out so that their lines show as text. The editor comes with
  the Brazilian Portuguese table (`languages/pt_BR.json`) and its font patch. The result is
  the same, byte for byte, as `u8lang.py build`.
- `u8lang.py fontpatch` writes a font patch; `u8lang.py build --keep-voices`; the tool runs
  with `uv run u8lang.py ...`, with no Python to install.

## 0.1.0

The first release of the portable Ultima VIII and its editor, for Windows and Linux (64-bit).
Both need your own copy of the game: nothing of the game is included.

**Ultima VIII, portable** (`ultima8-…`): the decompiled game built for today's systems.

- The whole game, with the emulated Roland MT-32 (given its ROMs, not included) or AdLib for
  music and the Sound Blaster for speech and effects.
- A settings screen (F11): window, scaling, fullscreen, volumes, the game folder.
- Saved games and settings in a home folder of their own; the game folder is only read.
- Every session recorded and replayable exactly (`--replay`), for checking that changes keep
  the game the same.
- The weapon, armour and monster statistics read from the game folder's U8.EXE, so changes
  made with the editor play the same here as in DOS.

**U8 Editor** (`u8edit-…`): an editor for the game's world and data, in Qt 6.

- Every map, drawn as the game draws it, with layers, eggs and their ranges, a minimap and a
  graph of the ways between maps.
- Place, move, copy, delete and change items and eggs; NPCs' places and statistics; the
  weapon, armour and monster tables. Undo and redo. What it saves plays in DOS U8.EXE too.
- Test in the Port (F5): save, and start a new game in the port.

**Also in the repository** (not packaged): `ultima8-portable/editor/tools/lang/`, which makes
a new language from a table of translated strings, with a complete Brazilian Portuguese table;
and the writeups `GAME-MECHANICS.md`, `DECOMPILATION-LESSONS.md` and
`ultima8-portable/PORT-LESSONS.md`.

Needs: Windows 10 or 11 with a Vulkan graphics driver; Linux with glibc 2.38 or newer
(Ubuntu 24.04, Debian 13, Fedora 39 or later).

Known limits: the port's Windows build draws with Vulkan only; macOS builds are not made.
