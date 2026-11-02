# Lessons from the portable version

How the port was made, what went wrong on the way, and what to do differently next time.
[PORTING.md](PORTING.md) is the reference for the code: what each Borland construct became and
the traps that remain. This writeup is the story behind it. The lessons about the decompiled
source itself are in [../DECOMPILATION-LESSONS.md](../DECOMPILATION-LESSONS.md).

## How it went

| Step | Commit | What it took |
| --- | --- | --- |
| Mechanical conversion | 18ca0ec | `tools/convert_source.py`: names, fixed-width types, `far`/`near`/`pascal` dropped, includes flattened; the libraries vendored at the Ultima VII port's commits |
| Hand port of everything the converter could not do | 6d37f3f, 7076b08 | the 50 assembly modules, the kernel, files, the usecode bridge, input, the shape drawers, sound, and an XMIDI sequencer in place of the Miles driver, until the whole tree compiled |
| First light | 22b47a0 | CMake, the libmatoya backend; links and plays the intro |
| Runtime bugs | 9619c12 → f931fe9 | the traps in PORTING.md, found one at a time by playing scripted sessions |
| Linking at every optimisation level | e551962 | per-file inline functions, out-of-line virtuals, `-fwrapv -fno-strict-aliasing` |
| Review and documentation | 1e767b6 → 85d192d | README, PORTING.md, a review pass, Borland's `rand()` |
| Recording and replay, settings, home folder | cd8cae4 | a deterministic clock and a state digest |
| Tables from U8.EXE | d3fce41 | the port reads the statistics the editor changes |
| Windows build | cc8ea49 | cross-compiled with mingw-w64; frames identical to Linux under Wine |

About 65,000 lines of C++ in `game/`, from 180 C files, 2 C++ files and 50 assembly modules.

## Lessons

### Copy an architecture that already worked

The Ultima VII port had already settled the hard choices: one header (`platform/plat.h`)
between the game and the machine, libmatoya for the window, input, video and audio, Munt and
Nuked OPL3 for the music, the same CMake layout and the same vendored commits. Starting from
that removed a whole class of decisions and kept the two ports alike, so a fix in one
suggests a fix in the other. Next time, start with a port that already works in the same
family.

### Convert mechanically first, and keep the converter

One script did the bulk of the dull work in one pass, and it stays in `tools/` to document
how the files were made, though it is not meant to run again. After it everything was by
hand. The order that worked: make the *whole tree* compile before running anything, then
link, then run. Compiler errors in a 65,000-line tree are cheap to find. A half-ported tree
that links by accident is not.

### Translate what the assembly did, not the hardware it touched

The assembly modules fell into two groups. Those that compute (the shape drawers, the
decompressors, `RANDINT`) were translated line for line into C++, with their clipping, their
return values and their 16-bit arithmetic, and their header comments kept. Those that drove
the extender, the interrupt controller or the sound card were *replaced* behind the same
interface (an emulated interrupt mask, a mouse hook, `plat_pcm_queue`). Mixing the two
approaches in one module is where the effort goes wrong: decide which kind a module is
before starting it.

### The bugs that matter compile cleanly

Every serious bug after first light built without a warning and failed at run time: a `void`
intrinsic whose return value usecode waited on, a string literal written to, a usecode far
pointer whose low word had to be a YAMM handle, a saved block of the data segment that also
held a list's count, globals that had to be repointed to processes rebuilt from a save, and
`rand()` reaching 2^31 and running off a sound table. PORTING.md lists them by kind.
What found them:

- **A usecode trace.** Most of the game is usecode, so most wrong behaviour shows up as an
  intrinsic returning the wrong thing or a process waiting forever. A trace of intrinsic
  calls and their results points straight at both.
- **Scripted sessions with screenshots.** `U8_TEST_INPUT` plays keys and mouse moves at set
  times and saves frames (`shot`). A new game, a conversation, the inventory, combat, death,
  saving and loading were each a script whose frames could be looked at and compared between
  runs and builds.
- **Tools for each kind of bug.** Once 16-bit arithmetic bugs had turned up more than once,
  `tools/check_int16.py` listed every expression whose meaning changes with a 32-bit `int`,
  and the rest were fixed from the list rather than found by playing.

### Build every configuration, every time

The per-file inline problem only showed up at some optimisation levels: a Debug build linked
and a Release build did not, and clang needed `-mno-constructor-aliases` before it emitted
the constructors the other files called. Two compilers and two build types caught it. The cost is small, so build them all before calling
anything done.

### Make determinism a feature, not an accident

Recording and replay came after the game worked, so they had to be retrofitted. Everything
from outside had to move into one place:

- the game's clock moves only in `plat_pump`, in fixed steps, and stands still between pumps;
- input is taken only there;
- anything else about the host (file times, free space, the date) is a recorded value;
- the sound thread gives the game nothing back;
- allocated memory starts zeroed, and directory listings are sorted.

Once that held, recordings replayed identically across clang Debug, clang Release and GCC, and
every recording became a regression test: a replay that diverges after a change says the
change altered the game. For the next port, put the clock and input behind the pump on day
one.

### Test harnesses have side effects

Scripted runs keep their saves in the data folder. Later test runs on a copy of that folder
inherited a game in progress, and so started somewhere else than a new game would. Every
scripted run now empties `GAMEDAT` first, and the editor's "Test in the Port" uses a home
folder of its own. When a test starts from a folder, check what the previous test left in it.

### Cross-compile, and test the result where you can

The Windows build was made on Linux with mingw-w64 rather than on Windows, and tested under
Wine, where a scripted new game gives the same frames as the Linux build, pixel for pixel.
Getting there meant working around mingw-w64's headers rather than the game:

| Problem | Fix |
| --- | --- |
| GUIDs defined twice | an audio wrapper defining `COBJMACROS`, and the one property key defined by hand |
| libmatoya's Direct3D renderers need Microsoft's `fxc` for their shaders | stubbed out; the Windows build draws with Vulkan, falling back to OpenGL |
| `U8_INLINE` functions defined in many objects | `-Wl,--allow-multiple-definition` (they are identical) |
| `xinput1_3` not available | `xinput1_4` |
| `fprintf` in game code recursed through the `vfprintf` macro | `#undef vfprintf` in the wrapper |

The game's own code needed no change for Windows. All the work was in the platform layer,
the port's library wrappers and the toolchain, which is where a platform layer is supposed
to keep it.

### Read what the original reads

Weapon, armour and monster statistics are compiled into the port from the decompiled
sources, so a U8.EXE changed by the editor would play differently in DOS than in the port.
The port now reads those tables from the U8.EXE in the game folder (`game/port/exetables.cpp`),
found by what editing leaves alone (the shape numbers, the ends of the armour lists). With
the original U8.EXE nothing changes, and old recordings still replay. The rule for a port
meant to run mods: anything the original took from a file, take from that file.

### The port is the test bed for everything after it

The editor and the language tool were checked by playing what they wrote in the port, with
scripted input: placed and moved items, changed statistics, and the Portuguese opening
conversation. That only works because the port starts quickly, runs without a window and
takes scripted input. Keep it that way.

## What is still open

- The clang-cl build on Windows and the macOS build have not been run.
- The editor's Windows build needs Qt 6 for Windows: build it there, or build Qt with
  mingw-w64.
- Recordings are made by the port only; a DOS session cannot be replayed.

## In short

- Start from a port that already works, and copy its seams.
- Convert mechanically, then make the whole tree compile before running anything.
- Translate computation, replace hardware.
- Expect the real bugs to compile cleanly. Trace, script and screenshot to find them, and turn
  each new kind of bug into a tool that finds the rest.
- Build every compiler and configuration.
- Put the clock and the input behind one function from the first day.
- Read data from where the original read it, so mods behave the same everywhere.
