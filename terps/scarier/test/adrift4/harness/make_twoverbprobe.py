#!/usr/bin/env python3
"""A line naming TWO library verbs: which handler answers it.

WINE-TRANSCRIPTS-TODO's "the Runner settles that by its call order" lead.
Every pre-4.0 handler enters on a whole-word c() test that looks at the
WHOLE line, so `x get coin` satisfies takes() and examines() alike, and
generaltasks (run380 4421A3-442201, run370 43B958-43B98A, run390
45F439-45F49E) calls them in ONE fixed order:

    takes, drops, inventory, insides, tasks, wears, removes, ... therest

A handler that acts claims the line (`If CBool(takes()) Then GoTo` the turn
tail); one that only writes a refusal does NOT, and the next handler may
overwrite it.  So WORD ORDER NEVER DECIDES below 4.0, and the answer to a
two-verb line is the first handler in call order that can act -- with
examines, down in therest, overwriting a refusal the earlier ones left.

cmdfile_p2verb.txt measured that on p3xREW/p4REW (Adrift_251_2v37.rtf,
252_2v38.rtf, 253_2v39.txt, 254_2v40.txt, 2026-09-21), and
cmdfile_p2verb2.txt the multi-word spellings (Adrift_253_2w37.rtf,
254_2w38.rtf, 255_2w39.txt, 256_2w40.txt): `take off coin` is "You pick up
the coin.", because takes() matches `take` and runs long before removes().

Those probes carry no WEARABLE object, so the one line the corpus really
types -- `take off <worn thing>` -- was still a guess.  This probe is that
cell and nothing else:

    hat    "a", held, WEARABLE
    coin   "a", in the room
    stone  "a", in the room, the control

    probe  PROBE OK.   both ends of the feed, so a shift shows

Usage:
    python3 make_twoverbprobe.py [370|380|390|400|all]

The same three objects answer the put/wear question the `put on X` lead left
open, because a wearable that is HELD is the one state in which wears() has
something to do: cmdfile_p2puton.txt walks the hat through held, worn and
loose, and cmdfile_p2puton2.txt the trailing spelling `put hat on` beside the
held-but-not-wearable coin (Adrift_259_2z37.rtf, 260_2z38.rtf, 261_2z39.txt,
262_2z40.txt, 2026-09-21).

Session (from ~/adrift-battle/runner/wine), job_p2verb3.txt +
cmdfile_p2verb3.txt:
    sh par.sh job_p2verb3.txt 4
    sh par.sh job_p2puton2.txt 4
"""
import sys

import make_surfprobe as surf

LIT = surf.LIT

surf.OBJECTS = [
    ("hat",   "a", "A felt hat.",   ("held",),      "", 0, 0, 0),
    ("coin",  "a", "A gold coin.",  ("room", LIT),  "", 0, 0, 0),
    ("stone", "a", "A grey stone.", ("room", LIT),  "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {}
surf.SITLIE = {}
surf.WEARABLE = {"hat"}
surf.WEAPONS = set()
surf.EVENTS = []
surf.NPCS = []
surf.VARIABLES = []
surf.BATTLE = 0
surf.TASKS = [("probe", "PROBE OK.")]
surf.OUT = {370: "p37TWO.taf", 380: "p38TWO.taf",
            390: "p39TWO.taf", 400: "p4TWO.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
