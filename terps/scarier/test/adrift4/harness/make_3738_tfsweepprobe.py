"""The run380 take-from task sweep probe: does it follow a REFUSED take-from?

run380's insides() ends its take-from arm, whenever the source object was
found (var_A8 > -1), with a sweep over the whole object table (447405): for
every object with a parent it runs tasks(1) on "get <Short> from <parent
Short>" if checktask passes.  The sweep sits after the whole container /
not-a-container branch, so the listing says it follows the refusals too
(not holding 446CFB, closed 446D5A, not a container 4472E2/44735E/4473D6)
and the take-all arm.  Scarier ports it only after a take-from that got as
far as the backend (lib_take_from_task_sweep_380, 4f79695e4).

The listing also runs the closed test (446D19) after the not-holding test
and unconditionally, so a CLOSED container that is not held should answer
"as it is closed!", where Scarier returns at "not holding".

The world (make_surfprobe.build() with this object table and task list):

    Lit Room  --n-->  Cave  (--s--> back)

     1 coin    held
     2 bag     held CONTAINER, open
     3 nut     inside the bag                -- the success controls
     4 drawer  STATIC CONTAINER, open
     5 knife   inside the drawer             -- the sweep's task target
     6 box     CONTAINER, open, on the floor -- not holding
     7 gem     inside the box
     8 chest   CONTAINER, CLOSED, on the floor -- closed vs not holding
     9 crate   CONTAINER, CLOSED, held       -- closed
    10 stone   loose                         -- not a container
    11 statue  STATIC                        -- static, not a container
    12 tray    SURFACE, held, empty          -- the empty-surface wording

    tasks: probe -> PROBE OK. ; get *knife* -> KNIFE TASK.

Feed: ~/adrift-battle/runner/wine/cmdfile_ptfsweep.txt, jobs_ptfsweep.txt
(run370x / run380x).

Usage:
    python3 make_3738_tfsweepprobe.py [370|380|all]   -> p3xTFSW.taf
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("coin",   "a", "A gold coin.",     ("held",),       "",          0, 0, 0),
    ("bag",    "a", "A cloth bag.",     ("held",),       "container", 5, 1, 0),
    ("nut",    "a", "A brass nut.",     ("in", "bag"),   "",          0, 0, 0),
    ("drawer", "a", "A wooden drawer.", ("room", LIT),   "container", 5, 1, 1),
    ("knife",  "a", "A steel knife.",   ("in", "drawer"), "",         0, 0, 0),
    ("box",    "a", "A tin box.",       ("room", LIT),   "container", 5, 1, 0),
    ("gem",    "a", "A red gem.",       ("in", "box"),   "",          0, 0, 0),
    ("chest",  "a", "An oak chest.",    ("room", LIT),   "container", 5, 2, 0),
    ("crate",  "a", "A pine crate.",    ("held",),       "container", 5, 2, 0),
    ("stone",  "a", "A grey stone.",    ("room", LIT),   "",          0, 0, 0),
    ("statue", "a", "A marble statue.", ("room", LIT),   "",          0, 0, 1),
    ("tray",   "a", "A silver tray.",   ("held",),       "surface",   5, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEARABLE = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("get *knife*", "KNIFE TASK."),
]
surf.OUT = {370: "p37TFSW.taf", 380: "p38TFSW.taf", 390: "p39TFSW.taf",
            400: "p4TFSW.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380] if arg == "all" else [int(arg)]):
        surf.emit(v)
