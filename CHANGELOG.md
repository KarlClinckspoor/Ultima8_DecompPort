# Changes

Each release's notes. The newest is first; `ultima8-portable/tools/release/notes.sh VERSION`
prints one section, and the release on GitHub carries it.

## v0.1.0

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
