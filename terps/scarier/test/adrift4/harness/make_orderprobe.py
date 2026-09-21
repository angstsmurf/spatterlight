#!/usr/bin/env python3
"""The claiming handlers the two-verb rule never measured: where do they sit?

WINE-TRANSCRIPTS-TODO's two-verb entries ("Below 4.0 a two-verb line is
decided by the call order" and "At 4.0 a two-verb line is decided by a
DIFFERENT call order") settled takes, drops, wears, removes and examines,
and left the rest of generaltasks' run alone because nothing had measured
it: openclose, whereis, gotoplace, characters, give and dobattle.

The listings put all four Runners in the SAME shape (run400 48A457-48B56E,
run390 45F439-45FD8A, run380 4421A3-442AE4, run370 43B942-43C22D):

    takes*    drops*    inventory*   [insides* 3.8+]   tasks*
    wears     removes   dobattle     dohints          [insides 3.7]
    sitstand  openclose  ...  look*  examines*  ...  score  give-rewrite
    whereis   gotoplace("&&&")       therest(empty buffer only)  characters

A starred handler CLAIMS -- `If CBool(x()) Then GoTo` the turn tail -- and
everything below it is skipped.  The rest are plain `Call`s whose message
the next writer may overwrite.  So the predictions this probe tests are:

  * openclose runs ABOVE examines and cannot claim, so `x open box` should
    OPEN the box and then print its description; and it runs BELOW takes,
    so `open take box` should be a take with the box still shut -- word
    order deciding nothing either way.
  * whereis and gotoplace run BELOW examines, so `x where is coin` and
    `x go to cave` should be examines' answer with no where-is and no walk.
  * therest runs only on an empty buffer and sits BELOW whereis, so `push
    where is coin` should be the where-is.
  * characters is the LAST thing generaltasks calls and overwrites, so
    `open ask bob about hat` should be Bob's reply -- but `x ask bob about
    hat` should not be, examines having claimed the line.

The world, deliberately flat and lit, one room holding everything so that
no cell is about scope (Cave exists only as somewhere to walk to):

    hat   "a", HELD, wearable      -- wears/removes, and Bob's topic
    coin  "a", in the Lit Room     -- takes/drops/whereis
    box   "a", CONTAINER, CLOSED   -- openclose's target
    nut   "a", inside the box      -- so a `look` shows whether it opened

    Bob, in the Lit Room, topic "hat"

plus the usual repeatable `probe` task printing "PROBE OK." at both ends of
every feed, so a shifted command shows.

Usage:
    python3 make_orderprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    sh par.sh job_p2ord.txt 4
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("hat",  "a", "A felt hat.",   ("held",),      "",          0, 0, 0),
    ("coin", "a", "A gold coin.",  ("room", LIT),  "",          0, 0, 0),
    ("box",  "a", "A wooden box.", ("room", LIT),  "container",  5, 2, 0),
    ("nut",  "a", "A brass nut.",  ("in", "box"),  "",          0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {}
surf.SITLIE = {}
surf.WEARABLE = {"hat"}
surf.WEAPONS = set()
surf.EVENTS = []
surf.VARIABLES = []
surf.BATTLE = 0
surf.NPCS = [
    ("Bob", "", "", "An ordinary man.", LIT, "Bob is here.",
     [("hat", "Bob says, 'That is a fine hat.'")], 0),
]
surf.TASKS = [("probe", "PROBE OK.")]
surf.OUT = {370: "p37ORD.taf", 380: "p38ORD.taf",
            390: "p39ORD.taf", 400: "p4ORD.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
