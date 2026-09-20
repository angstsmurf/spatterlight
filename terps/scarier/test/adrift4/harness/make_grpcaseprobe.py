#!/usr/bin/env python3
"""Case inside a 4.0 GROUP: the last arm of the "output filter" lead.

WINE-TRANSCRIPTS-TODO.md's open-leads list carried one line reading "the
NewParse `%` pattern binary path" ever since the `uip_set_binary_input()`
port, and `adrift-task-marker-case-free` narrowed it to a sentence: the two
engines "differ only at 4.0's NewParse group compare (45D7FA/45D835, binary
both sides) and nothing measures that."  This is the probe that measures it.

What the listing says.  A 4.0 task command meets three tests (45D9FC), and
each one treats case differently:

  * equality 45DA51 -- `LCase(line) = LCase(cmd)`, both sides folded;
  * the `*` matcher 457D68 -- lower-cases the PATTERN (457B17) and then
    compares binary, which is invisible because the typed line is already
    lower-case (45C5DC);
  * NewParse 45D940 -- folds NEITHER side.  Its two compares are
    `EqStr` at 0005D7FA and `EqVar` at 0005D835 in run400.p32dasm.txt, and
    a census of the whole dump finds 551 `EqStr` / 178 `NeStr` and not one
    text-compare opcode anywhere, so every module in the exe is Option
    Compare Binary.

So the group path is the Runner's only case-SENSITIVE command compare, and
two things could feed it a capital:

  1. the author, writing `zog [Rock/Gem]` in the Generator.  This is where
     the probe earns its keep: `adrift-task-marker-case-free` concluded
     from p*CASEREF that the task command is lower-cased AT LOAD (that is
     the only rule explaining both `frob %Object%` firing and `bork %Big%`
     missing a variable named `Big`), but the fold site was never found in
     the listing -- no LCase is assigned into the command anywhere grep can
     see.  Every other compare folds case by itself, so a capitalised
     group command is the ONLY cell in the Runner that can tell a load-time
     fold from no fold at all.
  2. a `%object%` substitution.  The object matcher 458E6C splices the
     Short/Alias RAW -- "CRITICAL: there is no LCase() on the substituted
     name", measured on p4BURN 2026-08-25 -- and routes a group pattern to
     45D940 at 458D11/458E29.  Its twin 4696A4 DOES lower the NPC Name
     (4691B4) and every Alias (469207) before splicing, and routes groups
     to 45D940 at 468FBB/46912D/4692E8/469542.  So the same group shape
     should refuse a capitalised object Short and accept a capitalised NPC
     Name, which is the asymmetry those two annotations predict and nothing
     has yet typed.

The world is make_surfprobe's, with one capital planted in it:

    rock   Short "rock", "a big", Lit Room
    Gem    Short "Gem",  "a",     Lit Room   -- the capitalised Short
    Fay    NPC, Name only, no Alias, Lit Room

    Tasks:
      0  `probe`                  -> "PROBE OK."
      1  `zog [Rock/Gem]`         -> "GC1."   an authored capital inside []
      2  `nurb {The} rock`        -> "GC2."   an authored capital inside {}
      3  `frob [%object%/zzz]`    -> "GC3 [%object%]."   KEY: the % path
      4  `wibb %object%`          -> "GC4 [%object%]."   control, no group
      5  `blip [%character%/zzz]` -> "GC5 [%character%]."  KEY: the twin
      6  `murg %character%`       -> "GC6 [%character%]."  control, no group

`zog`, `nurb`, `frob`, `wibb`, `blip`, `murg` and `zzz` are in no Runner's
vocabulary, so a cell that misses every task falls to DontUnderstand or the
object catch-all and a hit is never in doubt.

The cells, in feed order (cmdfile_pgrpcase.txt):

    probe
    look                 stamp the room: %object% gates on the seen byte
    zog rock             group expansion of an authored capital  -- THE CELL
    zog gem              the other alternative, same question
    zog [rock/gem]       the literal, lower case: equality folds, must fire
    zog [Rock/Gem]       the literal as authored -- the line is lower-cased
                         at read, so this is the same string as the one
                         above and must fire too
    nurb the rock        the optional word, authored `{The}`  -- THE CELL
    nurb rock            the optional word skipped: no capital is compared,
                         so this must fire whatever the answer above is
    frob rock            %object% spliced into a group, lower-case Short
    frob gem             the same task, capitalised Short -- refused if the
                         458E6C "no LCase" rule reaches the group path
    wibb rock            control: the same pair without a group
    wibb gem             control
    blip fay             %character% spliced into a group -- 4696A4 lowers
                         the Name, so this must fire where `frob gem` does
                         not
    murg fay             control
    probe                throwaway tail

Usage:
    python3 make_grpcaseprobe.py [400|all]
Session (from ~/adrift-battle/runner/wine):
    TRANSCRIPT=Adrift_grpcase400.txt ./fast.sh p4GRPCASE.taf \\
        cmdfile_pgrpcase.txt run400x.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("rock", "a big", "A grey rock.", ("room", LIT), "", 0, 0, 0),
    ("Gem",  "a",     "A green gem.", ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEAPONS = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("zog [Rock/Gem]", "GC1."),
    ("nurb {The} rock", "GC2."),
    ("frob [%object%/zzz]", "GC3 [%object%]."),
    ("wibb %object%", "GC4 [%object%]."),
    ("blip [%character%/zzz]", "GC5 [%character%]."),
    ("murg %character%", "GC6 [%character%]."),
]
surf.NPCS = [
    ("Fay", "", "", "A quiet woman.", LIT, "Fay is here.", [], 1),
]
surf.BATTLE = 0
surf.OUT = {370: "p37GRPCASE.taf", 380: "p38GRPCASE.taf",
            390: "p39GRPCASE.taf", 400: "p4GRPCASE.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "400"
    for v in ([400] if arg in ("400", "all") else [int(arg)]):
        surf.emit(v)
