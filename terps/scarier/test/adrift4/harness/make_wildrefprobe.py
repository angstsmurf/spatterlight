#!/usr/bin/env python3
"""A `*` task command that ALSO holds a %object% or %character%.

The unported half of the checkwild lead in WINE-TRANSCRIPTS-TODO.md.  Scarier
ran `uip_wildcard_match_pre400()` as the whole test for a pre-4.0 `*` command,
but only for a command with no reference in it, because the substitution the
Runner does first was emulated below 3.90 only.  A 3.9 command like
`* zog * %object% *` therefore fell back on the pattern tree, which matches
positionally and in order, and even at 3.80, where the substitution ran, the
reference the task's text expanded was still the tree's.  (4.0's own cutting
matcher is skipped for a reference-bearing command too -- `!strpbrk (pattern,
"%[{")` in run_match_task_commands() -- and the probe says that is harmless:
run400 agrees with the tree on every cell below.)

What the Runners do instead, read from the decompiles:

  * run390 checktask (44AA5A-44B149).  With `InStr(cmd, "%object%") > 0` it
    walks the object array under `c(Short, MemVar_468224) And .global_44 = 1`
    (44AAD6) and then the same for the Alias `.global_8` (44AB65): the SEEN
    byte is the only gate, there is no scope test and no break.  Every hit
    stores its index in MemVar_4681A8 and calls
    `Replace(cmd, "%object%", name, 1, -1, 0)` -- but the FIRST replacement
    leaves no "%object%" behind, so the string is spelled by the FIRST
    namesake the line names while the reference the task's text expands is
    the LAST.  MemVar_468224 is the line as it stood at 45F20F, before the
    everything/slap/except rewrites; when that loop binds nothing a second
    pair of loops (44ABFE, 44AC98) repeats the walk against checktask's own
    `text` argument.  %character% follows at 44AD2A with no gate at all and
    the Name only, then %number% and %t_number%.  Only then comes the test:
    an all-lowercase equality at 44B0E2 and, `If InStr(cmd, "*") > 0`,
    `checkwild(line, cmd)` at 44B139 whose answer IS the match flag.  The
    command is restored from var_A4 at 44B188, so nothing sticks (3.7 does
    not restore -- see run_370_rewrite_task_command()).

  * run400's matcher (458E6C) substitutes too, but tries each candidate in
    turn and exits on the first that MATCHES, routing a "*" command to
    457D68 and a "["/"{" one to 45D940.  So 4.0 is closer to a tree: the
    reference it binds is one the whole pattern accepted.

  * run370/run380 (4332CA / 43B78B) have the pre-3.9 shape already ported as
    `run_pre400_substitute_references()`: the Short of the lowest-index
    object c() finds in the line spells the command, the LAST such object
    binds it, there is no gate whatever, and no %character% at all.

The world, two rooms so an object can be left behind, deliberately lit:

    Lit Room  --n-->  Cave  (--s--> back)

    0 rock  "a big"  Lit Room                -- LOWEST index, substitutes
    1 gem   "a"      Lit Room, alias "stone" -- HIGHEST, so it binds
    2 coin  "a"      Cave                    -- unseen until `n`

    0 King  "the"    Lit Room                -- the only character

    Tasks:
      0 `probe`               -> "PROBE OK."
      1 `* zog * %object% *`  -> "WILD1 %object%."
      2 `nurb %object% *`     -> "WILD2 %object%."
      3 `* zog * %character% *` -> "WILD3 %character%."
      4 `blip %object% blip`  -> "FLAT %object%."

`blip`, `zog` and `nurb` are in no Runner's vocabulary, so a cell that misses
every task falls to the pre-4.0 object catch-all ("I don't understand what you
want me to do with the gem.") or to DontUnderstand, and a hit is never in
doubt.  `look` separates the cells, because a pending ambiguity answer slot
would otherwise eat the next one.

The cells, in feed order:

    probe
    blip zog blip gem blip      IN ORDER -- the control for task 1
    blip gem blip zog blip      OUT OF ORDER -- THE KEY CELL: checkwild
                                searches each piece with InStr over the whole
                                line, so pre-4.0 must run the task where the
                                tree (and 4.0) refuse
    blip zog blip rock blip gem blip
                                TWO namesakes named -- the string is spelled
                                by the rock (index 0) and the reference is
                                the gem (index 1), so 3.9 must print "WILD1 a
                                gem." for a line the rock matched
    blip zog blip               no object named at all -- "%object%" is left
                                standing in the command and can match nothing
    blip zog blip coin blip     the coin is in the Cave and UNSEEN -- the
                                seen gate, which 3.7/3.8 do not have
    nurb gem blip               task 2, prefix and a trailing " *"
    nurb gem                    the trailing " *" covering nothing: 3.9 pads
                                the line, 3.7/3.8 do not
    nurb blip gem               the name is in the line but not where the
                                prefix needs it -- must refuse everywhere
    blip zog blip king blip     task 3, %character% in order
    blip king blip zog blip     %character% OUT OF ORDER -- the second key
                                cell
    blip zog blip queen blip    no such character -- "%character%" stands
    blip gem blip               task 4, the no-`*` control: equality after
                                the substitution
    blip stone blip             the gem's ALIAS -- run390's second loop
    n                           into the Cave: the coin is seen, the King is
                                left behind
    blip zog blip coin blip     the coin, now seen, with the rock and the gem
                                out of scope -- 3.9 has no scope test
    blip king blip zog blip     the King is ABSENT now, and 3.9's %character%
                                loop has no gate at all
    s
    probe

What the four Runners answered, 2026-09-20 (Adrift_wildref370.rtf,
Adrift_wildref380.rtf, Adrift_wildref390.txt, Adrift_wildref400.txt):

  order       Pre-4.0 checkwild decides the substituted command, so ORDER IS
              FREE there too: `blip gem blip zog blip` runs task 1 at 3.70,
              3.80 and 3.90.  4.00 cuts and refuses it ("I don't understand
              what you want me to do with the gem.").
  two names   `blip zog blip rock blip gem blip` is "WILD1 a gem." at 3.80
              and 3.90: the walk replaces on its FIRST hit, so the rock (index
              0) spells the literal that matched, and stores an index on EVERY
              hit, so the gem (index 1) is what the task's text expands.  4.00
              tries each candidate whole and keeps the first that matches --
              "WILD1 a big rock."  3.70's command had already gone sticky, so
              it answers "WILD1 %object%."
  seen gate   The unseen coin binds at 3.80 ("WILD1 a coin.") and nowhere
              else: 3.90 gates the walk on the seen byte, 4.00 on seen and
              scope.  3.70 never reached the task (sticky again) and answered
              the 3.70-only "You can't see the coin."
  padding     `nurb gem` against "nurb %object% *" runs at 3.90 and 4.00 and
              falls to the object catch-all at 3.70/3.80, which never pad.
  %character% 3.90 binds the King by Name with NO gate at all: in order, out
              of order, and from the Cave with the King left behind, all
              three are "WILD3 King."  4.00 takes only the in-order line.
              3.70/3.80 have no %character% arm, so the literal stands and
              every cell is "I don't understand."
  alias       `blip stone blip` is "FLAT a gem." at 3.90 and 4.00 -- 3.90's
              second loop walks the Alias -- and the object catch-all at
              3.70/3.80, which know only the Short.

PORTED 2026-09-20: scrunner.cpp run_pre400_substitute_references().  3.70 and
4.00 were already right on every cell; 3.80 gained the last-hit binding and
3.90 the whole line-driven substitution.

Usage:
    python3 make_wildrefprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    TRANSCRIPT=Adrift_wildref390.txt ./fast.sh p39WILDREF.taf \
        cmdfile_pwildref.txt run390x.exe
    TRANSCRIPT=Adrift_wildref400.txt ./fast.sh p4WILDREF.taf \
        cmdfile_pwildref.txt run400x.exe
    TRANSCRIPT=Adrift_wildref380.txt ./fast.sh p38WILDREF.taf \
        cmdfile_pwildref.txt run380.exe
    TRANSCRIPT=Adrift_wildref370.txt ./fast.sh p37WILDREF.taf \
        cmdfile_pwildref.txt run370.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("rock", "a big", "A grey rock.",  ("room", LIT),  "", 0, 0, 0),
    ("gem",  "a",     "A green gem.",  ("room", LIT),  "", 0, 0, 0),
    ("coin", "a",     "A gold coin.",  ("room", CAVE), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {"gem": "stone"}
surf.WEAPONS = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("* zog * %object% *", "WILD1 %object%."),
    ("nurb %object% *", "WILD2 %object%."),
    ("* zog * %character% *", "WILD3 %character%."),
    ("blip %object% blip", "FLAT %object%."),
]
surf.NPCS = [
    ("King", "the", "", "A stern king.", LIT, "The King is here.", [], 0),
]
surf.BATTLE = 0
surf.OUT = {370: "p37WILDREF.taf", 380: "p38WILDREF.taf",
            390: "p39WILDREF.taf", 400: "p4WILDREF.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
