# Ultima 8 Decompile

This is a decompiled source of the Ultima 8 latest retail version, including the english usecode. This build contained some debug symbols which allowed most of the file and function names to be recovered as well. The compiler was Borland C++ 3.0 with a mix of flags. In this decompile the helper libraries (dos extender, etc) were left as binary because they aren't very high value and mostly just assembly anyway. It rebuilds byte-exact minus some debug data at the end of the file.

## Portable port

`ultima8-portable/` is the same source made to build as a modern 64-bit program for Windows,
macOS and Linux, after the Ultima VII portable port: given the game data, it plays Ultima
VIII in a window with emulated MT-32 or AdLib music and Sound Blaster speech and effects.
See [ultima8-portable/README.md](ultima8-portable/README.md) for building and running it, and
[ultima8-portable/PORTING.md](ultima8-portable/PORTING.md) for how it was ported.
