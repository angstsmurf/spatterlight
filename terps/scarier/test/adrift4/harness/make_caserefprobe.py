#!/usr/bin/env python3
"""Is a task command's reference marker matched case-insensitively?

The last open arm of the checktask lead in WINE-TRANSCRIPTS-TODO.md.
Scarier knows four markers by name -- %object%, %character%, %number% and
%text% -- and compares them BYTE FOR BYTE: `run_pattern_references()` tests
`token == "%character%"` and scparser.cpp's UIP_TOKENS lookup is a plain
strncmp.  Anything else a `%word%` spells becomes a TOK_VARIABLE, so a task
written `Molest *%Character%` (X-Files task 30) is an unknown-marker literal
here.  The Runners look to disagree, and p*VARREF already said so sideways:

  * run390 44AF07 builds the user-variable marker as `"%" & var(n).Name &
    "%"` and looks for it with InStr(..., 0) -- vbBinaryCompare, so nothing
    folds case at the comparison.  Yet `wibb %NUM%` reached a variable named
    `num` while `bork %Big%` AND `snib %big%` over a variable named `Big`
    reached nothing at all.  Both halves are explained by one rule: the
    task COMMAND is already lower-case by the time checktask reads it (the
    stored variable Name is not).  checktask itself holds no LCase above
    44B0BA, and the marker tests read the command straight out of the task
    record (`var_110(0).<n>.global_0` at 44AAA5/44AF12), so the folding has
    to happen at load.

  * If that is right, every marker in this file is lower-cased before any
    lookup, and `%Object%`, `%CHARACTER%`, `%Number%` and `%TEXT%` are all
    live references in the Runner -- the exact opposite of Scarier.

So this probe types the two halves of each marker pair and sees which task
speaks.  It also asks the same question of checkwild and of the equality,
where the answer is expected but unrecorded: a `*` command spelled with
capitals, and an ordinary word spelled with capitals.

The world is make_surfprobe's, two objects and one NPC in the Lit Room:

    rock, gem      both present, both stamped by the opening `look`
                   (3.90's %object% walk gates on the SEEN byte)
    Fay            present, Name only, no Alias -- pre-4.0's %character%
                   walk tests c(Name) and nothing else

    Tasks:
      0  `probe`              -> "PROBE OK."
      1  `zog %object%`       -> "LOOBJ [%object%]."     control
      2  `frob %Object%`      -> "UPOBJ [%object%]."     KEY
      3  `wibb %character%`   -> "LOCHR [%character%]."  control, 3.80+
      4  `nurb %CHARACTER%`   -> "UPCHR [%character%]."  KEY
      5  `bork %number%`      -> "LONUM [%number%]."     control, 3.90+
      6  `blip %Number%`      -> "UPNUM [%number%]."     KEY
      7  `snib %text%`        -> "LOTXT."                control, 4.00 only
      8  `murg %TEXT%`        -> "UPTXT."                KEY
      9  `* zug * rock *`     -> "LOWILD."               control
     10  `* Zag * GEM *`      -> "UPWILD."               KEY: does checkwild
                                                         (and 4.0's cutting
                                                         matcher) fold case?

`zog`, `frob`, `wibb`, `nurb`, `bork`, `blip`, `snib`, `murg`, `zug`, `zag`
and `quux` are in no Runner's vocabulary, so a cell that misses every task
falls to the pre-4.0 object catch-all or to DontUnderstand and a hit is
never in doubt.

The cells, in feed order (cmdfile_pcaseref.txt):

    probe
    look                 stamp the room, so the 3.90 %object% walk can bind
    zog rock             %object%   control
    frob rock            %object%   KEY -- runs only if %Object% is a marker
    wibb fay             %character% control      (3.80+)
    nurb fay             %character% KEY
    bork 7               %number%   control       (3.90+)
    blip 7               %number%   KEY
    snib quux            %text%     control       (4.00)
    murg quux            %text%     KEY
    xxx zug xxx rock xxx a '*' command, lower case
    xxx zag xxx gem xxx  the same spelled with capitals
    probe                throwaway: 3.7/3.8 lose the last cell from the .rtf

Usage:
    python3 make_caserefprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    ./par.sh job_caseref.txt
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("rock", "a big", "A grey rock.", ("room", LIT), "", 0, 0, 0),
    ("gem",  "a",     "A green gem.", ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEAPONS = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("zog %object%", "LOOBJ [%object%]."),
    ("frob %Object%", "UPOBJ [%object%]."),
    ("wibb %character%", "LOCHR [%character%]."),
    ("nurb %CHARACTER%", "UPCHR [%character%]."),
    ("bork %number%", "LONUM [%number%]."),
    ("blip %Number%", "UPNUM [%number%]."),
    ("snib %text%", "LOTXT."),
    ("murg %TEXT%", "UPTXT."),
    ("* zug * rock *", "LOWILD."),
    ("* Zag * GEM *", "UPWILD."),
]
surf.NPCS = [
    ("Fay", "", "", "A quiet woman.", LIT, "Fay is here.", [], 1),
]
surf.BATTLE = 0
surf.OUT = {370: "p37CASEREF.taf", 380: "p38CASEREF.taf",
            390: "p39CASEREF.taf", 400: "p4CASEREF.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
