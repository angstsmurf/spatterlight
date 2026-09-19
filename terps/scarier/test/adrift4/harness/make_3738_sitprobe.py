"""The sit/stand/lie + drop-all-except probe, in 3.70/3.80/3.90/4.00.

Leads from WINE-TRANSCRIPTS-TODO.md "Engine, needs a probe (3.7 / 3.8)":
run370's sitstand loop has no location test (42AEC8), and 3.7
`drop all except X` with X held (only the nothing-held cell was measured).

    Lit Room  --n-->  Cave  (--s--> back)

    1 coin    held
    2 stone   held
    3 stool   dynamic, Lit Room, sit/stand + lie
    4 chair   STATIC, Lit Room, sit/stand + lie
    5 bed     STATIC, Cave, sit/stand + lie
    6 crate   dynamic, Cave, sit/stand only

Usage:
    python3 make_3738_sitprobe.py [370|380|390|400|all] -> p3xSIT.taf / p4SIT.taf
    (4.00 is packed against the donor p4TAKE.taf, like make_surfprobe.py)

Feeds (~/adrift-battle/runner/wine): cmdfile_p3738sit.txt, cmdfile_p3738sit2.txt
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
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.SITLIE = {"stool": 3, "chair": 3, "bed": 3, "crate": 1}
surf.OUT = {370: "p37SIT.taf", 380: "p38SIT.taf", 390: "p39SIT.taf",
            400: "p4SIT.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
