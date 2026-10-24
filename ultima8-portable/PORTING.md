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

The modules that drove the DOS extender, the interrupt controller, the descriptor tables,
the Sound Blaster and the mouse driver (`INTERUPT`, `NEWHOOK`, `REDIRECT`, `SETREGS`,
`GETREGS`, `SB`, `SBDAC`, and the `DEXT286`/`DOSFIX32` code) have no port: the platform layer
does their work.

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

## Checking a file

`tools/syntax.sh [files]` compiles files with `-fsyntax-only` and lists errors; the build
itself is CMake (see README.md).
