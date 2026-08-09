# Topaz — walkthrough — ★ WON (banked, golden)

- **Engine:** ADRIFT 4 (native). Woodfish, ADRIFT 4th 1-Hour Comp 2004. Dig up
  the sentient sword Topaz, reach the skeleton of its old master Paragon, take
  the silver ring and wear it.
- **Status:** **WON and in the regression** — `goldens/topaz_solution.txt` is a
  MAP row in `run_v4_walkthroughs.sh` with a committed golden (win marker
  `The two of you set out into the forest.`). The v4 suite is 23/23 PASS.
- **Walkthrough source:** *Key & Compass* (native-ADRIFT — it wins on the real
  ADRIFT Runner, and now on SCARE too).
- **Which build:** the 4th 1-Hour Comp release, md5
  `7d4beb159bf3876f761bbac911395d05`, 4839 bytes. Three releases circulate under
  this filename and **the other two are unwinnable game files** — see the
  2026-08-01 section below before reporting any Topaz divergence.

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

`goldens/topaz_solution.txt` (23 commands):

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

## SOLVED: the "Choose an option to speak" lock-up is a different release (2026-08-01)

Long-running open question: on a real ADRIFT 4 Runner session the game got stuck
in the skeleton room — `x skeleton`, and every command after it, answered
"Choose an option to speak." SCARE walked straight past. Months of P-code
auditing, a live Wine run of the genuine `run400.exe`, and a Swedish-locale
re-run all failed to reproduce it, because **we were running a different build
of Topaz than Petter was.**

A `.tas` save taken at the stuck state settled it in one line. SCARE refused to
restore it, and the reason was the header count check:

```
save:  rooms=8  objects=11  tasks=26  events=4  npcs=0
game:  rooms=8  objects=9   tasks=23  events=4  npcs=0
```

Two extra objects and three extra tasks. There are two Topaz releases in
circulation. **Three, in fact** — a second stuck save Petter supplied matched
neither of the first two (8 rooms / 9 objects / **24** tasks) and turned up a
third build. All are called `topaz.taf` and all carry GameName `Topaz<cls>`:

| # | release | size | md5 | rooms/objs/tasks | winnable |
|---|---|---|---|---|---|
| 1 | 4th 1-Hour Comp (**our corpus copy**) | 4839 | `7d4beb159bf3876f761bbac911395d05` | 8 / 9 / 23 | **yes** |
| 2 | revision (`ifarchive_v4_new/topaz__2.taf`) | 4866 | `5f91c9cd4391b6e44c2c052698d01118` | 8 / 9 / 24 | no |
| 3 | IF Archive (`ifarchive_v4_new/topaz.taf`) | 5980 | `78c4966d7380e6fed8ece1e6b73db4a1` | 8 / 11 / 26 | no |

(The `__2` suffix is a download-time dedup artifact, not part of the name.)

Build 2 is build 1 plus exactly one task — the guard described below. Build 3
adds two more objects and two more tasks on top (an extra examine task for the
bird carving, and `about`), and keeps the guard unchanged. So the author
introduced the bug in the first revision and never caught it.

Restoring each save against its own build reproduces Petter's sessions
**exactly**, and so does replaying `topaz_solution.txt` against either: the
route works normally up to `n` into the skeleton room, and from there
`x skeleton` and every subsequent command reply "Choose an option to speak."

### The bug is in the game, not in either interpreter

Both post-comp builds add the same conversation-menu guard (index 17 in build
2, 18 in build 3):

```
TASK 18 where=2 room=-1 restr=0 rep=1 score=0 cmd=[*]
    WHERE_ROOMS=[5 6 ]
    CompleteText: <i><c>Choose an option to speak.</c></i>
```

A wildcard `*` command, no restrictions, repeatable — it swallows any input in
the rooms it covers. It is meant to cover the two fake "menu" rooms, which are
rooms **4 and 5**. It covers **5 and 6** instead. Room 6 is the skeleton room,
so the guard is scoped one room too far.

ADRIFT runs tasks in index order and the first runnable match wins, so the
guard precedes every task that matters in room 6 (build 3 numbering):

```
TASK 18  cmd=[*]                      rooms 5,6   <- always matches
TASK 19  take the silver ring         room 6
TASK 20  take the silver ring         room 6
TASK 21  talk to the sword/topaz      room 6
TASK 24  wear the silver ring         room 6      <- the win task
```

So **both post-comp releases of Topaz are unwinnable**: once you go north into
the skeleton room there is no way back out and no way forward. Only tasks with
a lower index can still fire there — in practice just task 6, the catch-all
movement task, which answers "You stumble around in the darkness." Verified by
trying `1`, `2`, `3`, `talk to topaz`, `take silver ring`, `wear ring`, `n`,
`s`, `ask topaz about ring`, `about`, `x ring` and a blank line from the
restored save: every one is eaten. Replaying the 23-command winning route
against build 2 yields nine "Choose an option to speak." and never reaches the
win marker.

The comp build has no `*` task at all — "Choose an option to speak" occurs once
in its inflated taf (task 10's CompleteText) versus twice in the post-comp
builds. That is why the Key & Compass solution documents the whole game:
Welbourn played the comp build, which is the one in our corpus and the one the
golden is recorded against.

### What this retires

- **SCARE is faithful on all three builds.** It wins the comp build and
  reproduces both post-comp builds' lock-up at exactly the command Petter
  named, from a save each time. No engine change is warranted.
- The earlier run400.exe P-code audit was correct in every particular (1-based
  room storage in the move-player action, 1-based task restrictions, 1-based
  event starter tasks) — it just answered a question about the wrong file.
- Everything previously suspected and ruled out stays ruled out and is now
  moot: Auto complete (Petter reproduces with it off), a different Runner build
  (md5 `f7077dddb00b2d1623857ab9b4d1fbc8`, Release 52), Swedish system locale
  (re-run in an `sv_SE.UTF-8` prefix with "Ja"/"Nej" message boxes; run400 has
  no `Like`/`CInt`/`CDbl` and parses with locale-independent `Val`), and the
  MORE pagination prompt eating the first keystroke of the next command (a real
  scripting footgun, but it yields a parser error, not this).

### Reproducing

```sh
printf 'restore\ny\n/path/to/topaz.tas\nx skeleton\nquit\ny\n' \
  | harness/scare '/path/to/ifarchive/topaz.taf'
```

A count mismatch on restore is the cheapest possible "wrong game file"
detector — worth reaching for first the next time a save won't load.

## Save round-trip verified against the real Runner (2026-08-01)

Both directions of `.tas` interop are now confirmed against a live `run400.exe`
(comp build, under Wine — see `~/adrift-battle/runner/wine/README.md`):

- **Runner → SCARE** — Petter's two stuck saves restore in SCARE and reproduce
  his sessions exactly (that is what cracked the mystery above).
- **SCARE → Runner** — a save SCARE wrote after the first 16 commands of
  `topaz_solution.txt` was loaded in the Runner with `restore`
  ("Loading game... done.", status bar `scare_made.tas`), came back in the right
  room with the right inventory, and **played on to the win** from there —
  `The two of you set out into the forest.`, status bar `Congratulations!`.
  So `ser_save_game`'s Runner-format output is not merely parseable, it is
  semantically correct.

Structurally the two saves of the same state are **157 records vs 157**, every
field in the same slot. The only format-relevant difference is record 8, the
player name: the Runner writes `Anonymous`, SCARE writes `''` (SCARE emits
`Globals.PlayerName` verbatim, and Topaz leaves it blank). The Runner accepts
the empty string without complaint. (Records 154/155 — elapsed seconds and turn
count — differ for benign harness reasons.)

### `g` is *again* — and Auto complete can fake a parser divergence

Step 22 of the route is `g`. Driving it by hand on the Runner first looked like
a standard-library divergence: `g` echoed as `get` and answered "Take what?",
suggesting SCARE's `{"[again/g]", lib_cmd_again}` (`scrunner.cpp:355`) was
wrong. It is not. **Options → Auto complete was still on**, and it rewrites the
input box *before* the command is echoed — so `g` was literally replaced with
`get` and the transcript showed a verb the player never typed.

With Auto complete off, the Runner agrees with SCARE:

```
x fields
The bare fields stretch far into the distance.

g
(x fields)
The bare fields stretch far into the distance.
```

The `(command)` line is the Runner echoing what it is repeating; SCARE prints
no such line, which is the only difference here. One genuine edge-case
divergence did fall out: the Runner stores the raw previous input *including*
`g`/`again`, so `x fields` / `g` / `again` replays the literal string `g` and
answers "I don't understand what you mean!". SCARE deliberately skips storing a
repeat command as the prior element (`scrunner.cpp:1575`), so its second repeat
still re-runs `x fields`. Nothing in the corpus depends on it.

So Auto complete is not just an input mangler — it can manufacture a
convincing-looking parser difference. Turn it off before reading anything into
a Runner transcript. (Related trap: sending a bare space to dismiss a MORE bar
will *activate the highlighted menu item* if a menu is open, which is how it
got switched back on mid-test.)

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
