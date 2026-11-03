# Languages for Ultima VIII

A new language for the game is made from a **translation table**: every string of the
usecode (`USECODE/EUSECODE.FLX`: the dialogue, books, scrolls, signs and item names) with its
translation. Buttons, menus, the intro and the credits are pictures or text in the program and
stay as they are.

| File | What |
| --- | --- |
| `pt_BR.json` | the Brazilian Portuguese table: all 5,766 strings |
| `u8fonts-pt.json` | the font patch it names: the 116 letters (`ã`, `õ`, accented capitals, in 9 fonts) that the fonts lack, as frames to add to `STATIC/U8FONTS.FLX` |
| `u8lang.py`, `usecode.py`, `fonts.py` | the tool that extracts tables, makes font patches and builds languages |
| `TRANSLATING.md` | the rules and glossary the Portuguese followed |

## Playing the Portuguese version

Everyone makes it from their own copy of the game (GOG: the `ENGLISH` folder of the
installation). The English folder is not changed; the new one is a whole game folder, played
with the port (`Ultima8 --data <PORTUGUESE>`) or in DOSBox like the English one. There are two
ways, which give the same files, byte for byte.

**With the editor** (nothing else to install): File > Apply a Translation. It offers the
Portuguese table the editor comes with (`languages/` beside the program), the English folder
open in the editor, and a `PORTUGUESE` folder beside it, and leaves out the English voices
unless told not to. Apply makes the folder, and the editor can open it.

**With Python**, from this folder. The easiest way to run it is [uv](https://docs.astral.sh/uv/),
which fetches a Python of its own on first use, so no Python needs installing:

    # install uv (once)
    winget install --id=astral-sh.uv -e                      # Windows (or the PowerShell line below)
    powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"
    curl -LsSf https://astral.sh/uv/install.sh | sh          # Linux, macOS

    uv run u8lang.py build "C:/GOG Games/Ultima 8/ENGLISH" pt_BR.json "C:/GOG Games/Ultima 8/PORTUGUESE"

`--keep-voices` keeps the English voices. With a Python 3.9 or newer already installed,
`python3 u8lang.py ...` (`python` on Windows) does the same; the tool needs nothing beyond
Python's standard library.

## Making a translation

    uv run u8lang.py extract <ENGLISH> --compare <FRENCH> <GERMAN> -o table.json
    (translate: fill in "tr" for every string; add "language", "folder" and "font_patch")
    uv run u8lang.py fontpatch <ENGLISH> -o u8fonts-xx.json
    uv run u8lang.py build <ENGLISH> table.json <NEW FOLDER>

`extract` lists each distinct string once, and with `--compare` marks the ones that are the
same in every language Origin shipped (file names, debugging, words of power) to leave as they
are. `fontpatch` writes the letters `fonts.ENCODING` lists that the fonts lack (Portuguese's,
today; add a language's letters there and to `fonts.SMALL`/`fonts.CAPITAL`). A table names its
font patch, which goes beside it; put both in the editor's `tools/lang/` and list them in
`U8EDIT_LANGUAGES` (its `CMakeLists.txt`) to have the editor offer it.

## Sharing a translation

Share the table and its font patch (`pt_BR.json`, `u8fonts-pt.json`), and let each player
build the folder from their own game, with the editor or with `u8lang.py`, as above. The editor
comes with them. That is what this repository does, and it is
the usual way of sharing fan translations:

- it works from any copy of the English game, which needs nothing else;
- what is shared is the translation table (which quotes the English lines it translates),
  not Origin's files: `EUSECODE.FLX` is the whole game's code, and `U8FONTS.FLX` Origin's
  fonts;
- a corrected table makes a corrected game the same way.

Sharing only the dialogue file is not enough. A built language changes three things, and a
player given `EUSECODE.FLX` alone would get the third wrong and miss the second:

1. `USECODE/EUSECODE.FLX`, the text;
2. `STATIC/U8FONTS.FLX`, the fonts with the letters the translation adds (for Portuguese, `ã`,
   `õ` and the accented capitals); without it those letters show as gaps;
3. the recorded voices `SOUND/E<number>.FLX` taken out, so that the voiced characters' lines
   show as text. With them, those characters speak English and show no text.

To hand someone a ready-built version anyway, give them those two files and tell them to
start from a copy of their English folder, replace the two files and delete
`SOUND/E*.FLX` (not `MUSIC.FLX` or `SOUND.FLX`). Both files are built from Origin's, so this
shares the game's code and fonts.

## What the new folder is

A copy of the English game folder with three changes:

- `USECODE/EUSECODE.FLX` holds the translated strings. The code is the English code, byte for
  byte, with every branch, call and event offset moved to where its instruction now is
  (`usecode.py` says which instructions point into code). Putting Origin's own French and
  German strings into the English file this way gives Origin's FUSECODE and GUSECODE classes
  byte for byte, except the few classes whose code Origin changed for those languages.
- `STATIC/U8FONTS.FLX` has the letters the translation needs that the fonts lack (for
  Portuguese: a and o with tilde and the accented capitals), drawn from the font's own accents
  (`fonts.py`) and kept as the font patch; each frame is appended to its font and the font's
  frame table points at it.
- The recorded voices (`SOUND/E<npc>.FLX`) are left out, unless asked to keep them. The game plays a voice only when the
  text matches the recorded words, and shows no text while it does, so with them the nine
  voiced characters would speak English with no subtitles. This is how Origin's French and
  German versions work too.

The saved game still says the language is English (byte 54 of `AVATAR.DAT` in
`SAVEGAME/U8SAVE.000`). Giving a new language its own number would need art and text for every
button and menu (`u8/u8yesno.cpp`, `u8/contgump.cpp`, `u8/world.cpp` index tables by it), so
the new language takes the English one's place. The folder plays in DOS U8.EXE as in the port.

## Rules a translation has to keep

`TRANSLATING.md` is the guide the Portuguese was made with. What the tool checks:

- Spaces at the start and end of a string stay: strings are joined at run time
  (`"My name is " + name`).
- `~` (line break in books), `*` (line break on graves and plaques), `%` (indent) and tabs stay
  where they are.
- The same English is translated the same everywhere: usecode compares strings with strings.
- Only letters the fonts can draw (`fonts.ENCODING`).
- Graves and plaques (classes named GRAVE and PLAQUE) are drawn in carved capitals with no
  accents, so accents there are dropped. Their lines are not wrapped: keep them about as long as
  the English.
- A class must stay under 64K. The books (class 1427) are the largest: 52,925 bytes in English
  and 51,653 in Portuguese.
