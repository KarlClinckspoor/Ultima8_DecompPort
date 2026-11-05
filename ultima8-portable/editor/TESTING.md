# Testing the editor: the plan

What `u8edit-tests` covers today, what it misses, and the suite it should grow into. The game's
data cannot be shipped, so most tests run against a copy of the player's game folder (`U8_DATA`)
and are skipped without one; the plan adds tests that need no game data at all, so that some
checking happens everywhere.

## What there is

`tests/tests.cpp`, one Qt Test program, every test on a fresh copy of the data in a temporary
folder:

| Test | What it checks |
| --- | --- |
| `roundTrip` | every map's FIXED.DAT and NONFIXED.DAT records written back byte for byte |
| `translation` | File > Apply a Translation with the Portuguese table: strings, fonts, voices |
| `editInWindow` | the main window driven with the mouse: select, drag, undo, place, delete, a weapon, save |
| `itemNames` | look routines: a key by its quality, Devon by the flags, full names |
| `flags` | FLAG.DAT written and read back |
| `groupEdits` | moving, flipping, copying and deleting several items, each one step to undo |
| `doorsAndKeys` | KEYRING's rule over the whole game; `key:` and `is:door` searches |
| `picking` | the right-click list's order; X-ray passing over faded items |

Gaps: no test runs without the game data; the drawing is never compared with what it should look
like; the right-click menu, the rubber band, the keys, the Item window's fields and Nudge, the
Search dock and the manual are not driven; nothing checks that what the editor saves plays in the
game; nothing measures speed; there is no continuous integration for the editor.

## The suite

### 1. Without game data (runs everywhere, every build)

Small synthetic files made by the tests themselves:

- **Flex files**: records in, the same records out; the checksum (`FlexFile::getChecksum`) right
  after every change; record 0 and 1 kept where EUSECODE.FLX keeps them.
- **TYPEFLAG.DAT decoding**: one record per bit, each landing in its field (Borland bit-field
  order); the footpad rules (`getWorldSize`: a loose shape is never flat).
- **Item records**: the 16-byte layout, contained items after their container, glob items never
  written.
- **Screen projection**: `toScreen` and `toWorld` undo each other; `DL_ItemNode::compare` on hand-made
  pairs (side by side, stacked, the tie-breakers).
- **The search language**: every term, quoting, hexadecimal numbers, unknown terms refused.
- **Names**: the names file written and read back; bad lines refused with their line number.
- **The usecode machine**: hand-assembled routines for each opcode the look events use; flags read
  with every width.

### 2. With the game data (as today)

- **Every map, every file**: load, save untouched, compare bytes (FIXED.DAT, NONFIXED.DAT,
  ITEMCACH.DAT, NPCDATA.DAT, FLAG.DAT, U8.EXE's tables), for the starting save and a game in progress.
- **Random edits and undo**: a seeded sequence of a few hundred edits (move, add, delete, copy,
  shape changes that cross FIXED/NONFIXED, flips, NPC statistics, flags, tables), then undo all:
  the files saved must equal the originals. Redo all and save: loading gives the same world.
- **Names over the whole game**: every item's look name computed without the run limit tripping;
  a list of known names (keys, the people of Tenebrae) checked in full and in save mode.
- **Transitions**: the count of teleport eggs, usecode teleports and missing destinations, against
  numbers recorded once (they change only when the data does).
- **Keys and doors**: every key with a door, every locked door with a key, written to a report;
  the numbers recorded.

### 3. Drawing (golden images)

- Render fixed views (a corner of map 3 at zoom 1, the whole of a small map, heights shown, X-ray at
  a point, each layer at half opacity) to images and compare them with images recorded once, by a
  hash and, when that differs, by the share of pixels that differ (fonts and smoothing vary between
  platforms; overlays are compared without labels).
- The overview image and the minimap the same way.

### 4. The interface (QTest, offscreen)

- **Map**: right-click a pile and choose the third entry; Ctrl+click two items and drag them;
  Ctrl+drag a box; Ctrl+A; Alt+arrows and Page Up/Down with Shift and Alt; Delete, Ctrl+D, F, `[`
  `]` on a selection; Esc. Each checks the world and the undo stack.
- **Item window**: every field of every egg kind, a door and a key, typed into and undone; Nudge
  with each step; Go to destination and Go to sender (one and several senders); Find key N.
- **Search dock**: this map and every map, inside containers or not, sorting by map, double-click
  showing the item.
- **Names**: naming a map and a shape, Export and Import Names, Full Names on and off.
- **Manual**: it opens from Help, every link inside it leads somewhere, every brown passage has a
  tooltip.
- **Settings**: the window's layout, snap, nudge step and X-ray settings survive a restart.

### 5. Against the game

- **In the port**: save an edit (an item placed, a door unlocked, a monster egg changed, a teleport
  egg moved), start the port headless on it with scripted input (it already takes scripted input
  and writes a state digest), and check the item is where it was put, the door opens, the Avatar
  arrives at the moved egg.
- **In DOS**: the same saves started in DOSBox-X with the original U8.EXE, for the files the
  original reads (a smoke test that it loads and the first frames match the port's).

### 6. Speed

- Opening the largest map, a whole-game search for a word and for `key:N`, a right-click on the
  densest pile, and a full save, each timed with `QBENCHMARK`; a failure when one is more than
  twice its recorded time.

### 7. Where they run

| Where | What |
| --- | --- |
| every push (CI) | part 1 on Windows (MSYS2 UCRT64), Ubuntu 22.04 (Qt 6.2, the oldest supported) and Ubuntu 24.04; a build with `-Werror` |
| before a release, by hand | parts 2 to 6 with the game data, on Windows and Linux |
| with a sanitizer build | parts 1, 2 and 4 under AddressSanitizer and UndefinedBehaviorSanitizer on Linux |

The game data stays on the machine of whoever runs the tests; CI never sees it.

## Order of work

1. Split `tests.cpp` into a data-free program and the one that needs the game, with a CMake test
   for each (`ctest`), and the CI workflow for the data-free one.
2. Random edits and undo (part 2): it finds the worst bugs for the least code.
3. The interface tests for the new map tools and the Item window (part 4).
4. Golden images (part 3).
5. The port round trip (part 5) and the benchmarks (part 6).
