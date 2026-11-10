# Roadmap (written 2026-11-09)

Six workstreams for the portable port (`ultima8-portable/`) and its editor. This is a plan, not
a record of work done. Facts below come from reading the repo (READMEs, `HANDOFF-live-view.md`,
`PLAN-assists.md`, `game/u8/world.cpp`, `game/item/npc.h`, the recordings' headers) and the
sibling projects `underworld-exhumed-enhanced` and `UltimaUnderworldEditorAvalonia`. Things I
did **not** verify are marked *(unverified)*. Sizes are S (a session), M (a few sessions),
L (a week or more of sessions).

## 0. Order, dependencies and who can work in parallel

```
      W1 replay gate  ─────────────┬──────────────► guards every engine change below
      (S/M, do first)              │
                                   ▼
 W5a Enhancements plumbing ──► W5b..W5e individual enhancements ──► W6 cheats screen (shares settings.c)
 (tab, mask, recording header)

 W4a finish live-view (commit) ──► W4b debug channel ──► W4c source-level debugging ◄── W3 source map
 W3a exact assembler ──► W3b decompiler ──► W3c compiler ──► W3d UI + names

 W2 Exhume research (read-only, feeds W1 and later "faithfulness" work)
```

| Stream | Touches | Conflicts with |
| --- | --- | --- |
| W1 replay gate | `tools/`, `record.c`/`events.c` (small), CI script | nothing big |
| W2 Exhume study | docs only | none |
| W3 usecode language | `editor/src/data`, `editor/src/ui/usecode*` | W4c (needs its source map) |
| W4 live debug | `game/u8/inter.cpp`, `game/port/livelink.cpp`, `editor/src/ui/livelink*`, `mainwindow` | W3 on `usecodeview` |
| W5 enhancements | `game/u8/*`, `game/item/*`, `platform/matoya/settings.c`, `record.c` | W6 on `settings.c`, W1 on `record.c` |
| W6 cheats screen | `settings.c`, `game/u8/world.cpp` | W5 on `settings.c` |

Suggested agent split: **A** = W1 then W5 (engine side, one writer); **B** = W3 (editor data
layer, standalone); **C** = W4 after A has committed live-view; **D** = W2 (read-only) then W6.
Never two writers on `settings.c` or `record.c` at once. Each agent works in its own worktree.

**Housekeeping before anything starts (S):** commit the `live-view` work (the handoff's step 7;
leave the three libmatoya `apple*` submodule changes out), and decide where the 5 recordings
(90 MB, plus `stage/` copies of saves) live: they should stay out of git (git-ignore `replays/`,
or use LFS), with a `replays/MANIFEST.txt` of names, game time and what each covers.

---

## W1. Recordings as a headless, fast "is it still beatable" gate

**What exists.** `Ultima8 --replay <folder>` already replays with no window and no sound, as fast
as the game runs (README says 10 to 20 times real time), on a copy of `stage/`. Exit code 0 means
all checkpoints (a state digest every game second) matched, 2 means divergence, 1 means could not
run. So the answer to "can they be run headless and fast-forwarded" is **yes, already**; what is
missing is the "beatable" part, batch running, and speed-ups.

**The five recordings** (from `u8rec.py info`): 

| Folder | Game time | Note |
| --- | --- | --- |
| 20261104-153626 | 1312 s | short |
| 20261105-131522 | 10072 s | long (34 MB) |
| 20261109-065148 | 1143 s | short |
| 20261109-071611 | 360 s | short |
| 20261109-102556 | 10613 s | long (33 MB); ends with a clean `END 0` |

At 10 to 20x, each long one should take roughly 9 to 18 minutes *(unverified: nobody has timed
them)*.

**Update:** the data folder is `C:\Users\karl9\Downloads\Claude\U8_test\PORTUGUESE` (fingerprint
matches). The 360 s recording replays with all 360 checkpoints matching, but took 3m48s: about
0.6x real time, not the 10-20x the README claims (that recording is 1.4 billion pumps). The long
ones (23 and 28 billion pumps) may take about an hour or more each. `tools/replay_all.py` is
written; `flicplay.cpp` now logs `ENDGAME reached` (not yet built or tested).

**First full run** (5 in parallel, `U8_test\PORTUGUESE`, current build): 4 pass, 1 fails.

| Recording | Result | Wall time |
| --- | --- | --- |
| 20261104-153626 | ok, 1312 checkpoints | 328 s |
| 20261105-131522 | **diverged at game second 5649** (digest differs; 5648 matched) | 2917 s |
| 20261109-065148 | ok, 1142 checkpoints | 250 s |
| 20261109-071611 | ok, 360 checkpoints | 281 s |
| 20261109-102556 | ok, all 10613 checkpoints | 3191 s |

The long recording (10613 s) replays to the end. Whether it reached the ending is not yet known
(needs the `ENDGAME reached` build). The failing one needs triage: either you edited the game
files during that session (two edits were mentioned), or a real engine difference; the
`--shots` pictures and a replay of the same recording on the commit it was made with would tell.

**Decisions (2026-11-09):** (1) `20261105-131522` is kept as stale, out of the gate. (2) The
recordings were meant as one playthrough in order; the latest reached the endgame (to be
confirmed by the `ENDGAME reached` build; `STATIC/I81B4U83.ICU`, the endgame flag file, in the
test data folder is dated after the batch run, which supports it). (3) The real gate is the
**English** release, so it needs new recordings in English; the Portuguese ones stay as a
Portuguese regression set meanwhile. (4) "Beaten" = `playEndgame()` called, nothing more.
(5) Record **short segments**, each starting from a save, instead of one long run: needs
`--replay-until <s>` and `--replay-save-stage <dir>` (write the state at second N as a new stage)
so a long recording can be cut, and a manifest saying which segment starts from which save.
(6) `replays/` stays in `.gitignore`.

**Gotcha found (2026-11-09):** the endgame flag file `STATIC/I81B4U83.ICU` is written into the
data folder by `playEndgame()` (a replay does it too), and the game's main menu reads it, so once
it exists the recordings diverge at game time 0.000 s. Replays must run on a throwaway copy of the
data folder **without** that file (`replay_all.py` should copy the data folder per run and delete
it; TODO). The copy in `U8_test\PORTUGUESE` currently has it (written by the batch run).

**Progress:** `build/port2` has the `ENDGAME reached` build (compiles). Added `--replay-until <s>`
(stops after the checkpoint at that game second; exit 0 if all matched), written but not yet built.
`--replay-save-stage` is dropped: a state saved mid-session does not reload bit-identically, so a
cut recording would not keep its checkpoints. Segments are recorded fresh, each from a save.
**Confirmed:** `20261109-102556` replayed alone on the new build prints `ENDGAME reached`, matches
all 10613 checkpoints, exit 0, in 38m47s (about 4.5x real time when run alone, no cheat note). So the
Portuguese playthrough is beatable as recorded. (`plat_log` printed no newline after it; cosmetic.)

**Earlier status (blocked on the data folder, now resolved):** Replaying the shortest recording
(`20261109-071611`) with the current `build/port/Ultima8.exe` diverges at game time 0.000 s
("the game read value 1 where the recording has an input step"), with a data-fingerprint warning:
the recordings say `data=c0f3a9899ca6030b`, but `gamedata/ENGLISH`, FRENCH, GERMAN, PORTUGUESE and
`C:\GOG Games\Ultima 8\ENGLISH` give `367c2a66512ea3a0`, `4ee297135af12052`, `d91b6e46ca685c8e`,
`f6d043195a319f58` and `367c2a66512ea3a0`. The fingerprint hashes the names and sizes of the files
in `STATIC` and `USECODE`, so the recordings were made on a data folder with a different file list
or sizes (e.g. a modified or extra file in USECODE). Needed: that folder (or the recipe, e.g. the
editor's Test-in-Port copy). The old `windows/Ultima8.exe` behaves the same, so it is not the build.

**Gaps and tasks**

1. **Time and verify (S).** Build the port as it is now, replay all five, record wall time and exit
   codes. Replays need the same data fingerprint (`data: c0f3a9899ca6030b` in the last one) and
   the same engine behaviour: any engine change that alters the digest makes them diverge, which
   is the point of the gate but means a failure needs triage (intended change vs regression).
2. **Define "beaten" (S).** The game has an `playEndgame()` intrinsic (0x6B, `game/u8/intrinsics.cpp`).
   Add a log line and a distinct result when it is called, e.g. `ENDGAME reached at game time T`,
   and `--replay --expect-endgame` (exit code 3 if the recording ended without it). Also record
   whether `avatar.cheater` was ever set (the game marks cheaters with ".1" in the version), so a
   "beaten" claim can reject runs that used the cheat window. *Open question for you: do the long
   recordings actually reach the ending? I could not tell from the headers.*
3. **Batch runner (S).** `tools/replay_all.py`: replays every folder in `replays/` (parallel jobs,
   each on its own copy of the stage), prints a table (recording, result, endgame?, cheater?,
   wall time), exits non-zero on any failure. This is the one command to run before merging an
   engine change.
4. **Fast-forward for humans (S).** `--replay-view` plays at recorded speed; add `--replay-speed N`
   (and optionally `--replay-until <game-second>`) so you can watch a section quickly, plus
   `--shots` already gives pictures to compare. Also make replays stop at the first divergence
   *and* print the nearest earlier `--shots` frame number, to ease triage.
5. **Milestone coverage (M, optional).** "Reached the endgame" proves one route. Add a milestone
   report at the end of a replay from global flags (names from `flagsym.dat`, which the cheat
   window already reads): e.g. which quest flags were set, which maps were visited. Lets us say
   *what* the recordings prove, and what no recording covers (e.g. alternate paths).
6. **Interaction with W5 (rule).** Every enhancement is off by default and recorded in the
   recording header (see W5a). The gate always runs with enhancements off; separate "enhanced"
   recordings can be made later for regression of the enhancements themselves.

**Done when:** `python tools/replay_all.py` exits 0 on all five, reports ENDGAME for the
recording(s) that finish the game, and takes under about 30 minutes in total.

---

## W2. What Exhume has that can prove the port is faithful

`underworld-exhumed-enhanced` is the same idea for Ultima Underworld. I read its README, PORT.md
and ENHANCEMENTS.md, not its tools *(unverified: what each tool does in detail)*. Candidates,
roughly by value for us:

| Exhume feature | What it gives | Fit for U8 |
| --- | --- | --- |
| **Golden references from the original DOS program**: sessions are recorded in DOS, with a state digest and screenshot at each checkpoint; the port must match them | Proves the port equals the *original*, not only itself. Our recordings were made in the port, so today they prove "unchanged since", not "same as DOS" | Highest value. Needs U8.EXE run under an emulator with a capture harness and the same digest definition. Large (L); do a feasibility study first |
| **Routine fuzzing**: the original bytes run in an x86 emulator beside the port's C for the same routine and inputs | Catches differences no recorded session reaches (rare branches, 16-bit overflow) | Good for the leaf routines (damage, RNG, collision, animation). We have `tools/check_int16.py` already for one class of bug |
| **Test vectors** (`uwN/vectors/`): input/output tables made from the original code | Cheap unit tests that stay valid when the engine changes | Pair with fuzzing |
| **Byte-matching rebuild** of the EXE with the original compiler | The strongest statement ("this source *is* the program") | Probably out of scope: depends on having the Borland toolchain and a source that matches; check how far `source/` is from it |
| **Crosswalk** (every function by its IDA name and address, with matched source) | Traceability from port code to original bytes | Cheap if we have a symbol list; a table generated from `source/` |
| **Behaviour docs** with exact numbers, **FINDINGS.md** (likely bugs in the original), **FORMATS.md** | Documents what "faithful" means, and the bugs we deliberately keep | Start `GAME-MECHANICS.md` into this shape |
| **Enhancement discipline**: every change is named, typed (timing / presentation / gameplay, plus "fixes"), off by default, logged as "not as DOS", and written into the recording | Exactly the contract W5 needs | Adopt wholesale (see W5a) |
| **Presentation proofs**: for a replaced renderer, a proof that it covers exactly the same pixels | Safe to modernise drawing | Only if we ever replace the renderer |
| **CI on three OSes, including from the release zip** | Determinism across compilers and platforms | Our README says it holds for Debug/Release/GCC/clang on Linux; extend to Windows/macOS and to the packaged build |

**Deliverable (S):** a short `docs/FAITHFULNESS.md` that ranks these, says what we have and what
we lack, and proposes the first two to do (my guess: the DOS golden-reference feasibility study,
then fuzzing of the damage/RNG/animation routines). Read-only, good for a side agent.

---

## W3. Usecode in a C-like language: decompile, edit, compile, byte-exact when unmodified

Model: the Avalonia editor's conversation language (`docs/conversation-language.md`):
`StructuredFront/Build/Print` (decompile), `ConversationSource/Compiler` (compile), `Assembly`
(bytecode assembler), tests that every conversation disassembles and assembles to the same bytes.

**Starting point in this repo:** `editor/src/data/ucode.cpp` (instruction table and decoder),
`disasm.cpp` (disassembler), `ucvm.cpp` (a usecode machine), `usecodegraph.cpp`, `usecodeview.cpp`
(383 lines), class names for all classes in `source/EUSECODE/*.unk`, flag names in `flagsym.dat`,
intrinsics table with argument kinds (`intrinsics.inc`). No assembler, no structuring, no
compiler yet.

**The exactness contract (decide this first).** Two levels, and the second is what makes
"100% when unmodified" *guaranteed* instead of hoped for:

1. **Function-level byte cache.** Every decompiled function remembers the original bytes of its
   code. If its source text is unchanged (normalised), emit those bytes verbatim. Modified
   functions are compiled fresh. This is what the Avalonia editor does ("unchanged functions keep
   their exact bytes").
2. **Whole-class and whole-file exactness.** Rebuilding an untouched class (header, 32-event
   offset table, code, debug info, strings) and an untouched `EUSECODE.FLX` gives identical
   bytes. Tested over every class.

Compiled output for modified code does not have to match any original compiler (U8's own
compiler is not available); it only has to be correct, and tested semantically by running it in
`ucvm`.

**Phases**

- **W3a. Exact assembler (M). DONE 2026-11-09:** `editor/src/data/ucasm.{h,cpp}`, test `usecodeAssembly`: all 516 shipped classes print to text (7.7 M characters) and assemble back byte for byte, and the whole `EUSECODE.FLX` rebuilt from them is identical; adding code moves labels, events and the header sizes. Cross-class call/spawn offsets stay numeric until the whole-file link step. Original text: Text form of the disassembly that assembles back to the same bytes:
  labels for jump targets, far calls, string/data section, class header, event table, the debug
  unit opcode (0x5c) operands. Test: all classes of the shipped `EUSECODE.FLX` round trip
  byte for byte, and so does the Portuguese/other language packs the editor makes.
- **W3b status (2026-11-09): decompiler in C++, `editor/src/data/ucdecomp.{h,cpp}`, test `usecodeDecompile`.**
  **1562 of 1578 routines (99.0%)** of the shipped game decompile to C-like text; the rest stay as assembly
  (12 `spawn_inline`, 2 stack-not-empty-at-branch, 1 `store_list_element`, 1 goto). It lifts expressions,
  `if`/`else`, `while`, endless loops, `break`/`continue`/`return`, `foreach (x in list)`,
  `foreach (item in search_...)`, `await` and `implies(A, B)`, list `^`/`-`/`in`/element. `U8_DECOMP_OUT=<dir>`
  in the test run writes every class's text for reading. The comparison opcodes in `ucasm.cpp` were
  named the wrong way round (0x28 is a<b by the interpreter): fixed. Not yet proven right: nothing
  recompiles the text, so a lifted routine is only checked by reading it; W3c's recompile/compare is the proof.
  **Text parser (2026-11-09):** `ucast.{h,cpp}` (the shared expression/statement tree and its printer) and
  `ucparse.{h,cpp}` (text back to the tree). Test `usecodeDecompile`: all 1562 lifted routines parse and print
  again as the identical text. This proves the printed language is unambiguous; it does not yet carry what
  code generation needs (see W3c).
  Original description:
- **W3b. Decompiler (L).** Stack-machine code to expressions (the stack temporaries are the hard
  part), control-flow structuring (`if/else`, `while`, `for`, `switch` if the original has jump
  tables, early `return`), typed locals and parameters (int, long, pointer, string, list), calls
  to intrinsics with names and argument kinds, calls to other classes' functions and process
  spawning (`spawn`, `wait`) and event handlers by name (`use`, `look`, `hatch`...). Anything it
  cannot structure stays as an inline `asm { ... }` block, so every class always decompiles and
  round trips (this is how the exactness contract survives odd code). Test: decompile then
  recompile-by-cache is trivial; so also run **decompile, rebuild from the AST without the cache**
  and report the percentage of functions that come out byte-identical. That number measures
  decompiler quality and tells us how often a user edit disturbs code it did not touch.
- **W3c plan (decided 2026-11-09).** Bytes come from the tree, not from the text directly, so the order is
  (1) make the text carry what code needs: a `frame N;` line, types of locals and arguments (byte, int, long,
  string, list), long constants marked (`0L`, today `push_long_imm 0` and `push_imm 0` both print `0`),
  string pointer versus string handle, and the cleanup the decompiler hides (`free_string_sp k` after a
  call, `free_*_local` before a store and at the end, `give_heap`); (2) code generation from the tree by
  rules (operand opcode chosen by type and value range, args pushed last first, `add_sp -n` after calls,
  frees for string and list temporaries, `locals` from the frame line); (3) the check: recompile every
  lifted routine and count how many give the original bytes, listing the first differing instruction of
  the rest, then run the differing ones in `ucvm` against the originals. An unchanged routine always
  keeps its original bytes (the byte cache), so exactness here only measures how faithful the rules are.
  Original description:
- **W3c. Compiler (L).** Parser and checker for the language, code generator to bytecode, the
  relocations a changed function size forces (offsets inside the class, event table, class
  sizes, the FLX index), strings, and clear errors with line numbers. Semantic test by running a
  few compiled functions in `ucvm` against the originals.
- **W3d. Names (M).** Placeholder names (`func_1A3`, `local_4`, `label_12`) replaced by names
  derived from context, **shipped with the editor** as a data file, keyed by (class, offset) with
  a fingerprint (hash) of the unmodified usecode: loaded only when the fingerprint matches.
  Derivation heuristics, to be run once offline and then reviewed by hand for the important
  classes: event handlers by event name; functions that call known intrinsics (`Item::hurl` to
  `launch`, `Npc::createNpc`...); names from strings they print; flags named from `flagsym.dat`;
  locals from the intrinsic argument they are passed to or the shape names (`itemnames.cpp`).
  When the usecode is modified (fingerprint differs), the shipped names are not applied; names
  can be set per project with **F2** (rename in place, all references updated), stored in a
  sidecar file beside the usecode.
- **W3e. UI (M).** Source tab in the usecode view with highlighting, error list, F2 rename,
  diff against original, "Apply" (compiles the class, shows size change and what moved), undo, and a
  "this class is unmodified: byte-exact" indicator. Also produces the **offset-to-line map** W4c
  needs.

**Order:** a, then b and the names data in parallel, then c, then e. Check in after a with the user, since
a failing round trip on any class changes the plan.

---

## W4. Syncing with the running game and debugging usecode live

**State today** (`HANDOFF-live-view.md`): the port writes snapshots every 500 ms to a link folder
(item cache, flags, avatar position); the editor shows a live read-only map and can pull a
full state. Uncommitted, tests not yet run. Debugging and live editing were explicitly out of
scope until now.

- **W4a. Finish live-view (S).** The handoff's next steps: run the editor tests with `U8_LINK_DIR`,
  hand test, show live flags in the Flags tab, docs, commit (no libmatoya files).
- **W4b. A debug channel (M).** Files at 2 Hz are too slow for stepping. Add a loopback TCP (or
  named pipe) connection, opened only with `--debug <port>`, with a small request/response
  protocol: pause, resume, step, set/clear breakpoint, read stack/locals/globals/flags/item.
  Port side hooks the interpreter's instruction dispatch (`game/u8/inter.cpp`) and the process
  list. Keep the link file mechanism as the fallback for the map view.
- **W4c. Debugger features (L), in order of usefulness:**
  1. Pause and resume the game; show the running processes (class, offset, waiting on what).
  2. Breakpoints at (class, offset) in the **disassembly** view; step over/into/out; call stack;
     locals and the evaluation stack. Works without W3.
  3. Watch globals and flags, item fields; "break on flag change" and "break on class entered".
  4. Source-level view: map offsets to lines using W3e's map, step by line, variable names from W3d.
  5. Stretch: **hot reload** of a recompiled class into the running game, and set-variable while
     paused. Needs rules for processes already running the old code (finish with old code, new
     spawns use new code).
- **Determinism rules:** a paused game must not advance the real-time clock the game reads (else a
  recording gets a huge time jump); simplest safe rule: debugging disables the black box and
  prints so, unless pause time is excluded from `backend_elapsed_us()`. Decide and test. Breakpoint
  checks must cost nothing when the debugger is off (a flag test per instruction at most, and the
  replay gate in W1 must still pass).

---

## W5. Enhancements tab in the port

`PLAN-assists.md` already designs the plumbing and four assists. Fold it into this, rename the tab
**Enhancements**, and adopt Exhume's discipline: each entry is named, off by default, typed
(timing / presentation / gameplay), logged as "not as the original", and written into the
recording header.

**W5a. Plumbing (M)** as in `PLAN-assists.md` section 0: the tab in `settings.c` (constant tab
count; 5 rows per tab, so more tabs or a scrolling list for about 12 rows), `plat_enhancements()` mask
(plus small numeric values such as interaction distance factor), latched on the game thread at a
fixed point in `plat_pump`, header key `enhance=<hex>` and a new recording op for mid-game
changes (`U8REC 2`; v1 files still read). Presentation-only entries (d) do not change the digest and do
not need latching.

| | Request | Approach | Class | Size |
| --- | --- | --- | --- | --- |
| a | **Easier movement**, narrow corridors and bends | `PLAN-assists.md` 1a: bigger run zone (`calcErection`), turn in stride, slide round corners before slowing (`AnimPrimitive::initialize`) | gameplay | M |
| b | **Easier running jumps** | `PLAN-assists.md` 1b: grace period, queue the press, aim the leap, catch ledges without the button. Ordered, each useful alone | gameplay | M |
| c | **Longer interaction distance** | Find where "too far" is decided (item use/pick up/open range, probably `Item`/`world.cpp` reach tests and the usecode `getRange`-style intrinsics) *(unverified: not located yet)*; scale by a setting (×1.5, ×2) | gameplay | S/M |
| d | **Stat gain display** | `Avatar` already has `strengthPoints`, `dexterityPoints`, `intelligencePoints` (progress toward next point). Find the gain code to get the threshold per level and the max (probably in usecode/`npc.cpp`); show "points / needed, current / max" in the stats screen or as a HUD line. Read-only, so it cannot change the simulation | presentation | S/M |
| f | **Easier climbing** (added 2026-11-09) | Climbing is the grab-ledge/climb animation chain started from a fall or jump near a ledge. Study `gravity.cpp` ledge grab (~488-549, reads the buttons directly) and the climb animations in `animprim.cpp`; options: larger grab reach, grab without holding the button (shared with b.4), forgiving height tolerance for the climb-up step. Not yet studied *(unverified)* | gameplay | M |
| g | **See-through objects above the player** (added 2026-11-09) | When something (wall, roof, ceiling) is drawn in front of or above the Avatar, draw it transparent (partial = alpha/dither, total = skipped). Done in the renderer's draw list (`DL_ItemNode`/camera), only for items overlapping the Avatar's screen box and whose z is above the Avatar's head. Pure presentation: must not touch the simulation, so no replay/digest impact; modes Off / Partial / Total. Risk: the 8-bit palette renderer has no alpha, so partial = a dither pattern or the game's existing translucency xform tables (`XFORMPAL.DAT`) *(unverified)* | presentation | M/L |
| e1 | **No spitters** | `PLAN-assists.md` 2: block `enterFastArea` for class 795 in `spawnUnk`, clear running loops | gameplay | S |
| e2 | **No falling stalactites** | same section: block hatch for class 1181 (the plan covers it; you did not list it again, so say if you want it dropped) | gameplay | S |
| e3 | **No falling platforms, sinking or disappearing platforms, crumbling floors into lava** | Same hook. Needs a research pass to identify the classes and eggs (search `source/EUSECODE` for the shapes and the quest areas, then test by playing there). Disabling must leave the platform solid and in place, not just stop the event | gameplay | M |
| e4 | **Longer jump distance** | Scale the leap's frame offsets (ANIM.DAT-driven) by a factor, with a landing check; depends on b's aiming to keep landings sane | gameplay | M |

**Rules for all of them:** (1) each ships with a scripted replay test on and off (`U8_TEST_INPUT`
with `--record`, then `--replay` must exit 0); (2) with everything off, W1's gate still passes;
(3) after W5e3 is playable, **re-run the W1 gate with them on** only as new recordings, never against the
old ones.

---

## W6. Cheats screen in the port menu

**What the game ships** (`game/u8/world.cpp`, `CheatWindow`; reached only when `avatar.canCheat`,
which `-cheat <string>` sets via `performCheatCheck` in `main.cpp`):

- Toggles: Hackmover (H), Power Avatar / immortal (P), Footpads (F), Egg bounds (E), plus keys `U`
  (draw "ugli kolur") and `I` (inspect an item).
- Teleport: to NPC, to location (x, y, z, map), to map + egg, to the "Happy Hunting Ground" (map
  0x3d), to the "Gift Shop" (map 1).
- View/Edit: NPC stats, usecode flags by name (from `flagsym.dat`), time in hours, item stats, item quality.
- Listen: any SFX, any music.
- Function-key handlers (`f1key` to `f6key`, `ctrlF1..F8`, `shiftF1..F8`, `altF1..F8`, `f10key`),
  not yet catalogued, and further cheat code in `world.cpp` (around lines 914, 938, 1058).

**Plan**

1. **Catalogue (S).** List every cheat and debug key and what each does, from the decompiled code
   (including the function keys); write `docs/CHEATS.md`. This is the "all cheats shipped with the
   game" list; I only read part of it.
2. **The screen (M).** A new entry in the port's menu (reached from the settings screen, `settings.c`) that
   lists each cheat with its key and a one-line description; selecting an entry runs it. Cheats
   that need arguments (teleport, flag edit) reuse the game's own prompts.
3. **Determinism choice.** Simplest and replay-safe: the screen only **enables `canCheat`** and
   feeds the game the same key events the cheat window reads, so everything is already in the
   input recording and no format change is needed. The alternative, calling game functions
   directly from the menu, needs a new recording op like W5a. Recommend the first.
4. **Honesty flag.** Using any cheat sets `avatar.cheater`, as the original does, so W1 can reject
   recordings that cheated, and the screen warns on first use.

Sequence after W5a, because both edit `settings.c`.

---

## Open questions (my defaults in brackets)

1. Does "beaten" mean the `playEndgame()` intrinsic fires? [yes]
2. Do the long recordings (10072 s and 10613 s) reach the ending? [please confirm; W1.1 finds out]
3. W3 exactness: is the function-level byte cache (changed functions only recompile) acceptable as
   the "100% when unmodified" guarantee, with decompile/recompile-without-cache reported as a
   quality metric? [yes]
4. W4 debugging while recording: disable the black box during debug sessions? [yes]
5. W5e2 stalactites were in the earlier plan but not this list: keep? [keep]
6. Recordings in git or not? [not; ignore and keep a manifest]
