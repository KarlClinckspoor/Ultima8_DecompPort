U8 Editor (u8edit)
==================

An editor for Ultima VIII's world and game data: the maps, the items and eggs on them, the
NPCs and their statistics, and the weapon, armour and monster tables. What it saves plays in
the original DOS game as well as in the portable version. It needs your own copy of the game.

Work on a copy of the game folder. Saving changes the files in the folder that is open (the
first time each file is written, the original is kept beside it as name.orig).

Starting
--------

    Windows:  u8edit.exe --data "C:\path\to\a copy of Ultima 8\ENGLISH"
    Linux:    ./u8edit --data "/path/to/a copy of Ultima 8/ENGLISH"

Without --data it asks for the folder, and remembers it.

File > Test in the Port (F5) saves, then starts the portable version on what was saved. It
looks for Ultima8 beside u8edit or one folder up; File > Choose the Port Program points it
anywhere else.

File > Apply a Translation makes a translated copy of the English game folder. The Brazilian
Portuguese translation is in languages/; the English folder is not changed.

The folder holds the Qt libraries the editor needs: on Windows, Qt's DLLs beside u8edit.exe;
on Linux, lib/ and plugins/, which the ./u8edit script uses. Linux needs glibc 2.38 or newer
(Ubuntu 24.04, Debian 13, Fedora 39 or later), X11 or Wayland.

More: https://github.com/KarlClinckspoor/Ultima8_DecompPort (ultima8-portable/editor/README.md).
