# How the sources were ported

Notes for whoever works on `game/`. The decompiled sources in `../source` build U8.EXE
byte for byte with Borland C++ 3.1 for a 16-bit Phar Lap 286 protected-mode target; `game/`
is the same program made to build with clang, GCC and clang-cl as a 64-bit program, talking
to the machine only through `platform/plat.h`.

## What Borland C++ 3.1 (large model) meant, and how it is spelled now

| Borland | Now |
| --- | --- |
| `int`, `short`, `signed` (16 bits) | `int16_t` |
| `unsigned`, `unsigned int`, `unsigned short` | `uint16_t` |
| `long` | `int32_t` |
| `unsigned long` | `uint32_t` |
| `char` (signed) | `char`, built with `-fsigned-char` (clang-cl: signed already) |
| `unsigned char` | `uint8_t` |
| far pointers (4 bytes, selector:offset) | ordinary pointers (8 bytes) |
| enums (16 bits) | as the host lays them out; give an enum `: int16_t` where its size matters (files, usecode) |
| structs packed to 1 byte | `#pragma pack(1)` in `u8port.h`, for every game header |
| all `.C` files compiled as C++ (`-P`) | all files are `.cpp` |

`tools/convert_source.py` made the first pass (types, lower-case names, includes, dropped
`far`/`near`/`pascal`/`register`). Everything after that was done by hand.

Integer promotion is the main trap left. On Borland, `int` was 16 bits, so arithmetic on
`int16_t`/`uint16_t` wrapped at 16 bits and `unsigned op int` was unsigned; on the host both
promote to a 32-bit `int`. Where the result goes straight back into a 16-bit variable nothing
changes. Where it is compared, shifted right, divided, or used as an index without being stored
first, cast it back: `(int16_t)(a - b) < 0`, `(uint16_t)(x + 1) == 0`. A constant compared
with a 16-bit variable (`x == 0xFFFF` with `int16_t x`) must be written for the variable's type
(`x == -1` or `(uint16_t)x == 0xFFFF`). Build with `-Wtype-limits -Wsign-compare` to find these.

## Memory

Phar Lap gave every allocation its own selector, with the block at offset 0. Code that used
that (`FP_SEG(p)` as a handle, `MK_FP(seg, 0)` to get the block back, `(uint16_t)p` as the
offset into a block) keeps a pointer to the block instead and computes offsets by subtraction.
`ProtMemoryManager`, `SelectorManager`, the descriptor tables and the 32-bit linear addresses
that went with them are gone: `malloc`, `new` and plain pointers do their job.

- `Vport::seg` (selector of the pixels) is `uint8_t *Vport::mem`. Row `y` starts at
  `mem + graphicYtable.index[y]` (16-bit offsets, as before; no viewport exceeds 64K).
- The screen (`VgaScreenDescriptor`, A000:0000) is `uint8_t *VgaScreen`, 320x200 bytes,
  shown by `plat_video_present`.
- The shape cache keeps its 32-bit offsets (`CacheNodePtr::offset`) into one block,
  `CacheNodePtr::memBase`; `CacheNodePtr::getPtr(o)` is `(char *)memBase + o`.
- Usecode keeps 32-bit "far pointers" on its own stack. `game/port/farptr.h` turns them into
  host pointers and back (`u8_far_from_ptr`, `u8_ptr_from_far`): the high word names a
  registered block (a process's stack, the YAMM heap, a unit's code), the low word is the
  offset into it, as a selector and offset did.

## Assembly

Each `.ASM` module is a `.cpp` file of the same name beside where the `.ASM` was (for
example `source/U8/SKIPDRAW.ASM` is `game/u8/skipdraw.cpp`), written in portable C++ that does
what the assembly did, including its clipping, its return values and its 16-bit arithmetic.
The comments at the top of each `.ASM` describe its interface and are kept. Functions keep
their names and C++ signatures (`extern "C"` and `pascal` are dropped).

The modules that drove the DOS extender, the descriptor tables and the hardware are replaced
rather than translated: `INTERUPT` and `NEWHOOK` keep their interfaces over the platform
layer (an emulated interrupt mask; the mouse hook), `SBDAC` queues the mixer's blocks with
`plat_pcm_queue`, `AIL16` (the Miles timer services, of which U8 used none) is a few empty
functions in `game/sound/xmidi.cpp`, and `REDIRECT`, `SETREGS`, `GETREGS`, `SMEMCPY`, `SB`
and the `DEXT286`/`DOSFIX32` code have no counterpart. The Miles XMIDI driver DLL that the
installer copied in (`AILXMI.DLL`) is `game/sound/xmidi.cpp` (the sequencer) with
`xmimt32.cpp` and `xmiopl.cpp` (the synths).

## DOS, BIOS and hardware

- Files: the `BaseFile` routines (`OPENFILE`, `READFILE`, ...) call `plat_file_*`.
  `findfirst`/`findnext`/`fnsplit` are in `game/port/u8dos.cpp`.
- printf and friends: `u8port.h` maps them to wrappers that read Borland format strings
  (`%ld` is 32 bits, `%Fs` a pointer) on any host.
- Timer: the kernel's real-time executor ran from the PIT interrupt. It runs from
  `plat_pump` through `plat_pit_set`, so every busy-wait in the game calls `plat_yield()`.
- Mouse: the driver's event hook is `plat_mouse_set_handler`.
- Keyboard: the BIOS buffer is `plat_key_available`/`plat_key_get`.
- Palette: `SETCOLORRGB`/`GETCOLORRGB` call `plat_video_set_palette`/`get_palette`.

## Traps found while bringing the game up

These all built cleanly and failed at run time. Look for more of the same kind.

- **Functions declared `void` that returned a value.** Borland returned whatever the last
  call left in AX, and usecode, which declares the intrinsics itself, used it: `Item::ask`
  returns the pid of the answer menu's process, which the conversation waits on;
  `Item::getSliderInput` and `Npc::cSetActivity` return values too. `tools/data/intrinsics.txt`
  lists the intrinsics; compare their C++ return types with ScummVM's list of what usecode
  expects, and read the DOS code (`out/U8.EXE`, map in `out/U8.MAP`) to see what AX held.
  Ordinary functions that fall off their end are errors (`-Werror=return-type`).
- **Writable string literals.** Borland put literals in writable data and the game changes
  some in place (the language letter of `eusecode.flx`). Global `char *x = "..."` now point
  at writable arrays; a literal written through a local pointer crashes and needs the same.
- **Usecode far pointers whose offset matters.** Usecode turns a YAMM handle into a far
  pointer and later frees the string through the pointer's low word, so the YAMM heap must be
  registered at its fixed selector (`FAR_YAMM`) with offset = handle.
- **Data the DOS save format carried implicitly.** `KERNEL.DAT`'s statics record is 157 bytes
  of the data segment and included the real-time list's count; without it the restored
  timers never ran. When a structure is saved as raw memory, check what else lay in it.
- **Globals named after a process.** Processes rebuilt from `KERNEL.DAT` replace the ones
  made at start-up; each class's `load`/`postLoad` repoints its global (`systemTimer`,
  `theCameraProcess`). Restored objects are built with the global `operator new`, as the
  original did, not the class's.
- **Inline functions per file.** The original defines header inlines (`Rect::Rect`,
  `NewGumpId::NewGumpId`) in each file that uses them, and some files call them with no
  definition in sight. Borland emitted an unexpanded inline as a "virtual segment" and the
  linker kept one copy, which those files called. They are spelled `U8_INLINE` now
  (`u8port.h`: `inline __attribute__((used))`), so every defining file emits them at any
  optimisation level; clang also needs `-mno-constructor-aliases` to emit complete-object
  constructors (CMakeLists.txt). Virtual functions defined only as such per-file inlines leave
  no file sure to emit the vtable, so those are ordinary out-of-line functions. A Debug build
  links even when a Release build would not: build both.
- **16-bit arithmetic.** See the first section; `tools/check_int16.py` lists comparisons,
  divisions and shifts that mix signed and unsigned 16-bit operands, and 16-bit arithmetic
  that Borland would have wrapped.

## Keeping the game deterministic

Recordings (README.md, "Recording and replaying") replay because everything from outside that
the game sees comes through the platform layer at known points, and the backend records it
there (`platform/matoya/record.c`). Changes to the port must keep it so:

- **Time moves only in `plat_pump`.** `plat_ticks`, `plat_milliseconds` and
  `plat_microseconds` read the game's clock, which the pump moves on in whole steps of
  `CLOCK_QUANTUM_US` and which stands still between pumps. A loop that waits on the clock
  must pump (`plat_yield`), or it never ends. Host code that needs real time (frame pacing,
  audio) reads `backend_elapsed_us`, and the game must never see what it reads.
- **Input arrives only in `plat_pump`.** Keys, the mouse and focus changes are taken from the
  app thread's queue there, and nowhere else.
- **Anything else from outside is a recorded value.** File times, free disk space, the day of
  the week and the time of day go through `record_value` (`plat_day_of_week`,
  `plat_time_now`). A new call that returns something about the host must too; directory
  listings are sorted by name.
- **Sound gives the game nothing back from the audio thread.** The speech queue's
  `plat_pcm_pending` drains by the game's clock, and a replay plays music on the device it
  was recorded with (`plat_music_device`).
- **Memory starts zeroed.** `malloc`, `farmalloc` and `new` are `calloc` (`u8port.h`), so
  what the game reads before writing is the same on every run; the heap is shared with
  other threads, so what a block held before is not.
- **The state digest** (`game/port/digest.cpp`) covers the random seeds, the item arrays,
  the global flags and the system timer, never host pointers. A replay that diverges after a
  change shows the change altered what the game does; if that was the point (a bug fix),
  record the session again.

`tools/u8rec.py info|dump SESSION` reads a recording.

## Checking a file

`tools/syntax.sh [files]` compiles files with `-fsyntax-only` and lists errors; the build
itself is CMake (see README.md).
