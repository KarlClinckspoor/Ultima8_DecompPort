# Ultima VIII: Pagan, portable

A port of the reconstructed Ultima VIII source in `../source` (16-bit Borland C++ for a Phar
Lap 286 DOS extender) to modern systems, built the same way as the Ultima VII portable port.
The game code talks to the machine only through a small platform layer
(`platform/plat.h`). The backend uses [libmatoya](https://github.com/snowcone-ltd/libmatoya)
for the window, input, video, audio and files, [Munt](https://github.com/munt/munt) to emulate
the Roland MT-32 and [Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3) to emulate the AdLib.
U8's Miles XMIDI drivers (the installer's `AILXMI.DLL`) are replaced by a sequencer written for
the port (`game/sound/xmidi.cpp`) in front of an MT-32 and an AdLib synth driver.

Everything needed to build is in this folder. You supply the game data and, optionally, the
MT-32 ROMs.

## Layout

```text
ultima8-portable/
  CMakeLists.txt          the build
  cmake/Libmatoya.cmake   builds libmatoya with its own makefiles
  game/                   the game: ../source converted to portable C++, plus ports of its .ASM
  game/port/              DOS and Borland library calls the game used, done with plat_*
  game/sound/xmi*.cpp     the XMIDI music driver that replaces AILXMI.DLL
  platform/plat.h         everything the game needs from the host
  platform/matoya/        the libmatoya backend (window, input, audio, files, timing)
  third_party/            libmatoya (MIT), Munt's mt32emu (LGPL 2.1+), Nuked OPL3 (LGPL 2.1+),
                          vendored at the same commits as the Ultima VII port
  tools/                  the source converter and checking scripts (see PORTING.md)
  PORTING.md              how the sources were ported, and the traps that remain
```

## Requirements

| | Linux x64 | Windows x64 | macOS |
| --- | --- | --- | --- |
| Compiler | clang 14+ (GCC 11+ should work) | clang-cl, from Visual Studio 2022 with "C++ Clang tools for Windows" | Xcode command line tools (clang) |
| Build tools | CMake 3.20+, make | CMake 3.20+, Ninja and nmake (both come with Visual Studio) | CMake 3.20+, make |
| Extra | `glslangValidator` (Ubuntu/Debian: `glslang-tools`), for libmatoya's shaders | | |

On Linux, libmatoya opens X11, OpenGL/Vulkan and ALSA at run time, so a desktop session with
those libraries is needed to play.

What has been tested: the Linux build, running headless with scripted input (see "Scripted
runs") on the English GOG data: the intro, naming the Avatar, conversations, walking into
Tenebrae, the paperdoll and backpack, combat mode, dying, saving, loading in the same session,
and resuming in a new session. The Windows and macOS builds have not been tried: the game
code compiles for a Windows target with clang and MinGW's headers, and the build follows the
Ultima VII port's, which builds on both, but a clang-cl build has not been run. Please report
what fails.

## Build

```sh
cmake -S ultima8-portable -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target u8
```

On Windows, build from an "x64 Native Tools Command Prompt for VS 2022", with clang-cl:

```bat
cmake -S ultima8-portable -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build --target u8
```

libmatoya's makefile relies on `cmd`'s `mkdir`. If Git's `usr\bin` is on `PATH`, nmake runs
Git's `mkdir` instead and the libmatoya step fails, so build from a prompt without it.

The first build also builds libmatoya (through its own makefile, into
`third_party/libmatoya/bin/`) and mt32emu (as a CMake subproject). The program is
`build/Ultima8` (`build\Ultima8.exe` on Windows), named so it can sit in the game folder beside
the DOS `U8.EXE`.

Options:

- `-DU8_MT32_ROM_DIR=/path/to/roms` bakes in a default ROM folder (see below).
- `-DCMAKE_BUILD_TYPE=Debug` for debugging.

## What you need to supply

**Game data.** A folder holding an installed copy of Ultima VIII, with its `STATIC`,
`USECODE`, `SOUND` and `SAVEGAME` folders (the GOG release has these in its language folder,
`ENGLISH` for the English game). File names are matched in any letter case. The game writes
to this folder (`GAMEDAT`, `SAVEGAME`), so use a copy.

**MT-32 ROMs** (optional). Munt needs the Roland MT-32 control and PCM ROM images (any file
names; they are recognised by content). They are not included. The game looks in the
`U8_MT32_ROMS` folder, else the `U8_MT32_ROM_DIR` build option, else the game data folder.
Without them, music plays on the emulated AdLib.

**Sound setup.** `U8.INI` is the setup the DOS installer wrote. The port reads it if it is
there, but it always has a Sound Blaster for speech and effects and plays music on the
MT-32 when the ROMs are found and on the AdLib otherwise, whatever the file says
(`game/sound/dsfxman.cpp`). The file is never changed. The game's options menu still turns
music and sound effects off.

## Run

```sh
build/Ultima8 --data /path/to/ultima8/ENGLISH
```

- `--data` is the game data folder (or set `U8_DATA`). By default it is the current folder if
  that has `STATIC`, else the folder the program is in, else `Documents/Ultima8`, so a
  double-clicked `Ultima8` placed in the game folder finds its data. The program checks for
  the core files at launch and names any that are missing.
- `--adlib` plays the music on the AdLib even when the MT-32 ROMs are found; `--roland` asks
  for the MT-32 (with no ROMs it falls back to the AdLib).
- `--help` lists the port's switches. Other arguments go to the game, as U8.EXE took them:
  `-english`, `-french`, `-german` (the language, when that language's data is installed),
  `-u <folder>` (usecode folder), `-cache <MB>` (shape cache size), `-test`, `-record`,
  `-playback`.
- On Windows the program is a windowed one, so no console opens with it. Started from a
  terminal, it prints its messages there.
- Alt-Enter toggles fullscreen.

In the game, the mouse and keys are the DOS game's: the left button selects and uses (double
click), holding the right button walks towards the pointer, Esc brings up the main menu (read
or write the diary to load or save), C toggles combat, Z opens the paperdoll and I the
backpack.

## Scripted runs

For testing, `U8_TEST_INPUT` drives the game from a script and opens no window:

```sh
U8_DATA=/path/to/copy U8_TEST_INPUT="12000 key esc; 16000 key k; 17000 key enter; \
    25000 shot first.ppm; 26000 move 300 60; 26100 rdown; 28000 rup; 30000 quit" build/Ultima8
```

Each step is a time in milliseconds and an action: `key` (a character, or esc, enter, space,
the arrows, f1-f12), `move x y` in the game's 640x200 mouse space, `down`/`up` and
`rdown`/`rup` for the buttons, `shot file.ppm`, `end n`, `quit`. See
`platform/matoya/testinput.c`.

Other variables for checking the port:

| Variable | Effect |
| --- | --- |
| `U8_AUDIO_DUMP=file` | writes everything the audio thread renders, raw 16-bit stereo at 48 kHz |
| `U8_PCM_DUMP=file` | writes the game's digitised sound as it queued it, raw unsigned 8-bit at 11025 Hz |
| `U8_UC_TRACE=1` | logs every usecode instruction (unit, offset, opcode, stack) and frees of things that are not YAMM handles |

## Differences from DOS

- The kernel's real-time processes ran from the PC's timer interrupt; here they run on the
  game thread whenever it waits or finishes a pass of the process list, at the rate the
  kernel programmed (`platform/matoya/events.c`).
- The XMIDI music driver is new code following the Miles AIL 2.0 drivers' behaviour (timbre
  library, percussion bank, velocity scaling, pitch bend range) rather than a translation of
  them; the MT-32 path has not been heard on real ROMs.
- Saved games are not interchangeable with the DOS game's. They keep the DOS layout except
  for the kernel's process records (`GAMEDAT\KERNEL.DAT`), which hold each process's class
  name and its object as the host lays it out. A new game starts from the shipped
  `U8SAVE.000`, which has no kernel record, so it is unaffected.
