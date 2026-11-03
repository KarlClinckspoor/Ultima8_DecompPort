# How Ultima VIII works: conversation, time, quests, statistics and combat

A description of the game's rules as the decompiled program implements them, for anyone
changing the game or writing a new one with it. Every rule here was read in the code. Where a
statement is an interpretation rather than something the code says outright, it is marked
*(inferred)*.

Where to look:

- **The engine**: `ultima8-portable/game/` (the decompiled C++ made portable; the same code in
  DOS spelling is in `source/`). Paths below are relative to `ultima8-portable/game/`.
- **The usecode**: the game's scripts, in `USECODE/EUSECODE.FLX`. `source/EUSECODE/*.unk` holds
  them as readable listings, one file per class. Usecode offsets below are `CLASS:offset`
  into a class's code; `ultima8-portable/editor/tools/lang/usecode.py` decodes them.
- **Classes**: an item's usecode class is its shape number (class 227 HOURGLAS is the hourglass,
  shape 227). A permanent NPC's class is `1024 + its NPC number` (1026 DEVON is NPC 2), and a
  usecode egg's is `1151 + its egg type` (`spawnUnk`, `u8/uprocess.cpp`). Class 1404 METHOD and
  class 1409 FREE hold routines the others share.

Most of the game is usecode. The engine supplies the clock, movement, animation, combat
arithmetic, gumps and about 250 intrinsics (`u8/intrinsics.cpp`); the story, the
conversations, the quests, the spells and most item behaviour are scripts.

## 1. Events: how the engine starts usecode

The engine sends an item an *event*; `spawnUnk` starts the item's class at that event's entry
if the class handles it. A class begins with 32 entry offsets, and the overload table (usecode
record 1) gives a 32-bit mask of the events each class handles.

| Slot | Event | Sent when | Classes handling it |
| --- | --- | --- | --- |
| 0 | look | the item is clicked once | 360 |
| 1 | use | the item is double-clicked | 158 |
| 2 | anim | the item's animation needs it | 8 |
| 3 | setActivity | `Npc::setActivity` | none |
| 4 | cachein | the item comes into the cache | 2 |
| 5 | hit | the item hits something | 19 |
| 6 | gotHit | the item is hit | 60 |
| 7 | hatch | an egg hatches | 48 |
| 8 | schedule | every game hour, and when a map loads (NPCs) | 35 |
| 9 | release | the item is let go | 10 |
| 10, 11 | equip, unequip | | |
| 12 | combine | | 1 |
| 14 | calledFromAnim | | |
| 15, 16 | enterFastArea, leaveFastArea | the item comes into or leaves the area around the Avatar | 87, 79 |
| 17 | cast | an NPC goes into combat | 35 |
| 18 | justMoved | | 1 |
| 19 | AvatarStoleSomething | | 19 |
| 20 | animGetHit | | |
| 21 | guardianBark | the Guardian speaks | 1 |

The names are the `#define`s in `item/item.cpp`. Slots 13 and 22 to 31 are never sent.

A running usecode routine is a *process* of the kernel. It waits for another process (a bark,
a menu, a spawned routine) by naming it as a dependency (opcode 0x54) and suspending; when that
process ends, its result is handed back (`NotifyList::notify`, `u8/notlist.cpp`; opcode 0x6d
reads it). Processes are saved with the game (`KERNEL.DAT`), so a scene half played survives a
save.

## 2. Time

### Clocks

The kernel runs real-time processes from the timer interrupt, each at its own rate
(`ProcessRealTime::setHertz`, `u8/process.cpp`), with the timer set to the fastest of them
(`Kernel::calculateTimerRate`):

| Process | Rate | Used for |
| --- | --- | --- |
| SystemTimer | 60 Hz | the main loop's pace; usecode delays |
| Animation | 30 Hz | **the game clock**; item animation |
| Wait | 30 Hz | timed waits (barks, stuns) |
| SoundTimer | 140 Hz | sound |
| Timer | 10 Hz | |
| StatRestorer | 1 Hz | regeneration |

The main loop (`Kernel::execute`, `u8/kernel.cpp`) runs every process once, then waits for the
next SystemTimer tick: at most 60 passes a second, with no catching up when a pass is slow.
There is no game-speed setting. The options menu's "Speed Limiting" lets running animations
skip their frame delays (`u8/animprim.cpp`), and "Text Speed" (0 to 9) sets how long barks stay.

### Game time

`Animation` (`u8/gametime.cpp`) counts 30 ticks a second and 27,000 ticks an hour:

- **A game hour is 15 real minutes; a game day is 6 real hours.** The hour is the only unit:
  `TimeInSeconds` and `TimeInMinutes` are real seconds and minutes of the clock.
- A day has six periods of four hours, named in the hourglass's usecode (HOURGLAS 227, its use
  event; the period is FREE's `hour % 24 / 4`):

  | Hours | 0-3 | 4-7 | 8-11 | 12-15 | 16-19 | 20-23 |
  | --- | --- | --- | --- | --- | --- | --- |
  | Period | Bloodwatch | FirstEbb | DayTide | ThreeMoons | LastEbb | EvenTide |

- The calendar is in the air spells' usecode (AIRSPEL 1430): day `d = hours / 24`; a six-day
  week (Guarday, Earthday, Waterday, Airday, Fireday, Blackday, by `d % 6`); 45-day months
  (`d % 45 + 1`) in a six-month year (Stonemark, Skyrock, Windbreak, Stormpeak, Firefall,
  DarkFlame), so 270 days a year. Usecode does this in 16-bit arithmetic, which would wrap
  after 32,767 game hours, about 340 days of real play *(inferred)*.
- A new game starts at tick 0, Bloodwatch on Guarday the 1st of Stonemark: the starting save
  has no `KERNEL.DAT`, so nothing sets the clock.
- The clock is saved: `KERNEL.DAT` holds the Animation process with its counters.
- The clock keeps running while the main menu is open; only the credits, the quotes and
  flic movies stop it *(inferred from what clears the clock's interrupt flag)*.

### What follows the clock

- **NPC schedules.** On every hour change, and on every map load, a `Scheduler`
  (`item/itemcach.cpp`) sends each NPC event 8 with the hour, one NPC a loop. 35 classes handle
  it: the named NPCs, the guards and AVATAR. DEVON's handler, for example, chooses a place from
  the period and plot flags and moves him there through METHOD; *(inferred)* the move is a
  teleport when the NPC or the place is far from the Avatar, so NPCs change places mostly out of
  sight. `Npc::setActivity` would start event 3, but no class handles it, so activities are the
  engine's own: loiter, combat, stand (`Npc::cSetActivity`).
- **No day and night.** Nothing in the engine reads the hour except the scheduler and a debugging
  console. The palette does change on the hourly event: AVATAR's schedule handler chooses
  normal, rainstorm or firestorm palettes by map and plot flags, which is weather following the
  story, not the time of day.
- **Resting.** The bedrolls and beds (BEDROLL 534, BEDEW 196, BEDNS 175) ask for one to five
  periods. They refuse in combat or with enemies near ("This is no time to rest!"), then fade
  out, move the clock on four hours a period, restore hit points and mana in full, and fade back
  in. The jump starts a full schedule pass.
- **Real time, not game time**, drives regeneration (section 5), stuns, feign death and the death
  screen.

### Waiting in usecode

Usecode has no sleep intrinsic. A routine waits with FREE's `delay(n)` (spawned by 156 classes),
which notes the SystemTimer tick and suspends until `n` ticks (60 a second) have passed, or by
depending on a process that ends when the wait is over: a bark, an answer menu, an animation, a
spawned routine.

## 3. Conversation

### Starting and ending

Double-clicking an NPC sends it the use event (`World::commandMouseLeft`, `u8/world.cpp`; there
is no reach check for NPCs). An NPC in combat ignores it, and a dead one opens its inventory
(`Item::use`). The NPC's class then:

1. marks its process as a conversation and gives up if one is already running for this NPC
   (opcodes 0x77 and 0x78), so a second double-click does nothing;
2. calls the shared opening in METHOD (1404:0bcf): it stops the NPC's pending barks and other
   processes, puts the Avatar in stasis (out of combat; keys ignored), turns the two to face
   each other and centres the camera on the NPC, and sets **global flag 15**, which marks a
   conversation in progress *(inferred)*. If the Avatar is invisible, the NPC says "Thought I
   heard someone..." and the conversation ends there;
3. shows menus until the player says goodbye, then calls METHOD's closing (1404:1090), which
   clears flag 15 and the stasis.

### What the NPC says

A line is a **bark** (`Item::bark`, `item/item.cpp`): text over the speaker's head, in a box 192
pixels wide and five lines high (`BarkGump`, `u8/bark.cpp`). `~` breaks a line, `*` a page,
`%` indents (`u8/format.cpp`). A page stays for `(11 - text speed) x (characters / 32) / 2`
seconds (at least one line's worth), three seconds per 32 characters at the default speed of 5,
or until clicked (clicks are ignored for its first 12 ticks). Usecode waits for each bark to
end before the next line, so a conversation goes one line at a time.

The speaker's colour is its font (`BarkGump`): the Avatar's lines are font 6, monsters' and
other items' font 8, and permanent NPCs font 0, 5 or 7 by their number modulo 3.

**Voices.** A bark is spoken instead of shown when a speech file has it:
`SOUND/<language letter><shape>.FLX`, so voices belong to a *shape*, not an NPC. The English game
has nine: the four Titans (Lithos 433, Stratos 385, Hydros 80, Pyros 109), four other shapes
(44, 129, 289, 597) and 666, the Guardian. The file's first record lists the recorded phrases;
a bark plays the first phrase it begins with, and the rest of the text plays the next matching
phrase when that one ends (`SpeechFlex::search`, `playSpeech`, `SpeechProcess`). While a voice
plays, **no text is shown**.

### What the Avatar says

The choices are a menu of strings (`Item::ask`, `u8/bark.cpp`): an `InputGump` over the Avatar,
one line per string in font 6, which waits 30 seconds. Clicking a line hands **the line's text**
back to usecode as a string (`InputElementGump::commandMouseLeft`), which the script compares
with each topic by string equality (opcode 0x26). Topics keep their trailing space
(`"Who are you? "`), and the comparison is exact. That is why a translation must translate a
topic the same everywhere it appears. If the time runs out, the answer is an empty string;
Devon says "Oh. I am sorry. I thought you were still speaking to me." the first time, and ends
the conversation the second.

A conversation is a loop in the NPC's class (DEVON 1026 from 0x86c): build the list of topics,
ask, compare the answer with each topic, run the matching branch, repeat until "Goodbye. ".
Branches change the list as they go, with list opcodes: 0x17 appends, 0x19 adds a topic not
already there, 0x1a removes one. In Devon's conversation, asking about "Tenebrae? " removes it
and "Sea of Rains? ", and adds "What is the lovelier place? ", "Who is the Lady? " and "I am
from far away. "; "Goodbye. " is moved to the end of the list each time.

Across the game there are 4,533 barks, 159 menus in 58 classes and 151 uses of the Avatar's
name (`getName`, joined to other strings with opcode 0x16, as in `"Oh dear, " + name + "."`).
The yes/no gump with buttons is only the engine's own "Are you sure?"; yes-or-no questions in
conversation are ordinary menus.

### What an NPC remembers

Within one conversation, the process's local variables. Between conversations, **global
flags** (section 4). In Devon's class: answering "Who are you? " sets flag 252, after which he
is greeted with "Hello, Devon. " instead; saying goodbye sets flag 257, after which he greets
the Avatar by name; and story flags (11, 54, 56, 60, 88, 90 to 93) decide which opening topics
he offers ("What is going on?", "I need the Blackrock fragment.", "The Lurker is free!").

## 4. Quest progression

### Where the state is

- **Global flags.** Usecode reads a flag with opcode 0x4e and writes one with 0x4f, giving a
  *bit* index and a width of 1 to 16 bits (`GlobalFlag::get`/`set`, `u8/flag.cpp`). There are
  8,192 bits (0x400 bytes), saved as `FLAG.DAT` with the game. A new game starts with every
  flag clear, because the starting save has no `FLAG.DAT`. The usecode accesses flags 2,471
  times, in 131 classes, as 445 distinct flags: 398 single bits, the rest counters and stages
  of 2 to 5 bits. The highest bit used is 592. Twenty flags are read but never written by any
  script, so they are always 0 *(inferred: cut content)*. The flags have no names: the debugger
  would read them from `flagsym.dat`, which did not ship.
- **Items.** A door's quality bit 0x400 is its lock (DOOR_NS 68 says "Locked Door"); frames
  record states (an item's frame is read 681 times and set 481 times in the usecode); items are
  counted where they lie.
- **NPCs.** Whether an NPC is dead (`Npc::isDead`, 240 uses) and on which map it is (`getMap`,
  105 uses), both saved in `NPCDATA.DAT` and `ITEMCACH.DAT`.
- **Running scripts**, in `KERNEL.DAT`.

### The story's shape in the flags

The flags fall into groups that follow the story *(the grouping is inferred from which classes
use them)*:

| Part of the story | Flags | Examples |
| --- | --- | --- |
| Tenebrae, Mordea, Devon and the Lurker (Hydros, Tempestry) | 54, 88 to 99, 252 to 283 | 91: the execution egg (1155:0ef5); 99: Bentic's journal (607:04d6); 88 and 95: Mordea's fall (1406); 54 (2 bits): the Lurker, 1 when the earth spell frees him (1414:0bef), 2 when Hydros is free (80:152c) |
| Necromancy (Lithos) | 44, 191 to 223 | 44: set by Lithos (433:091f); Vividos's conversation |
| Theurgy (Stratos) | 110 to 141, 422 to 458 | Stratos, Cyrrus, Stellos, Xavier, Torwin |
| Sorcery (Pyros) | 144, 146, 147, 354 to 360 | 357 (3 bits): Malchir's trial, 1 when it starts, one more for each test passed, 0 again on failure (which also sets 354 = 3, Malchir hostile); 144 (2 bits): which rival was dealt with, 1 for Vardion, 2 for Bane |
| Thaumaturgy (Mythran) | 288 to 310 | |
| Places the recall item knows | 70 to 76 | set by egg TELPFLAG (1215) on entering each place |

The endgame is not a flag. Using the tip of the Blackrock obelisk (BROCK 442, its use event)
counts the pieces of type 442 with a non-zero frame within five squares; with four, it starts
the ending spell (ETHRSPEL). Otherwise it counts the glowing pieces (type 793) on the ground and
carried, and acts when there are four. The pieces are the Titans' artifacts, told apart by
frame: Heart of Earth, Tear of the Seas, Breath (of Wind), Tongue (of Flame).

### How the story is gated

- **Usecode eggs** are invisible items (shape 73) that run their class when the Avatar steps
  into their box (`EggHatcher`, `item/egg.cpp`: the range nibbles times 32 across, within 48
  in height), and again after the Avatar has left and come back. They also get events 15 and
  16 when their area loads and unloads. Examples:
  - LOCKDOOR (1210) locks or unlocks the doors near it when its area loads, by its egg id:
    id 2 opens when flag 499 = 1 (Rhian), id 3 when 519 = 1 (Mordea and Aramina), id 5 when
    274 is 1 or 3 (Korick), id 10 locks when 144 > 0 and 146 = 1, and id 1 is a coin toss.
  - MAPTELE (1152) moves the Avatar on map 14 only once Malchir's trial is passed (357 >= 6),
    then checks 146, 147 and 236; one of its eggs needs four fire foci (397) carried before it
    sets 147. On maps 24 and 25 it chooses the destination by flag 54.
- **Teleport eggs** (shape 508) have no script: a frame-0 egg sends the Avatar to the frame-1
  egg with the same id (the quality's low byte) on the map its mapNum names
  (`TeleporterProcess`). Gated passages are therefore usecode eggs, or scripts calling
  `teleportToEgg`.
- **Conversations** offer topics by flags, and set flags by answers (section 3).
- **The recall item** (RECALL 833) offers only the places whose flags (70 to 76) are set:
  Central Tenebrae, the Plateau, the Upper Catacombs, the Hall of the Mountain King, Argentrock
  Isle, Carthax Lake and Daemon's Crag.

### Cheats

Holding Ctrl, Alt and Shift and double-clicking the paperdoll (checked against a code that
changes with the day of the week, `ContainerGump::commandMouseLeft`), or, with the same keys
held, clicking both buttons 16 times in the screen's top left corner, makes the Avatar a
cheater. Then clicking the Avatar opens a
menu in the AVATAR class (1025:00b2) with *Demo*, *Spell Book*, *Information*, *Enemy Caster*,
*Time*, *Key*, *Staff*, *Call Guards*, *Blow up!*, *Kill me*, *EndGame* and *Jump ahead*.
*Jump ahead* asks for a password ("ginaf"), then sets about 25 story flags, raises strength,
dexterity and intelligence to 21 and hit points and mana to 42, and hands over the recall item,
keys, reagents, a sword, a shield, armour and 200 coins. *Demo* sets story states directly
("Malchir Death", "Bane won", "Vardion won"). The engine's cheat window can teleport the Avatar,
and could set flags by name if `flagsym.dat` existed. It opens with F9 (key 0x143 in
`World`), though the message that announces cheating says "Cheats are F7!".

## 5. Statistics

### The numbers

An NPC is a 49-byte record (`NpcData`, `item/npc.h`; `NPCDATA.DAT` in the save):

| Offset | Field | |
| --- | --- | --- |
| 0, 1, 2 | strength, dexterity, intelligence | signed bytes |
| 3 | hit points | 0 to 255 |
| 11 | alignment | low nibble: its own; high nibble: the alignments it treats as enemies (`Npc::isEnemy`) |
| 27 | flags | invincible, ascending, descending, animation lock |
| 29 | equipment | 8 slots: 0 shield, 4 boots or legs, 5 weapon, 6 backpack (*inferred:* 1 to 3 arms, head, body) |
| 45 | mana | 16 bits |
| 47 | flags | in combat, dead, immortal, withstand death, feign death, stunned, poisoned, ... |

What the statistics do:

- **Maximum hit points are 2 x strength, maximum mana 2 x intelligence.** No maximum is stored;
  the stat bars, regeneration, potions and resting all compute it.
- **Strength:** damage (section 6); carrying, where an item can be taken only if its weight with
  its contents is at most `strength x 40` less what is already carried (`item/contain.cpp`;
  the paperdoll shows weights divided by 10, rounded up, so the limit there reads as
  `strength x 4`); throwing range
  (`64 - weight + strength`); jumping, which needs strength at least the distance in 32-unit
  squares; blocking.
- **Dexterity:** to hit and to avoid being hit; how soon the Avatar can swing again (a pause of
  `25 - dexterity` ticks after two swings when below 25); and throwing accuracy (below 30, a
  throw lands up to `(30 - dexterity) x 4` units off on each axis).
- **Intelligence:** only maximum mana, in the engine; spells spend mana.
- **Armour class** (`Npc::getArmorClass`): for the Avatar, the sum over slots 0 to 4 of each
  piece's value by its frame (`u8/armordat.cpp`) plus the weapon's armour bonus. Shield 4 or 3;
  leggings 2 or 3; helmet 4, 2, 2, 3 or 3; armour 4, 3 or 2; arm guards 2, 2, 1 or 3; magic
  armour 8 (or 6); magic shield 5; magic arm guards and leggings 4; magic helm 5; clothes 1
  (frames 0 and 1 share the first value). For other NPCs it is their monster record's.

The Avatar starts with **15 strength, 15 dexterity, 15 intelligence, 30 hit points and 30 mana**
(the starting save's `NPCDATA.DAT`).

Monsters get their numbers from a 13-byte record per monster shape (`u8/mondata.cpp`): hit
points, dexterity and damage between a minimum and a maximum, armour class, alignment, a byte
of flags (bit 0: it wanders in combat), and the damage types it deals and defends against. A
new monster rolls its hit points and dexterity within the ranges (`Npc::insertNpcData`); its
strength and intelligence are 0. Some of them:

| Monster | Hit points | Dexterity | Damage | AC | Deals | Defends against |
| --- | --- | --- | --- | --- | --- | --- |
| Troll (83) | 150-225 | 7-10 | 10-28 | 18 | blunt | pierce |
| Golem (214) | 150-175 | 7-8 | 15-25 | 18 | blunt | blade, fire, pierce |
| Demon (96) | 80-100 | 14-21 | 12-18 | 19 | normal, fire, magic | fire, pierce |
| Skeleton (119) | 35-45 | 15-20 | 7-13 | 17 | blade | undead, slayer |
| Ghost (411) | | | 0 | | (hits anywhere) | undead, magic, slayer |
| Guard (574) | 35-65 | 15-25 | 8-15 | 25 | blade | |

### Getting stronger

The Avatar improves by doing (`Avatar::accumulateStrength`, `...Dexterity`,
`...Intelligence`, `item/npc.cpp`). Each action adds points to a hidden counter for the
statistic. Each time points are added, the statistic goes up by one, and the counter returns to
0, if the counter has reached 650 or a roll of 1 in `650 - counter` succeeds, so the chance
grows as the counter fills. **Use raises no statistic above 25**, so the most hit points
earned this way are 50. The counters are saved in `AVATAR.DAT`.

| Action | Strength | Dexterity | Intelligence |
| --- | --- | --- | --- |
| a swing or kick | 1-2 | 2-3 | |
| a melee blow on an NPC | damage / 4 (before armour, hit or miss) | | |
| a blow that hits | | 2 x (defender's dexterity - the Avatar's), at least 2 | |
| climbing up h (2 to 9) | h | 2h | |
| throwing | | 1 | |
| the jumping and sinking puzzles (APPEAR, FLOATIN, FROCK, LAVASINK, SINKING, STANSTON, VANISH) | | 15 | |
| reading the log books (LOGBOOK, PLATE) | | | 100 |
| reading other books | | | 15 (BASEBOOK), 3 (GRIMOIRE) |
| casting | | | air 20, earth 5-10, fire the spell's number + 10 + 0-10 |

### Healing

- **Regeneration** (`StatRestorer`, `u8/statrest.cpp`, the Avatar only): one hit point and one
  mana point every 35 real seconds, up to the maximums. (The comment says mana comes every 20
  seconds; the counter resets at 35, so it comes once in each 35.)
- **Resting**: everything back (section 2).
- **Potions** (POTION, by frame): 1 heals in full, but at full health sets hit points to the
  strength ("Augh!"); 2 restores 5 to 8 mana; 3 heals 5 to 9; 4 hurts; 5 to 7 are protection,
  sleep and invisibility.
- **Food**: a hunger counter rises with regeneration (to 200), and eating lowers it and heals
  a little (`FeedAvatar`). The function that would weaken strength with hunger
  (`getEffectiveStr`) is never called, so **hunger has no effect**.
- **Poison** has a flag and functions, but nothing sets it: unused.

## 6. Combat

### Fighting

The Avatar enters combat with C, a right double-click on himself, or a script
(`Avatar::setInCombat`: combat music, weapon drawn, pointer changed). In combat
(`World::commandMouse...`, `u8/world.cpp`):

| Input | Move |
| --- | --- |
| left double-click, facing the target | swing the weapon |
| right double-click | kick |
| hold left, facing | block (until released) |
| hold right | advance, or retreat behind |
| hold right with the pointer far away | leave combat and run |

At most two attacks queue. NPCs go into combat when hit, from monster eggs, or by script; an
NPC's first act is its class's cast event, which casts spells for the mages and calls the
guards for the others ("Guardsmen, to arms!").

A swing hits what is in reach on the animation frames that have an attack range: the attacker's
box pushed `range x 32` units forward, first NPC found, else first item
(`AnimPrimitive::weaponCheck`, `u8/animprim.cpp`). A monster whose damage type has bit 0x200
hits wherever it is.

### Damage

`Npc::getDamageAmount` (`item/npc.cpp`):

| Attacker | Damage |
| --- | --- |
| the Avatar, armed | `strength / 5 + weapon base + random(0 .. weapon modifier)` |
| the Avatar, unarmed | `1 + random(0 .. strength / 2)` |
| the Avatar, kicking | `4 + random(0 .. strength / 2)`, +3 with magic boots (845) |
| a monster | `random(minimum .. maximum)` from its record |
| another NPC | 1 |

The weapons (`u8/wpncache.cpp`; names from their usecode classes):

| Weapon (shape) | Base | Modifier | Attack dex | Defence dex | AC | Damage types |
| --- | --- | --- | --- | --- | --- | --- |
| Hammer (540) | 4 | 9 | | | | blunt |
| Scimitar (541) | 5 | 12 | | | | blade |
| Sabre (542) | 5 | 12 | | | | blade |
| Sword (420) | 5 | 13 | | | | blade |
| Axe (419) | 6 | 14 | | | | blade, blunt |
| Dagger (418) | 3 | 7 | | | | blade |
| Mace (417) | 4 | 11 | | | | blunt |
| Hammer of Strength (815) | 11 | 9 | | | | blunt, magic |
| Deceiver (822) | 8 | 14 | +2 | +2 | +2 | blade, blunt, magic |
| Slayer (816) | 4 | 11 | | | | blunt, magic, slayer |
| SCIMOKG (821) | 5 | 12 | +1 | | | blade, undead, magic |
| Korghin's Fang (820) | 7 | 7 | +4 | | +1 | blade, magic |
| Bladestrike (819) | 5 | 13 | +7 | | | blade, magic |
| Protector (818) | 5 | 13 | | +4 | +5 | blade, magic |
| Flamesting (817) | 5 | 12 | +1 | | | blade, fire, magic |

So a strength-15 Avatar with a sword does 8 to 21 a blow.

Damage types are bits (`item/item.cpp`): normal 1, blade 2, blunt 4, fire 8, undead 0x10,
magic 0x20, slayer 0x40, pierce 0x80, falling 0x100. The Avatar's defence is fire when he
carries the magic shield, and nothing otherwise; a monster's comes from its record.

### A blow, step by step

`Item::receiveHit` (`item/item.cpp`):

1. **Immunity.** The blow's types less the defender's defences; undead, magic and pierce always
   stay. If the defender defends against magic and the blow is not magic, no damage (only magic
   weapons hurt ghosts). If no type is left, no damage: the magic shield makes plain fire
   harmless.
2. **Tough hides.** A defender that defends against pierce takes half from a blow that is not
   blade, fire or pierce.
3. **Slayer.** One blow in ten does 255.
4. **Undead.** The code means to double damage when an undead-slaying blow hits an undead
   defender, but the line is `damage * 2;`, which does nothing. It is so in the original source
   (`source/ITEM/ITEM.C`, line 1700) and in the shipped game.
5. **Armour** (not for pierce, nor for a slayer's 255): `ac = armour class x 3`; a defender
   blocking (and not stunned) takes `strength / 5` less first; fire halves `ac`, and being
   stunned halves it again; `ac` is at most 100, and the damage is scaled by `(100 - ac)%`,
   rounding a remainder up at random.
6. **To hit** (when damage is left, the blow is normal, blade or blunt, not pierce, and the
   attacker is an NPC): attack = dexterity + the weapon's attack bonus; defence = dexterity
   + the weapon's defence bonus, + dexterity / 5 when blocking. The blow lands if the defender
   is stunned or `random(0 .. attack - 1) + 3 > random(0 .. defence - 1)`; a monster that defends
   against pierce also lands one miss in four. *(Inferred:)* with dexterity 15 on both sides
   about 65% of blows land, and blocking brings that down. A miss does nothing.
7. **Hit points** go down unless the defender is immortal or invincible. With *withstand
   death*, a blow that would kill restores hit points to 2 x strength instead, once.

Then: a kick stuns (always, or one time in three against pierce defenders) for
`damage x 8 + 22` ticks; trolls, kith, golems, demons and torax are not interrupted by blows
(except one in ten, and kicks); and an NPC hit while not fighting turns on the attacker and calls
for help, bringing NPCs of its alignment within 0x400 into the fight (peasants and children
never come; guards bring only guards).

**Falling** (`GravityTracker`, `u8/gravity.cpp`): a fall of `h = height / 8`; 13 or more kills,
10 to 12 does `(h - 9) x 2`, less does nothing; the damage is pierce, so armour and to-hit do
not apply. Running into things does 1.

**Spells** call `receiveHit` with their own damage and types and go through the same steps:
the fire missile does 4 to 11 fire, a fire field 5 to 15, lava the victim's whole hit points
(pierce, magic and fire). Grant Peace (`u8/grantpea.cpp`) kills every undead of the target's
kind within 0x300, or a living target one time in ten.

### Death

At 0 hit points (`item/item.cpp`): the dead flag, the death animation, the remains and treasure
the type leaves (`deathAnimCall`), victory music. Skeletons not killed by fire stand up again
after 2 to 25 seconds with new numbers. When the Avatar dies, the game stops, and after 12
seconds shows the gravestone, waits for a key and fades to the main menu (`AvatarDeathWatch`).

### Monsters' tactics

`CombatBrain` (`u8/combatbr.cpp`): with a target, a monster that wanders does so one time in
ten; otherwise it tries a swing, and if the swing would connect, faces the target, swings and
waits `(25 - dexterity) x 3` ticks; if not, it walks to the target (or wanders if the target
is invisible, unless it is a ghost). Without a target it picks the nearest enemy within 600,
or loiters. Changelings and ghosts have their own rules.

## Odd things worth knowing

- The undead bonus does nothing (combat step 4).
- Hunger and poison are implemented and never used.
- Twenty global flags are read and never written.
- Event 3 (setActivity) is sent and never handled.
- Mana comes back every 35 seconds, not every 20 as the code's comment says.
- `Scheduler::process` goes on after ending at NPC 256 and builds an NPC 256 it never uses
  (also in the original, `source/ITEM/ITEMCACH.C`).
