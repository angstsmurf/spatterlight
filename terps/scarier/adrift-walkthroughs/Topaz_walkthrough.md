# Topaz — walkthrough — ★ WON (banked, golden)

- **Engine:** ADRIFT 4 (native). Woodfish, ADRIFT 4th 1-Hour Comp 2004. Dig up
  the sentient sword Topaz, reach the skeleton of its old master Paragon, take
  the silver ring and wear it.
- **Status:** **WON and in the regression** — `harness/topaz_solution.txt` is a
  MAP row in `run_v4_walkthroughs.sh` with a committed golden (win marker
  `The two of you set out into the forest.`). The v4 suite is 23/23 PASS.
- **Walkthrough source:** *Key & Compass* (native-ADRIFT — it wins on the real
  ADRIFT Runner, and now on SCARE too).

## It was a SCARE bug, not an unwinnable game (resolved 2026-07-13)

This game was previously written off as "unwinnable under SCARE". That diagnosis
was wrong in every particular, and the real cause was an engine bug that affected
**every** ADRIFT 4 game, not just this one.

The win task is `TASK 22 = [wear/put on/try on]{the}{silver}[ring]`, whose single
restriction is an object-location restriction `(var1=0, var2=0, var3=0)` — "**no**
object is hidden". SCARE evaluated that by looping over *all* objects and asking
each whether its `position` field equals `OBJ_HIDDEN` (-1).

But a **static** object has no position: SCARE leaves its `position` at -1 (which
is also the "hidden" sentinel) and keeps its real whereabouts in an authored
room-list. So every unmoved piece of scenery — fields, mud, hedges, sky — read
back as "hidden", and a "no object is hidden" restriction could never pass **in
any game that has scenery**. Topaz is simply the game that happened to depend on
one.

**Ground truth (run400.exe P-code disassembly, `mdlSpreadTheLoad.Sub_20_3`):** the
real Runner opens its object loop with an explicit static filter —

```
000807B5: MemLdUI1 [18]      ' Objects(i).Static
000807BB: LitI2_Byte 0
000807BD: EqI2
000807BE: BranchF 00080B3C   ' -> Next i
```

— so "any object" / "no object" range over **dynamic objects only**. The Runner
does not even keep a location field for statics (`[1A]` is dynamic-only; statics
use the per-room byte array at `[1C]`). Everything else in SCARE's decoding of
this restriction is correct: `var1=0` really is "no object", `var3=0` really is
"hidden", and rooms really are 1-based.

**Fix:** `screstrs.cpp`, `restr_pass_task_object_location()` now skips static
objects in the any/no-object loop, exactly as the Runner does. All 22 pre-existing
v4 goldens are byte-identical after the change; Topaz becomes the 23rd.

### Two claims in the old diagnosis that were simply false

- *"EVENT 3 never fires."* It fires every time. Its starter is stored as
  `TaskNum=19`, which is **1-based** — the engine resolves it to task index **18**,
  `take the silver ring`, not task 19 (`talk to topaz`). It fires the instant you
  take the ring.
- *"A hidden duplicate ring is never un-hidden."* The dynamic ring (obj 8) **is**
  un-hidden: `TASK 18`'s action moves it to the player. What EVENT 3 then does is
  **hide the static ring** (obj 7) that was on the skeleton's finger — correct
  behaviour, since the skeleton crumbles to dust as you take the real ring. Under
  the old (buggy) reading that hidden static then blocked the win forever, which
  is what made the game look unwinnable.

## Route

`harness/topaz_solution.txt` (23 commands):

```
x mud / x bushes / x fields / n        -- Muddy Path
x clouds / x gold / take gold          -- dig the glinting object out of the mud
z / z / z                              -- Topaz cleans up; the sword speaks
x glow / w                             -- follow the glow into the dark
x sword / take sword                   -- Topaz wakes; conversation menu opens
1 / 1                                  -- two menu answers (rooms 4 and 5 are fake
                                          "menu" rooms; the author built the menu
                                          out of tasks whose command is "1"/"2")
n                                      -- to the skeleton of Paragon
x skeleton / x silver ring
talk to topaz                          -- Topaz recognises his old master
take silver ring / g                   -- the skeleton crumbles; you hold the ring
wear ring                              -- WIN
```

## How it was diagnosed (reusable)

Enabled by fixing the harness build's dump macro (see below). With the
dump-capable `scare`:

```sh
printf 'look\nquit\ny\n' | SCR_DUMP_TASKS=1 ./scare games/topaz.taf 2>tasks.txt   # dump every task's cmd+restrictions+events
{ cat path.txt; echo 'wear silver ring'; } | SCR_TRACE_TASKS=1 ./scare games/topaz.taf 2>trace.txt  # per-restriction PASS/FAIL
printf 'look\nquit\ny\n' | SCR_DUMP_OBJLOC=1 ./scare games/topaz.taf 2>obj.txt     # initial object positions (spot the hidden obj 8)
```

**Build fix that unlocked this:** `harness/build.sh` was defining
`-DSCARE_DUMP_TOOLS`, but after the `scare`→`scarier` rename the instrumentation
guard is `SCARIER_DUMP_TOOLS` — so the dump/trace tools had been silently
compiled out (dead code). `build.sh` now defines `-DSCARIER_DUMP_TOOLS`; the
tools are env-var-gated, so the binary is behaviourally identical with the vars
unset (all 17 goldens still pass) but `SCR_DUMP_TASKS` / `SCR_TRACE_TASKS` /
`SCR_DUMP_OBJLOC` now work for route debugging.

## The real-Runner menu divergence — closed (2026-07-31)

An open question from the Plover TODO: on a real ADRIFT 4 Runner session the
game reportedly got stuck after the two numbered menu answers — every later
command replied "Choose an option to speak" — while SCARE exits the menu after
exactly two answers. The suspicion was an off-by-one in the Runner's
move-player destination. A run400.exe P-code audit refutes that:

- The task **action executor** is `mdlSpreadTheLoad.Sub_20_11` (`Sub_20_33` is
  the event engine; the turn loop is inline in `Form1.Text1_KeyPress`). Its
  move-player "to room" case (action type 1, Var2=0, @ `0008CA07`) stores
  `Var3 + 1` into the Runner's **1-based** room slot — semantically identical
  to SCARE's 0-based `gs_move_player_to_room (game, var3)`.
- Type-2 (task) restrictions (`Sub_20_3` @ `000810DE`) evaluate
  `Tasks(Var1-1).completed == 1 - Var2`: 1-based task refs, "must (not) be
  done" — identical to SCARE's decode of task 5's gate on task 16.
- Event starter task refs are 1-based, set-task actions (type 5) are 0-based —
  both matching SCARE.
- "Choose an option to speak" occurs exactly **once** in the (inflated) taf, in
  task 10's take-sword CompleteText. The menu rooms 4/5 have empty
  descriptions and no alternates, so no data path lets the Runner repeat that
  line.

So by its own disassembly the real Runner walks the same menu path SCARE does
(`1` → task 14 → room 5; `1`/`2` → task 16 → room 3; `n` → task 5, gated on
task 16 done → skeleton). The stuck session remains unexplained but is not the
Runner's canonical behavior — the Key & Compass solution is native-ADRIFT and
documents the whole game past the menu. No SCARE change needed.

## Confirmed empirically on the real Runner (2026-08-01)

The static argument above is now backed by an actual run. Petter's refined
report was that the break is at **`x skeleton`** — that command and every one
after it answering "Choose an option to speak" — i.e. *after* both menu answers
and the room move had already worked.

**It does not reproduce.** run400.exe was run under Wine on this M1 (see
`~/adrift-battle/runner/wine/README.md` for the harness) with the authentic
ADRIFT 4.0 runtime from ifarchive's `ADRIFT40.zip`, whose `run400.exe` is
byte-identical to our copy (MD5 `f7077dddb00b2d1623857ab9b4d1fbc8`) — so this is
the canonical build, not a variant. The full 23-command route was typed in and
the game **won**:

- `take sword` → menu opens; `1` → Topaz's introduction + level-2 menu;
  `1` → "Hmmm. How odd. You will explore now, mortal." — menu exits cleanly
  after exactly two answers, matching SCARE.
- `n` → skeleton room.
- **`x skeleton` → "The skeleton is stretched out upon the ground, his bones
  corroded by age, and smothered with dust. You notice a silver ring on one
  skeletal finger."** No "Choose an option to speak".
- `x silver ring` / `talk to topaz` / `take silver ring` ×2 / `wear ring` →
  "Forest Clearing", "The two of you set out into the forest.",
  "[Press any key to end]", status bar **"Congratulations!"**.

### What has been ruled out

- **Auto complete.** It is on by default and does demonstrably corrupt input (in
  one session it turned `take sword` into `take swordrd`, "Take what?"), but
  Petter reproduces the break on Windows 10 with Auto complete *off*, so this is
  not the cause.
- **A different Runner build.** MD5 as above; banner reads "Version 4.00 / ©
  Campbell Wild 1998-2012 / Last build: 6th September 2012 (Release 52)".
- **Swedish system locale.** Re-run in a `sv_SE.UTF-8` Wine prefix, with the
  locale verified live (the Runner's message boxes came up **"Ja" / "Nej"**).
  The route still won. This matches static analysis of the P32Dasm listing:
  run400 contains no `Like`, `CInt` or `CDbl`, and parses numbers with `Val`,
  which is locale-independent by definition.

### Still open: the **MORE** pagination prompt

The one input-eating mechanism that *did* reproduce here. At the small default
window size the Runner paginates constantly, and the response to `n` (into the
skeleton room) ends on a MORE bar — so the first keystroke of the next command
is consumed dismissing it and `x skeleton` arrives as ` skeleton` → "I don't
understand what you want me to do with the skeleton." This happened on both
replays, at exactly the command Petter named. It is environment-dependent
(window size, font size, Verbose setting), which would explain why it shows on
one machine and not another. It has *not* been confirmed as Petter's mechanism —
his symptom is a repeated "Choose an option to speak", not a parser error.

The data argument still stands regardless: "Choose an option to speak" occurs
exactly once in the inflated taf (task 10's CompleteText), task 10 is
non-repeatable and in room 3, and menu rooms 4/5 have empty descriptions. No
legitimate data path re-emits it, so SCARE is faithful here.

## Fixed: the Webdings dove on the title screen (2026-08-01)

A genuine, separate incompatibility, and this one *is* ours. Topaz draws a dove
under its title with

```
<font face="Webdings" size=72>\xFF</font>
```

Webdings is a symbol font, so `\xFF` is a pictogram, not a letter. Scarier
ignored `face=` (it only ever checked for Courier/Terminal to pick a monospaced
style) and printed the raw byte, which came out as a stray `ÿ`.

`os_glk.cpp` now tracks symbol faces on the font stack and translates their
content to the Unicode equivalents, so `\xFF` prints as **U+1F54A DOVE OF
PEACE** 🕊. The character-to-pictogram assignments were read off the glyph names
in the `post` table of the shipped Webdings font rather than guessed — entry
`0xFF` is the glyph literally named `peace`.

Scope check: Webdings is used by exactly one game in the 81-file v4 corpus
(this one); the other `face=` values in the corpus are ordinary text fonts.
(29 corpus files are ADRIFT 3.9-obfuscated and were not scanned.) The ANSI
walkthrough harness is deliberately left alone — it is a byte-oriented
transcript tool, not a display port, so `topaz_solution.expected.txt` still
holds the raw `0xFF` and the regression row still passes.
