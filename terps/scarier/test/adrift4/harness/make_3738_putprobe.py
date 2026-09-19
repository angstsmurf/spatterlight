"""The 3.7/3.8 put leftovers probe, in all three pre-4.0 file versions.

`make_surfprobe.py` settled put ON before 3.9 and the dark probes settled put
IN, but both worlds have at most one static, a surface.  This world is built
for the leads still open in WINE-TRANSCRIPTS-TODO.md's "Engine, needs a probe
(3.7 / 3.8)" Put bullet:

    Lit Room  --n-->  Cave  (--s--> back)

    1 coin      held
    2 stone     held
    3 bag       held CONTAINER, open        -- bare take from a HELD container
    4 nut       inside the bag
    5 cupboard  STATIC CONTAINER, open      -- put into a static container
    6 chest     STATIC CONTAINER, openable  -- `open` of a static container
    7 gem       inside the chest
    8 table     dynamic SURFACE on the floor -- neither held nor static
    9 statue    STATIC, plain               -- a bad static target

The chest is written OPEN (on-disk 6): `close chest` / `open chest` in the
feed give the open-listing cell without depending on the closed encoding.

The schema writer is make_surfprobe.build(); this file only swaps its object
table.  The .taf files stay untracked; this generator is the artefact.

Usage:
    python3 make_3738_putprobe.py [370|380|390|all]   -> p3xPUT.taf

Session (from ~/adrift-battle/runner/wine):
    VBRNG=xoshiro TRANSCRIPT=Adrift_p38put.txt ./fast.sh p38PUT.taf \\
        cmdfile_p3738put.txt run380x.exe

Feeds (all in ~/adrift-battle/runner/wine): cmdfile_p3738put.txt
(Adrift_154_p39put.txt); cmdfile_p3738put2.txt (Adrift_154_p37put2.rtf,
Adrift_155_p38put2.rtf); cmdfile_p39takeall*.txt; cmdfile_p3739drop.txt
(3.9, Adrift_160_p39drop.txt) and cmdfile_p3738drop.txt, the same without
the `put everything` lines that crash run370/run380 (Adrift_160_p37drop.rtf,
Adrift_161_p38drop.rtf).
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("coin",     "a",  "A gold coin.",     ("held",),        "",          0, 0, 0),
    ("stone",    "a",  "A grey stone.",    ("held",),        "",          0, 0, 0),
    ("bag",      "a",  "A cloth bag.",     ("held",),        "container", 5, 1, 0),
    ("nut",      "a",  "A brass nut.",     ("in", "bag"),    "",          0, 0, 0),
    ("cupboard", "a",  "A tall cupboard.", ("room", LIT),    "container", 9, 1, 1),
    ("chest",    "a",  "An oak chest.",    ("room", LIT),    "container", 9, 1, 1),
    ("gem",      "a",  "A red gem.",       ("in", "chest"),  "",          0, 0, 0),
    ("table",    "a",  "A low table.",     ("room", LIT),    "surface",   9, 0, 0),
    ("statue",   "a",  "A marble statue.", ("room", LIT),    "",          0, 0, 1),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.OUT = {370: "p37PUT.taf", 380: "p38PUT.taf", 390: "p39PUT.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390] if arg == "all" else [int(arg)]):
        surf.emit(v)
