#!/usr/bin/env python3
"""The `with ` history prepend, and what "With what?" leaves behind.

The last open arm of the checkwild/textsrc lead in WINE-TRANSCRIPTS-TODO.md.
p*TEXTSRC (2026-09-20) fed `frob rock` / `with zzz` / `wibb gem` /
`with rock` and got three cells where 3.90 answers unlike every other
Runner:

    | | 3.70 | 3.80 | 3.90 | 4.00 |
    | `with zzz` | "I don't understand." | WITH1 | "With what?"           |
    | `wibb gem` | catch-all gem         | ditto | "You don't have the gem."
    | `with rock`| catch-all rock        | WITH2 | "You don't have the rock."

which the note read as "3.90 does not do the prepend at all".  That reading
cannot be tested there, because every p*TEXTSRC task command holds a
`%object%`, and at 3.90 a `%object%` walk searches a SNAPSHOT of the line
(MemVar_468224, written at 45F20F, BEFORE the prepend at 45F2AF) with a
one-shot fallback to the argument -- so a 3.90 miss proves nothing about
which string the prepend built.  Every task command here is ALL LITERAL:
checktask's 44B0E2 equality is then the only test, and it reads the
argument, the line as the prepend left it.

The world is make_surfprobe's, three objects and no NPC:

    0 gem   "a"  Lit Room   -- present, never held: the "don't have" cell
    1 rock  "a"  Lit Room   -- the second object, for the 2+-object split
    2 pearl "a"  Cave       -- absent: an instrument 45D0D6 cannot place

    Tasks (all repeatable, unrestricted, Where = everywhere):
      0 `probe`                   -> "PROBE OK."
      1 `aaa with zzz`            -> "PREV1."
      2 `with zzz`                -> "NOPRE."
      3 `aaa with yyy`            -> "H1."
      4 `bbb with yyy`            -> "H2."
      5 `ccc with xxx`            -> "E1."
      6 `ddd with xxx`            -> "E2."
      7 `ccc, ddd with xxx`       -> "E0."
      8 `eee with www`            -> "B1."
      9 `with www`                -> "B0."
     10 `fff with zzz ggg`        -> "PFX1."
     11 `with zzz ggg`            -> "PFX2."
     12 `ggg with zzz`            -> "PFX3."
     13 `cut rock with pearl lll` -> "PFX4."
     14 `push rock`                -> "GENPFX."
     15 `fff with ggg`             -> "PFX5."

`aaa`..`lll`, `www`..`zzz` are in no Runner's vocabulary, so a cell that
matches no task falls to the catch-all or to DontUnderstand and a hit is
never in doubt.  `cut` and `with` are the only real words used.

The questions, one span of the feed each (cmdfile_pwithpfx.txt):

  Q1 PREPEND.  `aaa` then `with zzz`.  PREV1 = the prepend fired and the
     built line reached the task matcher; NOPRE = the line was matched as
     typed.  This is the cell p*TEXTSRC could not ask.

  Q2 WHICH LINE.  `bbb` / `aaa` / `with yyy`.  H1 = the immediately
     previous typed line, H2 = two back.  run390's history counts line
     ELEMENTS (45EC5B) where run380's counts typed lines, so an off-by-one
     would show here.

  Q3 ELEMENTS.  `ccc, ddd` -- ONE typed line, TWO elements at 3.90, one at
     3.7/3.8 -- then `with xxx`.  E2 = the last element, E1 = the first,
     E0 = the whole typed line.

  Q4 BLANK.  `eee` / <Return> / `with www`.  The index has the 3.8+ rewrite
     counting blank lines; B0 or "With what?" says it does, B1 that it
     skips them.

  Q5 CONTINUATION.  `fff` / `with zzz` / `ggg`.  `fff with zzz` is
     deliberately NOT a task, so the built line reaches therest and the
     single-object with-arm answers "With what?" and saves a prefix
     (45D1A0).  The next line then says WHAT was saved: PFX1 = the whole
     built line, PFX2 = just the `with zzz` the player typed, PFX3 = the
     answer went in front.  `jjj` after it asks whether the prefix lives
     one line, as every other 3.9 question prefix does.

  Q6 THE p*TEXTSRC CELL, controlled.  `hhh rock` / `with zzz` / `iii gem`.
     If Q5 says the prefix continues, this rebuilds the line
     `hhh rock with zzz iii gem`, which names TWO objects, so the 2+-object
     split claims it and the instrument after the split is the gem:
     present, not held, "You don't have the gem." -- the p*TEXTSRC answer,
     from a line the player never typed.

  Q7 THE OTHER "With what?".  `cut rock with pearl` is the 2+-object arm's
     own miss (45D16D, instrument absent).  `lll` next: PFX4 = its prefix
     continues a line the way the single-object arm's does; anything else
     = only one of the two arms keeps a prefix.

WHAT IT MEASURED (2026-09-20; Adrift_217_wp370, 219_wq380, 219_wp390,
220_wp400 and the wr/ws/wt/wu/wv/wx runs of cmdfile_pwithpfx3..8):

  * The prepend is real at 3.80, 3.90 AND 4.00, and absent at 3.70
    (PREV1 / NOPRE).  The p*TEXTSRC cells that read as "3.90 does not
    prepend" were the %object% snapshot rule and the continuation below.
  * It takes the previous TYPED LINE, not the previous element: `ccc, ddd`
    then `with xxx` is E0 everywhere that prepends, although 3.90 runs that
    line as two elements and its history counts elements (45EC5B).
  * A blank previous line crashes run380 ("Run-time error '9'"), gives
    run390 a garbage "Which pearl.  The gem or the rock?" prompt, and
    leaves run400 with " with www", leading space and all: DontUnderstand.
  * The "With what?" prefix (45D1A0 and 45D3E0 alike -- Q7 continues too)
    is 3.90's alone, and it is `Left(line, InStr(line, "with") + 4)`: the
    line cut just past the word, the instrument half thrown away.  A later
    line is retried as `<prefix> <line>` only when the line alone is not
    understood, and the retry reaches therest only, never the task matcher
    -- PFX1/PFX2/PFX3 never print.  A line therest answers by itself (`cut
    rock`, bare `push`, `i`, `look`) and any task (`probe`) drop the
    prefix.  Inside the retry the ordinary 3.9 rules run: two objects named
    and the split claims it ("You don't have the rock."), one object and
    the with-arm answers -- but only if its FIRST occurrence in the joined
    line sits after "with", so `hhh gem` / `with zzz` / `gem` is "With
    what?" again.  PORTED 2026-09-20 (lib_with_prefix_390_note() and
    lib_with_prefix_390_continuation() in sclibrar.cpp); feeds 3, 5, 6 and
    7 are identical to run390 on every turn.  It corrected the split as
    well: the "two or more objects" it needs are the objects the WHOLE line
    names, not one per half, so `fff with gem rock` is "You don't have the
    rock." although its head names nothing (lib_with_clause_390).
  * The " with " clause itself runs at 3.70 and 3.80 too, and `break` takes
    its refusals without its suffix and ends in "!" below 3.90; 3.70 makes
    the split above its absent-object test.  All three are ported.
  * Still open: `open X with Y` is the 4.0 wording at 3.70 but "You can't
    open the gem!" -- the instrument, with a bang -- at 3.80/3.90, and
    `read X with Y` is the examine ambiguity prompt below 3.90.

Feeds (all in ~/adrift-battle/runner/wine):
    cmdfile_pwithpfx.txt   Q1-Q7 as above (run380 dies at the blank line)
    ...2.txt   the same without the blank span, for 3.80
    ...3.txt   what clears the prefix: probe, look, an answered verb
    ...4.txt   the clause itself: cut/push/break/open/read with, held and not
    ...5.txt   the prefix over a task, and one line after it
    ...6.txt   lines answered above and inside therest (push, cut, i)
    ...7.txt   whether the prefix grows as it is retried
    ...8.txt   bare-verb baselines, and fix/lock/turn/smell/clear with

Usage:
    python3 make_withprefixprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    TRANSCRIPT=Adrift_wp390.txt ./fast.sh p39WITHPFX.taf \\
        cmdfile_pwithpfx.txt run390x.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("gem",   "a", "A green gem.", ("room", LIT),  "", 0, 0, 0),
    ("rock",  "a", "A grey rock.", ("room", LIT),  "", 0, 0, 0),
    ("pearl", "a", "A white pearl.", ("room", CAVE), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {}
surf.WEAPONS = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("aaa with zzz", "PREV1."),
    ("with zzz", "NOPRE."),
    ("aaa with yyy", "H1."),
    ("bbb with yyy", "H2."),
    ("ccc with xxx", "E1."),
    ("ddd with xxx", "E2."),
    ("ccc, ddd with xxx", "E0."),
    ("eee with www", "B1."),
    ("with www", "B0."),
    ("fff with zzz ggg", "PFX1."),
    ("with zzz ggg", "PFX2."),
    ("ggg with zzz", "PFX3."),
    ("cut rock with pearl lll", "PFX4."),
    ("push rock", "GENPFX."),
    ("fff with ggg", "PFX5."),
]
surf.NPCS = []
surf.BATTLE = 0
surf.OUT = {370: "p37WITHPFX.taf", 380: "p38WITHPFX.taf",
            390: "p39WITHPFX.taf", 400: "p4WITHPFX.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
