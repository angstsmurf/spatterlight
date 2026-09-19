"""The pre-4.0 task-comma, drop-"and" and namesake-handler probe.

Three leads from WINE-TRANSCRIPTS-TODO.md's "Engine, needs a probe (3.7 /
3.8)" and "(3.9)" lists share one world:

  * A comma in a task command match.  The library treats `verb, noun` as
    `verb noun` at 3.7/3.8 (uip_match_whitespace), but whether run370 /
    run380's checktask does is unmeasured.  Tasks with a literal command, a
    `*` command, a `%object%` command and a command that itself holds a
    comma cover the cases.
  * The pre-4.0 drop "and" arm skips an object inside a held container
    (o(22) 0 or &H9C only).  Ported that way, never measured: the nut in the
    held bag is the cell.
  * 3.7/3.8 handlers other than take were measured on single matches only.
    Two hats and two boxes, both pairs sharing a Short, give wear / remove /
    drop / examine / open / close a namesake each.

The world (make_surfprobe.build() with this object table and task list):

    Lit Room  --n-->  Cave  (--s--> back)

    1 coin    held
    2 bag     held CONTAINER, open
    3 nut     inside the bag             -- the drop "and" cell
    4 stone   loose in the Lit Room      -- `push stone` task target
    5 hat     "a red",  WEARABLE, held
    6 hat     "a blue", WEARABLE, loose  -- the wear/remove/drop namesake
    7 box     "a red",  CONTAINER, openable (open), loose
    8 box     "a blue", CONTAINER, openable (open), loose -- open/close namesake
    9 statue  STATIC

    tasks: probe -> PROBE OK. ; push stone ; rub * ; poke %object% ;
           say hello, world

Feeds (in ~/adrift-battle/runner/wine): cmdfile_ptaskcomma.txt,
cmdfile_pdropheld.txt, cmdfile_pnamesake.txt; jobs_ptask.txt drives all
three under run370x/run380x/run390x.

Usage:
    python3 make_3738_taskprobe.py [370|380|390|all]   -> p3xTASK.taf
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("coin",   "a",      "A gold coin.",     ("held",),      "",          0, 0, 0),
    ("bag",    "a",      "A cloth bag.",     ("held",),      "container", 5, 1, 0),
    ("nut",    "a",      "A brass nut.",     ("in", "bag"),  "",          0, 0, 0),
    ("stone",  "a",      "A grey stone.",    ("room", LIT),  "",          0, 0, 0),
    ("hat",    "a red",  "A red hat.",       ("held",),      "",          0, 0, 0),
    ("hat",    "a blue", "A blue hat.",      ("room", LIT),  "",          0, 0, 0),
    ("box",    "a red",  "A red box.",       ("room", LIT),  "container", 5, 1, 0),
    ("box",    "a blue", "A blue box.",      ("room", LIT),  "container", 5, 1, 0),
    ("statue", "a",      "A marble statue.", ("room", LIT),  "",          0, 0, 1),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEARABLE = {"hat"}
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("push stone", "You push the stone."),
    ("rub *", "You rub something."),
    ("poke %object%", "You poke it."),
    ("say hello, world", "You say hello world."),
]
surf.OUT = {370: "p37TASK.taf", 380: "p38TASK.taf", 390: "p39TASK.taf",
            400: "p4TASK.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390] if arg == "all" else [int(arg)]):
        surf.emit(v)
