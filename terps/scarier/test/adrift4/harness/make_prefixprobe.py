#!/usr/bin/env python3
"""The Prefix contest, at 3.90 and 4.00.

Two routines in run400 settle a namesake ambiguity by looking at what the
player typed of each candidate's *Prefix* -- 454454 for objects, called from
co() (46486C), and 450610 for characters, called from the namesake scan
(45E99C).  Neither is modelled in scarier: `lib_co_400_line_leaves_which_
pending()` and `lib_npc_400_find_namesakes_in()` both count namesakes and
stop there, so scarier raises "Which guard.  A red guard or a blue guard?"
for a line that names one of them outright.  The TODO has carried the gap
twice ("co()'s crowded arm (454454) and its &HFE/&HFF answers", "454454's
prefix contest handing the write to a namesake with more Prefix words
typed").  This builds the world that measures it.

    Lit Room  --n-->  Cave  (--s--> back)

    NPCs, all in the Lit Room, all neutral:
      Dave  Prefix ""        no alias    -- the control: never a namesake
      Ann   Prefix "a red"   alias guard
      Bob   Prefix "a blue"  alias guard
      Cid   Prefix "a red"   alias guard -- so "red" TIES between Ann and Cid

    Objects, all in the Lit Room:
      0 tree   Prefix "a red"
      1 tree   Prefix "a blue"
      2 rock   Prefix "a"                -- the control

The NPC count is deliberately one higher than the object count and the
guards deliberately sit at NPC indices 1..3: the flagged namesake index is
the LAST present namesake's (45E8CA), i.e. Cid's 3, which is past the end
of the object array, so the object half of the "Which" question (48B6B1)
can never fire and every character cell reads clean.  See the "names the
OBJECT that shares the character's index" entry in WINE-TRANSCRIPTS-TODO.md
for the collision this dodges.

Both Shorts are "tree" so the object cells need no alias at all; "red" and
"blue" are Prefix words only, which is the whole point -- a Prefix word is
not a name, and the question is whether typing one picks a candidate.

Usage:
    python3 make_prefixprobe.py [390|400|all]
Session (from ~/adrift-battle/runner/wine):
    ./fast.sh p39PFX.taf cmdfile_ppfx.txt run390x.exe
    ./fast.sh p4PFX.taf  cmdfile_ppfx.txt run400x.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

if len(sys.argv) > 2 and sys.argv[2] == "2":
    # Probe 2 scores the contest instead of just detecting it: "big" is a
    # Prefix word only Ann/tree 0 own, "red" is shared by all three, and the
    # articles differ ("a" twice, "the" once) so `x a red guard` and `x the
    # red guard` say whether an article scores like any other Prefix word.
    surf.OBJECTS = [
        ("tree", "a big red", "A big red tree.", ("room", LIT), "", 0, 0, 0),
        ("tree", "a red",     "A red tree.",     ("room", LIT), "", 0, 0, 0),
        ("tree", "the red",   "The red tree.",   ("room", LIT), "", 0, 0, 0),
        ("rock", "a",         "A grey rock.",    ("room", LIT), "", 0, 0, 0),
    ]
    PREFIXES = ["a big red", "a red", "the red"]
    DESCS = ["ANN DESC.", "BOB DESC.", "CID DESC."]
    OUT = {390: "p39PFX2.taf", 400: "p4PFX2.taf"}
else:
    surf.OBJECTS = [
        ("tree", "a red",  "A red tree.",  ("room", LIT), "", 0, 0, 0),
        ("tree", "a blue", "A blue tree.", ("room", LIT), "", 0, 0, 0),
        ("rock", "a",      "A grey rock.", ("room", LIT), "", 0, 0, 0),
    ]
    PREFIXES = ["a red", "a blue", "a red"]
    DESCS = ["A red guard.", "A blue guard.", "A second red guard."]
    OUT = {390: "p39PFX.taf", 400: "p4PFX.taf"}
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEAPONS = set()
surf.TASKS = [("probe", "PROBE OK.")]
surf.NPCS = [
    ("Dave", "",          "",      "A quiet man.", LIT, "Dave is here.",
     [("key", "DAVE KEY.")], 0),
    ("Ann",  PREFIXES[0], "guard", DESCS[0],      LIT, "Ann is here.",
     [("key", "ANN KEY.")], 1),
    ("Bob",  PREFIXES[1], "guard", DESCS[1],      LIT, "Bob is here.",
     [("key", "BOB KEY.")], 0),
    ("Cid",  PREFIXES[2], "guard", DESCS[2],      LIT, "Cid is here.",
     [("key", "CID KEY.")], 0),
]

surf.BATTLE = 0

surf.OUT = OUT

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
