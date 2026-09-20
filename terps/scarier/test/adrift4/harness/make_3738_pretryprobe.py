"""The canonical prefixed retry, in all three pre-4.0 file versions.

The 3.7/3.8 companions to `make_39_pfxprobe.py`, which measured run390:
does the library's own task look-up ever rebuild the line from the resolved
object's authored Prefix -- "take a pebble" for a typed `take pebble` -- and
hand that to the tasks?  run400 does, for the one-object take (the definite
form, ported 2026-09-14); run390 does not, in any family.

Four take cells pin the retry's shape rather than just its presence:

    pebble   task `take a pebble`   typed `take pebble` and `get pebble`
    stone    task `get a stone`     typed `take stone`  and `get stone`

A typed-verb retry runs the task for `take pebble` and `get stone` only; a
canonical-"get" retry for the two stone lines only; no retry for none.  Then
the two-object put (`put a bean in a jar`) and the drop twin (`drop a coin`,
typed `drop coin` and `put down coin`).

No reporter tasks here -- make_surfprobe's task table is unrestricted by
design, and none are needed: a task that claimed the line prints its
CompleteText where the library prints its own wording.  Each task's
canonical spelling is typed verbatim at the end as the aliveness control.

NB 3.80 rewrites a whole-word `take` to `get` before any task matching
([[adrift38-builtin-take-to-get-rewrite]]), so at 380 the `take ...` cells
arrive as `get ...`; read that version's four take rows accordingly.

Usage:
    python3 make_3738_pretryprobe.py [370|380|390|all]   -> p3xPRETRY.taf

Session (from ~/adrift-battle/runner/wine):
    ./fast.sh p37PRETRY.taf cmdfile_pretry3738.txt run370.exe
    ./fast.sh p38PRETRY.taf cmdfile_pretry3738.txt run380.exe
    ./fast.sh p39PRETRY2.taf cmdfile_pretry3738.txt run390.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("pebble", "a", "A small pebble.", ("room", LIT), "",          0, 0, 0),
    ("stone",  "a", "A grey stone.",   ("room", LIT), "",          0, 0, 0),
    ("bean",   "a", "A dry bean.",     ("held",),     "",          0, 0, 0),
    ("coin",   "a", "A gold coin.",    ("held",),     "",          0, 0, 0),
    ("jar",    "a", "A clay jar.",     ("room", LIT), "container", 9, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.TASKS = [
    ("probe",               "PROBE OK."),
    ("take a pebble",       "TAKEPFX."),
    ("get a stone",         "GETPFX."),
    ("put a bean in a jar", "PUTPFX."),
    ("drop a coin",         "DROPPFX."),
]
surf.OUT = {370: "p37PRETRY.taf", 380: "p38PRETRY.taf", 390: "p39PRETRY2.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390] if arg == "all" else [int(arg)]):
        surf.emit(v)
