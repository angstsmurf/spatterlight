"""The take-"and" and take-from-"and" probe, in all four file versions.

Leads from WINE-TRANSCRIPTS-TODO.md, "Engine, needs a probe (3.9)" and
"(3.7 / 3.8)":

  - take "and" with a candidate present: the rest of run390's multi loop at
    454CA3 after a partial pre-pass (a worn object, a named absent object, a
    static or a contained object beside a loose candidate);
  - take-from: the " and " clause picks the last container in 3.9 and the
    first in 4.0; 3.9's " and " collection bug; the pending slot after
    `Get X from what?`; in/on wording of the parent-derived take (`take
    nut` with the nut in the box, `take key` with the key on the table);
  - the 3.8 object loop's scope test (`lie on bed` from the next room).

The world is make_surfprobe.py's schema with these objects:

    1 coin    held
    2 hat     held, WEARABLE
    3 stone   loose, Lit Room           -- the candidate
    4 rock    loose, Lit Room           -- a second candidate
    5 gem     loose, Cave               -- named but absent
    6 box     open container, Lit Room, holding
    7 nut
    8 bag     open container, Lit Room, holding
    9 bolt
   10 table   surface, Lit Room, carrying
   11 key
   12 chest   CLOSED container, Lit Room, holding
   13 ring
   14 statue  static, Lit Room
   15 bed     static, Cave, sit/lie

Usage:
    python3 make_3738_tkaprobe.py [370|380|390|400|all]
        -> p37TKA.taf / p38TKA.taf / p39TKA.taf / p4TKA.taf
    (4.00 is packed against the donor p4TAKE.taf, like make_surfprobe.py)

Feed (~/adrift-battle/runner/wine): cmdfile_ptka.txt, jobs_ptka.txt
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("coin",   "a", "A gold coin.",      ("held",),       "",          0, 0, 0),
    ("hat",    "a", "A felt hat.",       ("held",),       "",          0, 0, 0),
    ("stone",  "a", "A grey stone.",     ("room", LIT),   "",          0, 0, 0),
    ("rock",   "a", "A round rock.",     ("room", LIT),   "",          0, 0, 0),
    ("gem",    "a", "A green gem.",      ("room", CAVE),  "",          0, 0, 0),
    ("box",    "a", "A wooden box.",     ("room", LIT),   "container", 5, 1, 0),
    ("nut",    "a", "A brass nut.",      ("in", "box"),   "",          0, 0, 0),
    ("bag",    "a", "A cloth bag.",      ("room", LIT),   "container", 5, 1, 0),
    ("bolt",   "a", "A steel bolt.",     ("in", "bag"),   "",          0, 0, 0),
    ("table",  "a", "A low table.",      ("room", LIT),   "surface",   5, 0, 0),
    ("key",    "a", "A small key.",      ("on", "table"), "",          0, 0, 0),
    ("chest",  "a", "An oak chest.",     ("room", LIT),   "container", 5, 2, 0),
    ("ring",   "a", "A silver ring.",    ("in", "chest"), "",          0, 0, 0),
    ("statue", "a", "A marble statue.",  ("room", LIT),   "",          0, 0, 1),
    ("bed",    "a", "A narrow bed.",     ("room", CAVE),  "",          0, 0, 1),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.SITLIE = {"bed": 3}
surf.WEARABLE = {"hat"}
surf.NPCS = []
surf.OUT = {370: "p37TKA.taf", 380: "p38TKA.taf", 390: "p39TKA.taf",
            400: "p4TKA.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
