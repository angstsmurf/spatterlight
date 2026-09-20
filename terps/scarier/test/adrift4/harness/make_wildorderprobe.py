#!/usr/bin/env python3
"""checkwild's middle pieces: do they have to come in order?

The open half of the pre-4.0 checkwild lead in WINE-TRANSCRIPTS-TODO.md:
`uip_wildcard_match_pre400()` is only a VETO on top of Scarier's pattern
tree, so a line the tree refuses can never run the task -- but checkwild
searches each middle piece with `InStr(1, line, piece, 0)` over the WHOLE
line and never cuts it (run390 434611-434674, run380 4295E0, run370
4243D4), and in the Runner's checktask it is the only test there is: at
44B0E2 a command with no `*` is compared for equality, and at 44B10D, when
`InStr(cmd, "*") > 0`, checkwild's answer alone sets the match flag.  So in
the Runner the pieces need not be in order, and a piece may be satisfied
twice by the same occurrence.

The patterns, and what each cell asks:

    1 `* king * rose *`   prefix and tail both empty, two middle pieces
                          " king " and " rose ".  ORDER.
    2 `* zog * zog *`     the same piece twice: one occurrence in the line
                          satisfies both, because nothing is consumed.
    3 `frob * grue`       a real prefix and a real tail.  `frob grue` makes
                          them OVERLAP -- Left(line, 5) = "frob " and
                          Right(line, 5) = " grue" out of nine characters --
                          which a tree matcher that wants the `*` to cover
                          something cannot take.

run390 pads the line with a leading space for a pattern starting "* " and a
trailing one for a pattern ending " *" (4344F0, 434520); run370 and run380
pad nothing, so there a piece written " king " needs a space typed on both
sides of the word.  That split is already ported; the `rose king` cell below
keeps it honest.

The cells, in feed order:

    probe
    blip king blip rose blip   in order -- the control, every version runs
                        it.  The verb is nonsense on purpose: `give king to
                        rose` never reaches the task before 3.90, where the
                        give handler answers "You don't have that." first.
    blip rose blip king blip   OUT OF ORDER, with a space on both sides of
                        both words -- THE KEY CELL, and it asks every
                        version
    king rose           in order but bare: 3.9 pads the line for the
                        leading "* " and trailing " *" and runs it, 3.7/3.8
                        pad nothing and must refuse
    rose king           out of order AND bare -- both rules at once
    a zog b zog c       two occurrences -- the control for task 2
    a zog b             ONE occurrence for two pieces -- the second key
                        cell; 4.0's matcher cuts the line past the first
                        hit, so 4.0 must refuse where pre-4.0 runs it
    frob x grue         prefix, something, tail -- the control for task 3
    frob grue           prefix and tail overlapping, the `*` covering
                        nothing
    frobgrue            the prefix is not the line's start -- must refuse
    probe

Usage:
    python3 make_wildorderprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    TRANSCRIPT=Adrift_wildorder400.txt ./fast.sh p4WILDORD.taf \
        cmdfile_pwildord.txt run400x.exe
    TRANSCRIPT=Adrift_wildorder390.txt ./fast.sh p39WILDORD.taf \
        cmdfile_pwildord.txt run390x.exe
    TRANSCRIPT=Adrift_wildorder380.txt ./fast.sh p38WILDORD.taf \
        cmdfile_pwildord.txt run380.exe
    TRANSCRIPT=Adrift_wildorder370.txt ./fast.sh p37WILDORD.taf \
        cmdfile_pwildord.txt run370.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("rock", "a", "A grey rock.", ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEAPONS = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("* king * rose *", "WILD1 OK."),
    ("* zog * zog *", "WILD2 OK."),
    ("frob * grue", "WILD3 OK."),
]
surf.NPCS = []
surf.BATTLE = 0
surf.OUT = {370: "p37WILDORD.taf", 380: "p38WILDORD.taf",
            390: "p39WILDORD.taf", 400: "p4WILDORD.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
