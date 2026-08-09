# Cursed — walkthrough

- **Engine:** ADRIFT 4.0. *Cursed*, by **Nick Rogers** (IFComp 2011, placed
  equal 13th; the wired build is the post-comp **2.1.10**, 1 Sep 2012).
- **Game file:** `cursed.taf` (symlinked into `games/`).
- **Solution:** `goldens/cursed_solution.txt` — 298 commands.
- **Result:** ★ **WON**, **93 / 101**, `The End` on the Battlements.
- **Row:**
  `cursed_solution.txt|cursed.taf|The honour will be all mine, father|SCR_SKIP_WAITKEY=1`
- **Row env:** **`SCR_SKIP_WAITKEY=1`** — the game paginates every chapter
  break and most cut-scenes with `[ Press any key ]`; without it one solution
  line stops being one command and the whole script desyncs.

Derived from the ClubFloyd transcript of 31 Jan / 7 & 14 Feb 2013
(AllThingsJacq.com, *NightFloyd*), which also finishes on **93/101** — see
[The missing 8 points](#the-missing-8-points).

Torrin, ward of King Rithusar of Rylane, is convicted of murdering Prince
Alsanter and is "mercifully" cursed by the court wizard Rixomas into the shape
of an animal instead of being executed. You pick the animal in the opening
scene; this route takes the **fox** (`fox` at the choice prompt), which is the
path the ClubFloyd session and the game's own hint file follow.

## Route

Prologue (three vignettes) → Part One (escape castle and city) → Interlude
(you play *Rixomas* talking with the king) → Part Two (cross-country as a fox:
barn, mill, mountain) → Interlude (the lair) → Part Three (back into the
castle) → Epilogue (you play *King Rithusar*).

Score checkpoints along the committed script:
3 → 8 → 10 → 20 → 24 → 27 → 28 → 33 → 34 → 35 → 40 → 55 → 58 → 63 → 73 → 83 →
**93**.

### Part One — Mistake in Identity (20 pts)

`fox`, then the meeting hall: `push pedestal`, `pull curtain`,
`put curtain in fire`, `hide behind curtain` draws the guards away (+3). Out
through the stable: `roll barrel`, `pull rope` (+5 escaping the castle),
`hide under cart` / `follow cart`. In the city, `open latch` frees the sheep
(+2), the rake must be dragged five times (`drag rake se` ×5), the sack ripped
six times, then `open gate` and `dig under scaffolding` gets you out (+10).

### Interlude — Conversations with the King (4 pts)

You are Rixomas. The whole interlude is a yes/no interrogation; the committed
answers are six `yes` then twelve `no`, which is what scores the 4 points.

### Part Two — Lost in Transformation (31 pts)

- Waking as a fox: `think`, `feel`, `feel feet`, `feel ground`, `feel grass`,
  `smell`, `listen`, `l` — the discovery sequence is scored as a block.
- Avoiding the hunting party (+3): `swim`, then `creep over stones`, `shake`,
  `wag tail`.
- **The barn** (+1 trap, +5 warrior): `chew rope`, `dig under door`,
  `chew bale`, `pull rope`, `pull plank to hay`, `pull pick to plank`. Then
  lure the warrior with `bark` from the right rooms.
- **The mill** (+1 door, +1 tray, +5 warrior): `push wheelbarrow` before going
  `sw`, then `bark` (the horse bolts and the barrel smashes the door).
  **`x stream` is required** before `open gate` — the water gate does not
  exist as an object until you have examined the stream. Then `pull lever`,
  `climb wheel`, `jump on tray` ×3, `bark`, `chew rope`.
- **The mountain** (+2 vixen, +1+1+1 traps, +10 Limos): four `w` moves resist
  the vixen; `search bushes`, then **`smell` before `x ground`** (the rotting
  smell is what unlocks the second trap — `x ground` alone just re-prints the
  scenery), then `search rocks`; `n`, `lie down` finds Master Limos.
- Limos will not hand over the potion until his topic list is exhausted.
  Eleven `ask limos about …` in the committed order; the last two topics
  ("Ralyon warriors", "Potion") only appear after the first nine.

### Interlude — Lair (3 pts)

`drink potion`, `think invisible` / `think visible` to learn the ability,
`search garbage`, `x floor`, **`open trapdoor`** (not "open secret trapdoor" —
that resolves to the metal plate, which "can't be opened"), `down`, `e`,
`drop all`, `s` (+1 magical entrance). `take papers` (+1), `take seal`,
`x window` (+1 identifying the location).

### Part Three — The Plan for Salvation (25 pts)

- The gardens: `dig material` (+1 sack), drag the sack into the shed, push the
  spade onto it, drag it back out, `pry door with spade` (+1).
- The kitchen dog (+3): `push broom` (knocks the spice rack down),
  `blow spices into fireplace`, `invisible`, `w`. Invisibility alone is not
  enough — the dog tracks by scent.
- **Recruiting Lord Vonisor.** The upstairs south corridor runs, west to east:
  *End of the corridor · Reken · Edukam · Vonisor · Gaxin · Hallyn · Sulanar ·
  Adath · Rixomas*, with the Landing north of the Rixomas end. Vonisor's door
  is the only one a fox can open (`open door` five rooms west of Rixomas').
  After that he follows you and every `open door` / `open curtains` you type
  is redirected to him.
- Alsanter's rooms (north wing, past the Landing) for the cup and the
  bloodstain, then Princess Tevona's rooms: `smell`, `x buttress` (this is
  what reveals the indentation), `push indentation` — Vonisor opens the secret
  poison cabinet and the confrontation begins.

### The confrontation (+10 Vetan, +10 Rixomas)

Timing here is exact and has two independent requirements:

1. **`wink at vonisor` must have been used at least once** before the attack.
   Without it Vonisor "reacts somewhat belatedly", his thrust misses, and
   Vetan throws you out of the window. With it he "stabs Vetan through the
   chest" and you score.
2. **`attack vetan` only works on the turn after Vetan stops watching you.**
   The scene plays out as a chain of scripted beats, one per *turn*; the beat
   you are waiting for ends `You sense that Vetan's attention has left you
   completely.` The next turn must be `invisible`, and the turn after that
   `attack vetan`. One turn earlier the attack draws blood but fails; one turn
   later Vetan kills you; waiting a turn *while invisible* also kills you
   ("Ha!" Vetan yells).

The committed script is `wink at vonisor`, `z`, `z`, `z`, `listen`,
`invisible`, `attack vetan` — see [`z` is three turns](#z-is-three-turns) for
why that adds up to eleven turns and not five.

Then `z` (Vonisor leaves to cure Rixomas) and `s e e s s` back to the corridor
outside Rixomas' rooms, which scores the second +10 and switches you to King
Rithusar for the Epilogue.

### Epilogue — the Battlements (+10)

`up`, then the nine topics in order: `himself`, `tholin`, `god`, `journey`,
`tevona`, `curse`, `rixomas`, `limos`, `vetan`. `topics` then reports the list
exhausted. **Six more turns must pass** (the committed `z`, `z`) before Torrin
"turns and leans against the battlement" and the game says he *is waiting for
something more from you, something more than words*. `hug torrin` on the next
turn ends the game at 93. Hug too early and you get "you decide to wait a bit
more"; wait too long and Torrin asks to be excused, the game ends anyway, and
you finish on 83.

## Derivation notes

### `z` is three turns

`Globals.WaitTurns` in this game is **3**, so a single `z` runs three turns of
events. That is invisible in normal play but fatal when you are counting
scripted beats: three `z` commands consume nine turns, and the cut-scene fires
three beats per prompt. Confirmed with `SCR_TRACE_EVENTS=1`:

```
$ printf 'z\nquit\ny\n'      | ./scare cursed.taf 2>&1 >/dev/null | grep -c '^Event: ticking event 0:'
3
$ printf 'listen\nquit\ny\n' | ./scare cursed.taf 2>&1 >/dev/null | grep -c '^Event: ticking event 0:'
1
```

Use a one-turn command (`listen` is harmless everywhere in this game) whenever
a scene has to be stepped beat by beat. The committed script mixes the two:
`z z z listen` is exactly the ten turns that put `invisible` on turn 11 of the
Tevona confrontation.

The 2013 ClubFloyd transcript reaches the same beats on different turn
boundaries — its players were typing single-turn commands (`x`, `smell`,
`wink`) between the `z`s. The beat *order* is identical; only the grouping per
prompt differs.

### Worn clothing is not "held" — engine fix in `screstrs.cpp`

The second interlude gates the magical veil on a task whose restriction is
*"**No object** is held by the player"* (task 1231, `RESTR type=0 v1=0 v2=1
v3=0`), while the player is wearing street clothes that cannot be removed
(`OBJNAME obj=568 [clothes]`). Scarier counted a worn object as held, so `s`
through the veil failed no matter what you dropped and the game was
unwinnable.

`run400.exe`'s restriction handler distinguishes the two cases. At `00080871`
in `mdlSpreadTheLoad.Sub_20_3` — the **quantified** (`Any object` / `No
object`) loop — the `Var3 = 0` arm tests only `location == 0` (held) and, for
the container arm, `location == 246` with the parent held (`0`) or worn
(`156`). There is no `location == 156` test on the object itself. The
**single-object** path at `00080C9B` plainly has one.

`restr_object_in_place()` now takes a `quantified` flag and returns worn-by-
player as a match only on the single-object path:

```c
position = gs_object_position (game, object);
if (position == OBJ_HELD_PLAYER)
  return TRUE;
if (position == OBJ_WORN_PLAYER)
  return !quantified;
```

Full v4 corpus after the change: **128/128 PASS**.

### Unresolved `[key=value]` tokens

The transcript prints raw tokens in a few places —
`[playermove=Rithusar]` on the move into the Rixomas corridor,
`[ridingpaddlewheeldescription=0]` in the mill, `[listen-Dead]` and
`[listen-Complete]` at the endings. These are unresolved ALR-style
substitutions and they are **in the golden on purpose**: the 2013 SCARE
1.03.10 session prints exactly the same tokens in exactly the same places, so
they are pre-existing SCARE behaviour rather than a scarier regression. Fixing
the substitution model is tracked separately; when it lands, this golden will
need re-blessing.

### The missing 8 points

`full score` at the end lists 24 scoring items totalling 93 of the 101
available. The unaccounted 8 are not in the ClubFloyd transcript either — that
session played all three chapters over three evenings, tried the alternative
animals, and still finished on 93. The most likely home for them is the King
interlude ("4 points for your responses to the king's questions" reads like a
partial award), but nothing in the transcript or the shipped `cursed_hints.taf`
confirms a better answer set, and re-deriving that interlude would desync every
later beat in the script. 93/101 is the documented result.

### Other footguns

- `open gate` at the mill fails with "You don't see a gate here at the moment"
  until `x stream` has run.
- `x ground` on the mountain plateau needs `smell` first.
- In the warehouse, `open secret trapdoor` matches the metal plate and fails;
  `open trapdoor` works.
- The Village Inn is unenterable while the roaming warrior is in the way — the
  committed route sidesteps it entirely (`d`, `out`, `n`, `w`,
  `push wheelbarrow`, `sw`).
- The game ships its own `cursed_hints.taf`; it is a separate ADRIFT game and
  is not wired into the corpus.
