"""The pre-4.0 re-measure of three rules first measured on run400 only.

  * A task action never moves a static object (b3e6cfbab, run400's mover
    skips Static = 1).  `kick %object%` moves the referenced object to the
    Cave: the stone is the dynamic control, the statue the static cell.
  * A trailing space in a task command is a space the input must have
    (093a12d5e, NODE_HARD_WHITESPACE): `get placemat `.  The wildcard- and
    %object%-terminated `pat * ` / `polish %object% ` ignore their space in
    run400.
  * A silent literal task that the library could never reach by verb +
    short name loses the line to the library (193593e7b, the Space Boy
    `drop cape to the floor` peek).  Here typed exactly, +250; `drop cape
    gently` +100 likewise; `take orb` +5 is the reachable (Sommeril) twin.

    Lit Room  --n-->  Cave  (--s--> back)

    1 stone     loose in the Lit Room
    2 statue    STATIC, Lit Room
    3 placemat  loose in the Lit Room
    4 cape      held
    5 orb       loose in the Lit Room

Feed: ~/adrift-battle/runner/wine/cmdfile_pbeyond.txt, under run370x,
run380x and run390x (4.0 was the original measurement).

Usage:
    python3 make_beyondprobe.py [370|380|390|400|all]   -> p3xBEYOND.taf
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("stone",    "a",   "A grey stone.",     ("room", LIT), "", 0, 0, 0),
    ("statue",   "a",   "A marble statue.",  ("room", LIT), "", 0, 0, 1),
    ("placemat", "a",   "A woven placemat.", ("room", LIT), "", 0, 0, 0),
    ("cape",     "a red", "A red cape.",     ("held",),     "", 0, 0, 0),
    ("orb",      "a silver", "A silver orb.", ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEAPONS = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("kick %object%", "KICKED.", {"move_ref": CAVE}),
    ("get placemat ", "PLACEMAT TASK."),
    ("pat * ", "PATTED."),
    ("polish %object% ", "POLISHED."),
    ("drop cape to the floor", "", {"score": 250}),
    ("drop cape gently", "", {"score": 100}),
    ("take orb", "", {"score": 5}),
    ("zap %object%", "ZAPPED.", {"move_ref": CAVE}),
    ("wave cape slowly", "", {"score": 1000}),
]
surf.BATTLE = 0
surf.OUT = {370: "p37BEYOND.taf", 380: "p38BEYOND.taf",
            390: "p39BEYOND.taf", 400: "p4BEYOND.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
