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
  editor/                 u8edit, a Qt 6 editor for the game's world and statistics (its own
                          build; see editor/README.md)
  game/                   the game: ../source converted to portable C++, plus ports of its .ASM
  game/port/              DOS and Borland library calls the game used, done with plat_*
  game/sound/xmi*.cpp     the XMIDI music driver that replaces AILXMI.DLL
  platform/plat.h         everything the game needs from the host
  platform/matoya/        the libmatoya backend (window, input, audio, files, timing)
  third_party/            libmatoya (MIT), Munt's mt32emu (LGPL 2.1+), Nuked OPL3 (LGPL 2.1+),
                          vendored at the same commits as the Ultima VII port
  tools/                  the source converter and checking scripts (see PORTING.md), and
                          u8rec.py, which reads recordings
  PORTING.md              how the sources were ported, and the traps that remain
```

## Requirements

| | Linux x64 | Windows x64 | macOS |
| --- | --- | --- | --- |
| Compiler | clang 14+ or GCC 11+ | clang-cl, from Visual Studio 2022 with "C++ Clang tools for Windows" | Xcode command line tools (clang) |
| Build tools | CMake 3.20+, make | CMake 3.20+, Ninja and nmake (both come with Visual Studio) | CMake 3.20+, make |
| Extra | `glslangValidator` (Ubuntu/Debian: `glslang-tools`), for libmatoya's shaders | | |

On Linux, libmatoya opens X11, OpenGL/Vulkan and ALSA at run time, so a desktop session with
those libraries is needed to play.

What has been tested: the Linux build (clang Debug and Release, GCC Release), running headless
with scripted input (see "Scripted runs") on the English GOG data: the intro, naming the
Avatar, conversations, walking into Tenebrae and the scene on the docks, the paperdoll and
backpack, combat mode, dying, saving, loading in the same session, and resuming in a new
session; the data folder named with `--data` or `U8_DATA` from another folder, and read-only
data files; recording sessions and replaying them, on the build that made them and on the
others (clang Debug and Release, GCC), with a save and a load inside; the settings screen's
keys and settings file, in a window on Xvfb, and its drawing at several window sizes (Xvfb's GL
showed only a black window, so not on screen). The Windows and macOS builds have not been tried: the game
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

A Windows build can also be made on Linux with mingw-w64 (the `mingw-w64` package):

```sh
cmake -S ultima8-portable -B build-windows -DCMAKE_TOOLCHAIN_FILE=ultima8-portable/cmake/mingw-w64.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --target u8
```

That `Ultima8.exe` is one file with nothing else to install, but it draws with Vulkan only:
libmatoya's Direct3D renderers need shaders compiled by Microsoft's `fxc`, so they are left out
(`cmake/mingw/`). Current Windows graphics drivers include Vulkan; the clang-cl build above has
Direct3D 11 too.

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
`ENGLISH` for the English game). File names are matched in any letter case. The folder is only
read: the game's progress and saved games go in the port's home folder (below).

**MT-32 ROMs** (optional). Munt needs the Roland MT-32 control and PCM ROM images (any file
names; they are recognised by content). They are not included. The game looks in the
`U8_MT32_ROMS` folder, else the `mt32-roms=` folder in the settings file, else the
`U8_MT32_ROM_DIR` build option, else the game data folder.
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

- `--data` is the game data folder (or set `U8_DATA`), and is remembered for later starts. By
  default it is the one remembered, else the current folder if that has `STATIC`, else the
  folder the program is in, else `Documents/Ultima8`, so a double-clicked `Ultima8` placed in
  the game folder finds its data. The program checks for the core files at launch and names
  any that are missing.
- `--adlib` plays the music on the AdLib even when the MT-32 ROMs are found; `--roland` asks
  for the MT-32 (with no ROMs it falls back to the AdLib).
- `--home <dir>` (or `U8_HOME`) moves the home folder; `--saves-in-data` keeps the game's
  `GAMEDAT` and `SAVEGAME` in the game folder, as DOS did.
- `--scale <n>`, `--no-aspect` and `--no-integer` set the window for one run (see "Settings").
- `--help` lists the port's switches. Other arguments go to the game, as U8.EXE took them:
  `-english`, `-french`, `-german` (the language, when that language's data is installed),
  `-u <folder>` (usecode folder), `-cache <MB>` (shape cache size), `-test`, `-record`,
  `-playback`.
- On Windows the program is a windowed one, so no console opens with it. Started from a
  terminal, it prints its messages there.
- Alt-Enter toggles fullscreen. F11 (Cmd+, on a Mac) opens the settings screen.

In the game, the mouse and keys are the DOS game's: the left button selects and uses (double
click), holding the right button walks towards the pointer, Esc brings up the main menu (read
or write the diary to load or save), C toggles combat, Z opens the paperdoll and I the
backpack.

## The home folder

The port keeps its own files in `~/.u8port` (on Windows `%APPDATA%\u8port`; `--home` or
`U8_HOME` puts it elsewhere):

| | |
| --- | --- |
| `u8port.cfg` | the settings, as `key=value` lines |
| `GAMEDAT/`, `SAVEGAME/` | the game in progress and the saved games. The first start copies both from the game folder (the shipped `U8SAVE.000` is the start of a new game); after that the game folder is not written to. To start afresh, rename the folder rather than delete it. |
| `recordings/` | the last five sessions (see "Recording and replaying") |

## Settings

The settings screen opens over the game with F11 (Cmd+, on macOS, which takes F11 for itself)
and closes with F11 or Esc. It also opens before the game on every start until its "Show this
at start" row is turned off; the game waits for it. While it is open the game's clock stands
still. Up and Down move between the rows and the tab bar, Left and Right or Enter change a
value, Tab goes to the next tab; the mouse can click tabs and rows and drag the volume.

Every change is written to `u8port.cfg` at once. Rows marked "next start" are used from the
next start; the others at once.

| Tab | Row | Settings-file key | When | Command line (this run only) |
| --- | --- | --- | --- | --- |
| Sound | Music: Automatic (the MT-32 when its ROMs are found), Roland MT-32, AdLib | `music` (`auto`, `roland`, `adlib`) | next start | `--roland`, `--adlib` |
| Sound | MT-32 ROMs: found or not, and the folder (shown only) | `mt32-roms` | next start | `U8_MT32_ROMS` |
| Sound | Volume, 0 to 100 | `volume` | at once | |
| Display | Fullscreen (Alt-Enter too) | `fullscreen` | at once | |
| Display | Window scale, 1x to 8x the 320x200 screen, as far as fits the display | `scale` | at once | `--scale n` |
| Display | 4:3 aspect: the 200 lines shown as 240, as on a CRT; off for square pixels | `aspect` | at once | `--no-aspect` |
| Display | Whole-number scaling; off fills the window | `integer` | at once | `--no-integer` |
| Game | Game folder (shown only) | `data` | next start | `--data` (remembered) |
| Game | Home folder (shown only) | | | `--home` |
| Game | Record sessions | `recording` | next start | `--no-recording` |
| Game | Show this at start | `settings-at-start` | next start | |

A value the file holds that the screen cannot show is read as that row's default.

## Recording and replaying

Every session's input is recorded, so a session can be played again exactly: the same game
in the same state at every moment. That makes a crash something that can be replayed, and a
long session a test: record a playthrough once, and after changing the code replay it to see
that the game still does exactly what it did (and so is still winnable).

**The black box.** Each session goes to `recordings/<date>-<time>/` in the home folder; the
newest five are kept. Turn it off on the settings screen ("Record sessions") or for one run
with `--no-recording`. Scripted runs (`U8_TEST_INPUT`) and replays are not recorded unless
asked with `--record`. A session folder holds `SESSION.U8R`, the recording, and `stage/`, the
`GAMEDAT` and `SAVEGAME` folders as they were when it began (so the black box takes the size of
your saved games, five times over). An hour of play records to about 10 MB.

**Replaying.**

```sh
build/Ultima8 --replay ~/.u8port/recordings/20261025-142233
```

replays with no window and no sound, as fast as the game runs (10 to 20 times faster than
it was played here), on a copy of the session's `stage/`, so your own saves are untouched. It prints
how it ended, and the exit code says it:

| Exit code | |
| --- | --- |
| 0 | every checkpoint matched to the end of the recording (or to where the recorded session crashed or was killed, which the replay reaches too) |
| 2 | the game diverged: the message says at what game time, and how |
| 1 | the replay could not run, or was stopped |

Once a second of game time the recording holds a checkpoint: a digest of the game's state
(the random seeds, every item's place and state, the global flags, the timer). The replay
checks each one, and the first that differs stops it with exit code 2, as does any point
where the game asks for input or a value where the recording has something else.

`--replay-view` replays in a window, at the speed it was played, with sound. `--record <dir>`
records a session to a folder of your choosing (for a scripted run, too). `--shots <dir>`
saves the game's screen every `--shot-every <n>` game seconds (default 60), named by the
game second, in recordings and replays alike, so the two can be compared picture by picture.

A replay needs the same game data (the recording notes a fingerprint of `STATIC` and
`USECODE`, and warns when it differs) and plays its music on the device it was recorded with.
It does not depend on the machine, the build type or the compiler: a session recorded with a
Debug build replays on a Release or GCC build. `tools/u8rec.py info <session>` summarises a
recording and `tools/u8rec.py dump <session>` lists it. How it works, and what the port has to
keep doing for it to work, is in PORTING.md ("Keeping the game deterministic").

## Scripted runs

For testing, `U8_TEST_INPUT` drives the game from a script and opens no window. Scripted runs
keep the game's files in the game folder, so use a copy of it:

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
  kernel programmed (`platform/matoya/events.c`). The game's clock moves in 1 ms steps, and
  only then.
- The XMIDI music driver is new code following the Miles AIL 2.0 drivers' behaviour (timbre
  library, percussion bank, velocity scaling, pitch bend range) rather than a translation of
  them; the MT-32 path has not been heard on real ROMs.
- The weapon, armour and monster statistics are compiled into the game, as they were into
  U8.EXE; at start the port takes them from U8.EXE in the game folder when it has them
  (`game/port/exetables.cpp`), so a U8.EXE changed by the editor plays the same here as in DOS.
  The console says so when they differ from the original numbers.
- Saved games are not interchangeable with the DOS game's. They keep the DOS layout except
  for the kernel's process records (`GAMEDAT\KERNEL.DAT`), which hold each process's class
  name and its object as the host lays it out. A new game starts from the shipped
  `U8SAVE.000`, which has no kernel record, so it is unaffected.
