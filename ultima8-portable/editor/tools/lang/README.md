# Languages for Ultima VIII

`u8lang.py` makes a new language for the game out of a table of translated strings: the
dialogue, books, scrolls, signs and item names, which all live in the usecode
(`USECODE/EUSECODE.FLX`). Buttons, menus, the intro and the credits are pictures or text in the
program and stay as they are.

    python3 u8lang.py extract <ENGLISH> --compare <FRENCH> <GERMAN> -o table.json
    (translate: fill in "tr" for every string)
    python3 u8lang.py build <ENGLISH> table.json <NEW FOLDER>

`pt_BR.json` is Brazilian Portuguese, all 5,766 strings:

    python3 u8lang.py build <ENGLISH> pt_BR.json <PORTUGUESE>

## Playing the Portuguese version

Everyone builds it from their own copy of the game: the repository holds the tool and the
translation, not the game's files. It needs Python 3 (Windows: from python.org, or in MSYS2
`pacman -S mingw-w64-ucrt-x86_64-python`) and the English game folder (GOG: the `ENGLISH`
folder of the installation). From this folder:

    python3 u8lang.py build "C:/GOG Games/Ultima 8/ENGLISH" pt_BR.json "C:/GOG Games/Ultima 8/PORTUGUESE"

(`python` instead of `python3` on Windows.) The new folder is a whole game folder. Play it with
the port (`Ultima8 --data <PORTUGUESE>`), or in DOSBox the way the English folder is played.
The English folder is not changed.

## Sharing a translation

Share the table (`pt_BR.json`) with `u8lang.py`, `usecode.py` and `fonts.py`, and let each
player build it from their own game, as above. That is what this repository does, and it is
the usual way of sharing fan translations:

- it works from any copy of the English game, which needs nothing else;
- what is shared is the translation table (which quotes the English lines it translates),
  not Origin's files: `EUSECODE.FLX` is the whole game's code, and `U8FONTS.FLX` Origin's
  fonts;
- a corrected table makes a corrected game with the same command.

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
  (`fonts.py`).
- The recorded voices (`SOUND/E<npc>.FLX`) are left out. The game plays a voice only when the
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
