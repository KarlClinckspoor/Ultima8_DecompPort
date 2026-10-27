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
