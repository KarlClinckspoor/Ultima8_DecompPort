# U8 Editor (u8edit)

A viewer for Ultima VIII's world, and the start of an editor for it. It reads the game's own
files as the game reads them (the decompiled sources in `game/item` and `game/u8` are the
reference for every format) and changes nothing yet.

## Building

It needs Qt 6.2 or newer (Widgets only) and CMake. It is a project of its own, apart from the
game:

    cmake -S editor -B build/editor -DCMAKE_BUILD_TYPE=Release
    cmake --build build/editor

On Windows, point CMake at Qt with `-DCMAKE_PREFIX_PATH=C:\Qt\6.x\msvc2022_64` and run
`windeployqt` on the result to ship it.

## Running

    u8edit [--data <game folder>] [--save <U8SAVE.nnn or a GAMEDAT folder>] [--map <n>]

Without `--data` it tries `U8_DATA`, then the folder it used last, then the folder the port
remembers (`data=` in the port's `u8port.cfg`), and then asks. Movable items come from the
saved game given, else `GAMEDAT` when it holds a game, else `SAVEGAME/U8SAVE.000` (the game's
starting state). File > Open Saved Game opens the port's saves (`~/.u8port/SAVEGAME`).

## What it shows

- **Maps**: every map with items, with counts, and the number of teleport eggs leading out.
  Double-click a name (or press F2) to name a map; names are kept in the editor's settings and
  used everywhere.
- **Map**: the map drawn in the game's order (`DL_ItemNode::compare`). Drag to move, wheel to
  zoom, arrow keys to move, `+`/`-` to zoom, `1` for actual size, `0` for the whole map.
  Click an item for its details. The toolbar turns on and off the fixed items, the glob items,
  the movable items, NPCs, eggs (with where each one hatches and what it does), and items with
  the invisible flag. "Roof below" hides what is at or above a height, as the game does inside
  buildings.
- **Item details**: the item's record, its shape's type flags, its usecode class, what it does
  as an egg (teleport destination, monster, glob, usecode class), what it holds, and the
  transitions it is part of. Linked entries (blue) can be double-clicked to go there; a sending
  teleport egg has a "Go to destination" button.
- **Minimap**: the whole map with the view's area and the teleport eggs (orange sends, green
  receives). Click or drag to move.
- **Transitions**: the maps as a graph. Solid arrows are teleport eggs; dashed ones usecode that
  calls `teleportToEgg` with constant arguments; dotted ones (off by default) usecode that calls
  `Npc::teleport` with a fixed place, which also moves NPCs off stage. Hover an arrow for the
  list of ways it stands for; double-click to go to the first. Double-click a map to open it.
  Drag maps to tidy the picture; the places are kept.

## How the data is read

| File | What | Where the game reads it |
|------|------|-------------------------|
| `STATIC/FIXED.DAT` | fixed items, a flex record of 16-byte items a map | `ItemCache::loadMapLevel` |
| `NONFIXED.DAT` (in the save) | movable items, the same; map 0 holds the NPCs | the same |
| `ITEMCACH.DAT` (in the save) | the item slots, NPCs in slots 1-255 | `ItemCache::save` |
| `STATIC/GLOB.FLX` | groups of items that glob eggs stand for | `GlobExpander::expand` |
| `STATIC/U8SHAPES.FLX`, `U8PAL.PAL` | shapes and the palette | `u8/skipdraw.h`, `FilePalette::read` |
| `STATIC/TYPEFLAG.DAT` | 8 bytes of flags a shape | `item/type.h` |
| `USECODE/EUSECODE.FLX` | class names; teleport calls | `spawnUnk`, `u8/intrinsics.cpp` |

A contained item follows its container in the file, its x the depth it is at. Teleport eggs
with frame 0 send the avatar to the egg with frame 1 and the same id (the quality's low byte)
on the map in their mapNum byte (`TeleporterProcess`). The usecode scan reads bytes, not decoded
code, and keeps only calls whose arguments are all constants.

A mid-game save keeps the map the avatar was on in the item cache rather than in
`NONFIXED.DAT`; that map's movable items show as `NONFIXED.DAT` last had them (the status bar
says which map).
