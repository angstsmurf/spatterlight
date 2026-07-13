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
