#!/usr/bin/env python3
"""Two present NPCs share a name: what the pre-4.0 character handler answers.

SCARE asked "Please be more clear, who do you want to <verb>?  Ann or Bob?",
a string no Runner carries.  The Runners' characters() is one pass over
every NPC in index order: an arm that assigns the message buffer leaves the
LAST named NPC's answer, an arm guarded by an empty buffer the FIRST's.

    Lit Room: Dave; Ann (female) and Bob, both "a guard"; the stone
    Cave:     Cora, "a guard" too -- unseen until you go north

Usage:
    python3 make_3738_npcambprobe.py [370|380|390|all]   -> p3xNPCAMB.taf
Session (from ~/adrift-battle/runner/wine):
    ./fast.sh p39NPCAMB.taf cmdfile_pnpcamb.txt run390x.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("stone", "a", "A grey stone.", ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.TASKS = [("probe", "PROBE OK.")]
surf.NPCS = [
    ("Dave", "", "", "A quiet man.", LIT, "Dave is here.",
     [("key", "DAVE KEY.")], 0),
    ("Ann", "a", "guard", "A tall guard.", LIT, "Ann is here.",
     [("key", "ANN KEY.")], 1),
    ("Bob", "a", "guard", "A short guard.", LIT, "Bob is here.",
     [("key", "BOB KEY.")], 0),
    ("Cora", "a", "guard", "A third guard.", CAVE, "Cora is here.",
     [("key", "CORA KEY.")], 0),
]
surf.OUT = {370: "p37NPCAMB.taf", 380: "p38NPCAMB.taf", 390: "p39NPCAMB.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390] if arg == "all" else [int(arg)]):
        surf.emit(v)
