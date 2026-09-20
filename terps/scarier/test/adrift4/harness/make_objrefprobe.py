#!/usr/bin/env python3
"""A task command's %object%: WHICH of several namesakes binds it.

The seen gate is measured and ported (run400 458E6C gates the object array
on the seen byte, run390's checktask on .global_44; `uip_match_entity()`
carries it as `uip_strict_reference && !is_character && !gs_object_seen`).
What has never been measured is the ORDER, which the TODO has carried as an
open lead since the SCR_TRACE_SCOPE audit: "the two-pass %object% scope
filter proper -- its seen gate is ported, its present-before-absent order is
not."  Scarier has no order at all: `uip_match_entity()` walks the object
array once and lets the LAST seen namesake win, present or not.

Read first, so the probe knows what to ask:

  * run400's task-command matcher (458E6C) takes a third argument and tests
    `Proc_21_53_44B578(i) = arg_14` -- 44B578 is obj_indirectly_in_room(),
    -1 when the object is visible where it stands and 0 when it is not.  So
    one call sees PRESENT objects only.  The first object that both passes
    the seen gate (field 48) and compares equal stores its index in 494208
    and Exit Subs, so within a pass it is the FIRST in index order that
    wins, not the last.  The tail at 458E64 calls the routine again with
    arg_14 = 0, which is the ABSENT-but-seen pass.

    Predictions, if that reading is right: a line naming a present namesake
    binds the lowest-indexed PRESENT one even when a higher-indexed namesake
    is also present; an absent-but-seen namesake binds only when no present
    one does; an unseen one never binds.

  * run390's checktask (44AAD6 for the Short, 44AB65 for the Alias) walks
    the whole object array under `c(name, cmd) And .global_44 = 1` with no
    break and no scope test at all, so the LAST seen namesake wins whether
    it is present or not -- which is what scarier already does.  Note the
    rewrite at 44AB40: Replace() writes the substituted command back over
    the pattern, so the SECOND match replaces nothing (there is no
    "%object%" left) while still storing its index in 4681A8.  With
    namesakes that share a Short the two are indistinguishable; the probe
    keeps every rock's Short "rock" precisely so the binding shows only in
    the task's own text.

  * run370/run380 rewrite the pattern in checktask (run380 43B78B, run370
    4332CA) with the Short of the lowest-index object c() finds in the line
    -- `run_pre390_first_named_object()`, which gates on NOTHING.  The gem
    cells below say whether that is right.  (A UTF-16LE census of the four
    exes counts "%object%" run370: 1, run380: 1, run390: 1, run400: 2, so
    the older Runners DO hold the literal; find.py's 458E6C note and
    RUNNER_TESTS_TODO.md said otherwise and were wrong.)

The world, deliberately lit, two rooms so an object can be seen and left:

    Lit Room  --n-->  Cave  (--s--> back)

    0 rock  "a big"    Lit Room   -- present from turn 1
    1 rock  "a small"  Cave       -- unseen until `n`, then absent-but-seen
    2 rock  "a red"    Lit Room   -- present from turn 1, HIGHEST index
    3 gem   "a"        Cave       -- the only gem; unseen, then absent-seen
    4 coin  "a"        Lit Room   -- the unique-name control

    Tasks:
      0 `probe`        -> "PROBE OK."   (must answer at both ends)
      1 `nurb %object%` -> "NURBED %object%."
      2 `zork`          -> "ZORKED %object% and %character%."  (references
                          in the TEXT that the command never binds: what
                          does a Runner print for a reference that was
                          never bound, and does one bound on an earlier
                          turn linger into this one?)

Three rocks with one Short and three different Prefixes is the whole trick:
the command "nurb rock" is the same string whichever rock binds, so every
cell reaches the task, and the task's own text -- %object% expands to
Prefix + Short -- names the object that bound it.

The cells, in feed order.  `nurb` is in no Runner's vocabulary, so a cell
that misses the task falls to the object catch-all and the two engines must
agree there too.  `look` separates the cells, because a pending ambiguity
answer slot would otherwise eat the next one.

    zork        the reference neither bound nor carried over: turn 1
    nurb rock   in Lit: rocks 0 and 2 present and seen, rock 1 unseen
                -- FIRST-vs-LAST among present.  458E6C says "a big rock",
                   scarier says "a red rock".
    zork        again, straight after a bind -- does it linger?
    nurb coin   the control: one namesake, must agree everywhere
    nurb gem    absent AND unseen -- the seen gate, already ported
    nurb zzz    names no object at all -- nothing binds, and 458E6C's
                unconditional tail call would recurse for ever, so this
                cell also says whether that reading can be right
    n           into the Cave: rock 1 and the gem become seen
    nurb rock   in Cave: present = rock 1 (index 1); rocks 0 and 2 are
                absent but seen -- THE KEY CELL.  Present-first says "a
                small rock", scarier's last-wins says "a red rock".
    nurb gem    present and seen
    s           back to the Lit Room
    nurb rock   as the first cell, but rock 1 is seen now
    nurb gem    absent but SEEN, and no present gem -- the second pass
    nurb zzz    once more, after the passes have run
    zork        and once more at the end

What the four Runners answered, 2026-09-20:

  4.00  two passes, present before absent, FIRST in index order within a
        pass.  `nurb rock` is "NURBED a big rock." in the Lit Room and
        "NURBED a small rock." in the Cave; `nurb gem` is "I don't
        understand." unseen and "NURBED a gem." once seen, present or not;
        `nurb zzz` is "I don't understand." and the Runner plays on, so
        458E6C's tail call is not the endless recursion it reads like.
  3.90  no scope test at all: "NURBED a red rock." in both rooms, the last
        seen namesake, which is what scarier already did.
  3.80  the same, and it binds an unseen absent object too: `nurb gem`
        answers from turn 2.
  3.70  the substitution is written back INTO THE TASK and sticks, so the
        first %object% line a game types spells the task for the rest of
        the session: after `nurb rock` the command is literally "nurb
        rock", `nurb coin` falls to the object catch-all, and a later
        `nurb rock` runs the task with nothing bound -- "NURBED %object%."
        A line that does not match restores the command (feeds 3 and 5).

  and, from task 2, all four print an unbound %object% / %character% RAW,
  on the turn right after one was bound as much as on turn 1.

Usage:
    python3 make_objrefprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine), with the feeds this file
describes; the transcripts are cited from scparser.cpp and scrunner.cpp:

    TRANSCRIPT=Adrift_objref400.txt ./fast.sh p4OBJREF.taf \
        cmdfile_pobjref.txt run400x.exe
    TRANSCRIPT=Adrift_objref390.txt ./fast.sh p39OBJREF.taf \
        cmdfile_pobjref.txt run390x.exe
    TRANSCRIPT=Adrift_objref380.txt ./fast.sh p38OBJREF.taf \
        cmdfile_pobjref.txt run380.exe
    TRANSCRIPT=Adrift_objref370.txt ./fast.sh p37OBJREF.taf \
        cmdfile_pobjref.txt run370.exe

The four short feeds that isolate 3.7's rewrite (b and c/d/e are 3.7's,
b is run on 3.8 as the control):

    cmdfile_pobjref2.txt  probe / nurb gem / look / nurb rock / look /
                          nurb coin / look / nurb gem / look / probe
                          -> Adrift_objref370b.rtf, Adrift_objref380b.rtf
    cmdfile_pobjref3.txt  probe / x coin / nurb rock / look / nurb coin /
                          look / probe   -> Adrift_objref370c.rtf
    cmdfile_pobjref4.txt  probe / frob gem / nurb rock / look / nurb gem /
                          look / probe   -> Adrift_objref370d.rtf
    cmdfile_pobjref5.txt  probe / frob rock / nurb coin / look / nurb rock
                          / look / probe -> Adrift_objref370e.rtf
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("rock", "a big",   "A grey rock.",   ("room", LIT),  "", 0, 0, 0),
    ("rock", "a small", "A small rock.",  ("room", CAVE), "", 0, 0, 0),
    ("rock", "a red",   "A red rock.",    ("room", LIT),  "", 0, 0, 0),
    ("gem",  "a",       "A green gem.",   ("room", CAVE), "", 0, 0, 0),
    ("coin", "a",       "A gold coin.",   ("room", LIT),  "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEAPONS = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("nurb %object%", "NURBED %object%."),
    ("zork", "ZORKED %object% and %character%."),
]
surf.NPCS = []
surf.BATTLE = 0
surf.OUT = {370: "p37OBJREF.taf", 380: "p38OBJREF.taf",
            390: "p39OBJREF.taf", 400: "p4OBJREF.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
