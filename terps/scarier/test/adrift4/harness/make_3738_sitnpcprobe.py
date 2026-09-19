"""The sit/stand/lie word-anywhere probe with a character and a wearable.

Lead from WINE-TRANSCRIPTS-TODO.md "Engine, needs a probe (3.7 / 3.8)":
lines that `lib_sitstand_anywhere` and `run_wait_anywhere` leave alone
(give, ask, talk, say, inventory, direction, score, hint, profanity words;
a successful wear or remove on such a line) were unmeasured at every
version.  This is the p3xSIT world of make_3738_sitprobe.py plus

    7 hat     held, WEARABLE
    Bob       Lit Room, topic "hat" -> "BOB HAT."

so each of those lines has something real to act on.

Usage:
    python3 make_3738_sitnpcprobe.py [370|380|390|400|all]
        -> p37SITN.taf / p38SITN.taf / p39SITN.taf / p4SITN.taf
    (4.00 is packed against the donor p4TAKE.taf, like make_surfprobe.py)

Feed (~/adrift-battle/runner/wine): cmdfile_psitn.txt, jobs_psitn.txt
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("coin",  "a", "A gold coin.",     ("held",),      "", 0, 0, 0),
    ("stone", "a", "A grey stone.",    ("held",),      "", 0, 0, 0),
    ("stool", "a", "A wooden stool.",  ("room", LIT),  "", 0, 0, 0),
    ("chair", "a", "A soft chair.",    ("room", LIT),  "", 0, 0, 1),
    ("bed",   "a", "A narrow bed.",    ("room", CAVE), "", 0, 0, 1),
    ("crate", "a", "A plank crate.",   ("room", CAVE), "", 0, 0, 0),
    ("hat",   "a", "A felt hat.",      ("held",),      "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.SITLIE = {"stool": 3, "chair": 3, "bed": 3, "crate": 1}
surf.WEARABLE = {"hat"}
surf.NPCS = [
    ("Bob", "", "", "A quiet man.", LIT, "Bob is here.",
     [("hat", "BOB HAT.")], 0),
]
surf.OUT = {370: "p37SITN.taf", 380: "p38SITN.taf", 390: "p39SITN.taf",
            400: "p4SITN.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
