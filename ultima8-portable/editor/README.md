# U8 Editor (u8edit)

An editor for Ultima VIII's world and game data, for making new games out of it. It reads and
writes the game's own files the way the game does (the decompiled sources in `game/item` and
`game/u8` are the reference for every format), so what it saves plays in DOS U8.EXE as well as
in the port.

## Building

It needs Qt 6.2 or newer (Widgets only) and CMake. It is a project of its own, apart from the
game:

    cmake -S editor -B build/editor -DCMAKE_BUILD_TYPE=Release
    cmake --build build/editor

### Windows, with MSYS2

[MSYS2](https://www.msys2.org) installs Qt, the compiler and CMake from its package manager,
with no Qt account or installer. Use its **UCRT64** terminal (the *MSYS* terminal is for
MSYS2's own tools, and its packages do not make Windows programs):

    pacman -Syu
    pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,qt6-base,qt6-tools,qt6-translations}

`pacman -Syu` may close the terminal to finish updating itself: open UCRT64 again and run it
once more before the second line. Install every package with the same `ucrt-x86_64` prefix;
packages for another environment (`x86_64` is MINGW64's) do not mix.

Then, from the repository's top folder (MSYS2 names `C:\` as `/c/`):

    cmake -S ultima8-portable/editor -B build/editor -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build/editor
    ./build/editor/u8edit --data /c/path/to/Ultima8/ENGLISH

Started from the UCRT64 terminal it finds Qt's DLLs on that terminal's `PATH`. To start it by
double-clicking, or to copy it elsewhere, put the DLLs it needs beside it:

    windeployqt6 build/editor/u8edit.exe
    ldd build/editor/u8edit.exe build/editor/*/*.dll |
        grep -i ucrt64 | awk '{print $3}' | sort -u | xargs -I{} cp {} build/editor/

`windeployqt6` copies Qt's own DLLs and plug-ins. The `ldd` line copies the rest that MSYS2
builds need and `windeployqt6` leaves out: the compiler's run-time (`libstdc++-6.dll`,
`libgcc_s_seh-1.dll`, `libwinpthread-1.dll`) and the libraries Qt uses (ICU, FreeType,
HarfBuzz, zlib, ...). `windeployqt6` stops with *Cannot open .../translations/catalogs.json*
when `qt6-translations` is not installed: install it, or add `--no-translations`.

Qt here is LGPL: it stays in its own DLLs beside `u8edit.exe`, which is how the LGPL lets it
be shipped with a program.

### Windows, with Qt's installer

Point CMake at Qt with `-DCMAKE_PREFIX_PATH=C:\Qt\6.x\msvc2022_64` and run `windeployqt` on
the result to ship it.

`-DU8EDIT_TESTS=ON` also builds `u8edit-tests`, which works on a copy of the game data:

    U8_DATA=<game folder> build/editor/u8edit-tests

## Running

    u8edit [--data <game folder>] [--save <U8SAVE.nnn or a GAMEDAT folder>] [--map <n>]

Without `--data` it tries `U8_DATA`, then the folder it used last, then the folder the port
remembers (`data=` in the port's `u8port.cfg`), and then asks.

**Work on a copy of the game folder.** Saving changes the files in the folder that is open
(the first time each file is written, the original is kept beside it as `name.orig`).

## What it edits, and where it goes

| What | File | Notes |
|------|------|-------|
| Fixed items: land, walls, buildings, eggs | `STATIC/FIXED.DAT` | shared by every game |
| Movable items | `NONFIXED.DAT` in the saved game | |
| NPCs: where they stand, frame, flags | `ITEMCACH.DAT` in the saved game | what NPCs carry is not editable yet |
| NPC strength, dexterity, intelligence, hit points, mana, alignment, flags | `NPCDATA.DAT` in the saved game | |
| Weapons, armour, monsters | `U8.EXE` | tables in the program's data; see below |
| Global flags (the story so far) | `FLAG.DAT` in the saved game | written when a flag is changed; `U8SAVE.000` has none, so a new game starts with every flag 0 |

The saved game is `SAVEGAME/U8SAVE.000` unless another is opened: every new game starts from it.
File > Open the Game in Progress edits `GAMEDAT` (the game the DOS version continues) instead.

A new item goes in `FIXED.DAT` when its shape is fixed (`TYPEFLAG.DAT`), else in
`NONFIXED.DAT`; an item put in a container goes where the container is. Changing an item's shape
moves it between the files when that changes. Glob items (the floors and scenery a glob egg puts
down) cannot be edited one by one: change, move or delete the glob egg, or pick another glob.

### The map the starting save was made on

`U8SAVE.000` was saved with map 3 loaded, and a new game takes map 3 from the save's item cache,
not from `FIXED.DAT` and `NONFIXED.DAT`. The editor shows map 3's movable items from that cache.
When map 3 changes, saving loads it into the cache again from the files just written, as
`ItemCache::loadMapLevel` would, and marks no region as loaded, so the game brings in the regions
around the avatar itself (and expands their globs) when it starts. The same goes for whichever
map another saved game was made on.

### Weapons, armour and monsters

Their statistics are not in any data file: they are tables in U8.EXE's data segment
(`u8/wpncache.cpp`, `u8/armordat.cpp`, `u8/mondata.cpp`). The editor finds them by what editing
leaves alone (the weapon shapes, the monster shapes, the ends of the armour lists) and changes
only their bytes, so DOS U8.EXE plays with the new numbers. The port reads the same tables from
U8.EXE in the game folder when it starts (`game/port/exetables.cpp`) and says so on its console
when they differ from the original ones.

- Weapons: a hit does strength / 5 + base damage + 0 to the random bonus; dexterity bonuses for
  attack and defence, an armour class bonus, and damage type bits (normal, blade, blunt, fire,
  undead, magic, slayer, pierce, falling).
- Armour: the armour class each piece adds, by its frame (frames 0 and 1 use the first number).
- Monsters: what monster eggs and the like make: hit points, dexterity and damage between a
  minimum and a maximum, armour class, alignment, damage type and the damage types it defends
  against.

## Using it

- **Maps** (left): every map with items, with counts and the number of teleport eggs leading out.
  Double-click a name (or press F2) to name a map; names are kept in the editor's settings.
- **Map**: drawn in the game's own order (`DL_ItemNode::compare`). Drag to move the view, wheel
  to zoom, arrow keys to move, `+`/`-` to zoom, `1` for actual size, `0` for the whole map.
  Click an item to select it and see it in the Item window. Hover over an item for its name.
  - Drag the selected item to move it; Shift+drag moves it up and down.
  - Ctrl+arrows move it along the world's x and y, Page Up/Down raise and lower it (Shift: by 1).
  - Delete deletes it (and what it holds), Ctrl+D copies it, F flips it, `[` and `]` change its
    frame.
  - Snap (toolbar) sets the step for moves and placing, in world units.
  - The layer buttons show and hide fixed, glob, movable and NPC items, eggs (with where each
    hatches and what it does), and invisible items. "Roof below" hides what is above a height,
    as the game does inside buildings.
- **Shapes** (right): every shape, filtered by number, usecode name or family. Pick one and a
  frame, then Place (or double-click, or press P over the map) and click on the map; a new item
  stands on what is under the mouse. Right-click or Esc stops placing. I picks the selected item's
  shape.
- **Names**: an item's name is what the game shows when the Avatar looks at it: the editor runs
  the item's look routine from the usecode (in a small copy of the game's usecode interpreter)
  against the item's quality, frame and the saved game's flags, so a key says which door it
  opens and Devon is "man" until the flags say the Avatar knows him. The name heads the Item
  window and the map's tooltip. Items whose usecode has no look routine (most scenery) have
  none, and show their usecode class.
- **Item** window: the Edit tab changes the item: its position, shape, frame, quality, npcNum,
  mapNum and flags, and in plain terms what it does as an egg: a teleport egg's destination map,
  teleport id, fade and hatch range; a monster egg's monster, count, probability and activity; a
  usecode egg's type; a glob egg's glob. An NPC also shows its statistics. The Details tab shows
  everything about it, with links (double-click) to its container, glob egg, contents and the
  transitions it is part of. A sending teleport egg has "Go to destination".
- **Minimap**: the whole map with the view's area and the teleport eggs (orange sends, green
  receives). Click or drag to move.
- **Transitions**: the maps as a graph. Solid arrows are teleport eggs; dashed ones usecode that
  calls `teleportToEgg` with constant arguments; dotted ones (off by default) usecode that calls
  `Npc::teleport` with a fixed place, which also moves NPCs off stage. Hover an arrow for the
  ways it stands for; double-click to go to the first. Double-click a map to open it. Drag maps
  to tidy the picture; the places are kept.
- **Game Data**: the weapon, armour and monster tables and the NPCs' statistics, to type into.
- **Flags** (View > Flags, Ctrl+L): every global flag the usecode reads or writes (445 of
  them), with its value in the open saved game, the classes that read and write it, and a name
  and notes of your own (kept in the editor's settings, for you only; a few the editor knows
  are filled in, greyed). Type a value to change it; it is saved to the saved game's
  `FLAG.DAT`. Search by number, name, notes or class, or show only the flags that are not 0.
  Double-click a flag's number or its classes for every place the usecode uses it, with the
  routine and the strings around it, which usually say what the flag means.
- **File > Apply a Translation** makes a translated copy of the English game folder from a
  translation table: the dialogue, books, signs and item names, the letters the language needs
  added to the fonts, and (by choice) the English voices left out, so that their lines show as
  text. The Brazilian Portuguese table comes with the editor (`languages/` beside it); see
  `tools/lang/README.md`.
- **Edit > Undo/Redo** take back any change; **File > Save** (Ctrl+S) writes them.
- **File > Test in the Port** (F5) saves, then starts the port (`Ultima8` beside `u8edit` or one
  level up, or the one chosen with File > Choose the Port Program) on a home folder of its own
  holding only the edited saved game, so it begins a new game from what was saved.

## How the data is read

| File | What | Where the game reads it |
|------|------|-------------------------|
| `STATIC/FIXED.DAT` | fixed items, a flex record of 16-byte items a map | `ItemCache::loadMapLevel` |
| `NONFIXED.DAT` (in the save) | movable items, the same; map 0 holds the NPCs | the same |
| `ITEMCACH.DAT` (in the save) | the item slots, NPCs in slots 1-255, the loaded map | `ItemCache::save`, `load` |
| `NPCDATA.DAT` (in the save) | 256 records of 49 bytes | `Npc::load`, `NpcData` |
| `STATIC/GLOB.FLX` | groups of items that glob eggs stand for | `GlobExpander::expand` |
| `STATIC/U8SHAPES.FLX`, `U8PAL.PAL` | shapes and the palette | `u8/skipdraw.h`, `FilePalette::read` |
| `STATIC/TYPEFLAG.DAT` | 8 bytes of flags a shape | `item/type.h` |
| `USECODE/EUSECODE.FLX` | class names; teleport calls | `spawnUnk`, `u8/intrinsics.cpp` |
| `U8.EXE` | weapon, armour and monster tables | `u8/wpncache.cpp`, `u8/armordat.cpp`, `u8/mondata.cpp` |

A contained item follows its container in the file, its x the depth it is at. Teleport eggs
with frame 0 send the avatar to the egg with frame 1 and the same id (the quality's low byte)
on the map in their mapNum byte (`TeleporterProcess`). The usecode scan reads bytes, not decoded
code, and keeps only calls whose arguments are all constants. A flex file's checksum is the
signed sum of its bytes from the end of the header to 0x80 before the end of the file
(`FlexFile::getChecksum`); the editor keeps it right, though the game only shows it on its
diagnostics screen.

Writing a map back gives the same bytes it was read from (`u8edit-tests` checks every map), so
a save only changes what was changed.
