"""The take-"and" / take-from-"and" probe with HELD containers, all four
file versions.  Companion to make_3738_tkaprobe.py, whose containers sat in
the room and so were unreachable below 4.0 (the pre-4.0 reach rule turned
every take-from cell into "You are not holding a box." / "You can't do
that!").  Here the box, bag, tray and chest are carried, so:

  - the 3.9 take-from " and " clause (last container, the collection loop
    that collects nothing, the pending slot after `Get X from what?`);
  - the 3.7/3.8 take-from " and " clause (last matching name);
  - the 4.0 " and " clause (first container) and the take-"and" line that
    names a contained object beside a loose one (`take stone and nut`);
  - the 3.8 "Please take objects from one place at a time." arm

all reach the container.

    1 coin    held
    2 hat     held, WEARABLE
    3 stone   loose, Lit Room
    4 rock    loose, Lit Room
    5 gem     loose, Cave                  -- named but absent
    6 box     open container, HELD, holding
    7 nut
    8 bag     open container, HELD, holding
    9 bolt
   10 tray    surface, HELD, carrying
   11 key
   12 chest   CLOSED container, HELD, holding
   13 ring
   14 table   surface, Lit Room, carrying
   15 cup
   16 statue  static, Lit Room

Usage:
    python3 make_3738_tkbprobe.py [370|380|390|400|all]
        -> p37TKB.taf / p38TKB.taf / p39TKB.taf / p4TKB.taf

Feed (~/adrift-battle/runner/wine): cmdfile_ptkb.txt, jobs_ptkb.txt
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
    ("box",    "a", "A wooden box.",     ("held",),       "container", 5, 1, 0),
    ("nut",    "a", "A brass nut.",      ("in", "box"),   "",          0, 0, 0),
    ("bag",    "a", "A cloth bag.",      ("held",),       "container", 5, 1, 0),
    ("bolt",   "a", "A steel bolt.",     ("in", "bag"),   "",          0, 0, 0),
    ("tray",   "a", "A tin tray.",       ("held",),       "surface",   5, 0, 0),
    ("key",    "a", "A small key.",      ("on", "tray"),  "",          0, 0, 0),
    ("chest",  "a", "An oak chest.",     ("held",),       "container", 5, 2, 0),
    ("ring",   "a", "A silver ring.",    ("in", "chest"), "",          0, 0, 0),
    ("table",  "a", "A low table.",      ("room", LIT),   "surface",   5, 0, 0),
    ("cup",    "a", "A tin cup.",        ("on", "table"), "",          0, 0, 0),
    ("statue", "a", "A marble statue.",  ("room", LIT),   "",          0, 0, 1),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.SITLIE = {}
surf.WEARABLE = {"hat"}
surf.NPCS = []
surf.OUT = {370: "p37TKB.taf", 380: "p38TKB.taf", 390: "p39TKB.taf",
            400: "p4TKB.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
