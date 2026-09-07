#!/usr/bin/env python3
"""ADRIFT 4.0 probe 2: WHICH library commands survive a spent task's RepeatText?

p4REPEAT (make_400_repeatprobe.py) answered the first half of the parked
lead: at 4.00 a done, non-repeatable, non-reversible task with a RepeatText
beats essentially the whole standard library.  Thirteen of its fourteen cells
printed the RepeatText -- including `take`, `x`, `kiss`, `push`, `smell`,
`listen`, a WILDCARD command, and movement, which did not move the player.
Exactly one lost: `talk to bob`, where run400 answered with the library's
`Use the format "ask Bob about [subject]".`

That matches run400's p-code shape only if the RepeatText comes out of the
task dispatcher Proc_19_24_44CCE0 at 48A481, whose TRUE return takes the
`GoTo loc_48B4E3` past every later verb -- so the survivors should be exactly
the handlers that run BEFORE it: inventory (Proc_19_70_45C304), the put/drop
list (Proc_19_40_459DB4), get_outer (Proc_19_22_4582D8), and whatever sits in
the unread 489FD4..48A457 stretch ahead of them, which is where `talk to X`
must live.

Sixteen cells, same shape as probe 1 -- first typing prints `C<n>`, second
prints either `R<n>` (RepeatText won) or the library's answer:

   1  i                  inventory        -- expected to SURVIVE
   2  drop coin          put/drop list    -- expected to SURVIVE
   3  put coin on desk   put/drop list    -- expected to SURVIVE
   4  get out            get_outer        -- expected to SURVIVE
   5  ask bob about hat  ask/talk family  -- ?
   6  talk               thelasthour printed the RepeatText for this one
   7  talk to bob        the known survivor, repeated here as the control
   8  wear hat           wears (48A48C), after the dispatcher
   9  look
  10  wait
  11  score              administrative; may not reach generaltasks at all
  12  open desk
  13  read desk
  14  give coin to bob
  15  x bob              an NPC examine, which is an administrative turn
  16  zzz                Where type 1 = BRAVO: completed there, re-typed in
                         ALPHA, so it says whether an out-of-scope spent task
                         can still refuse -- the reading that would explain
                         thepkgirl (Where "some rooms") and witchtale keeping
                         their library answers in the corpus census.

An event of length 1 that restarts immediately prints `TICK.` at the end of
every counted turn, so the transcript also says which of these are turns.

A third cell set (`--third`, p4REPEAT3) narrows the four survivors down to
their exact wording: the inventory abbreviations, the whole `drop`/`put`/
`all` family, the three spellings of an NPC examine against an examine of an
object and of the player, and the other ways of addressing a character
(`speak to`, `talk <NPC>`, `<NPC>, hello`, `ask <NPC> for`).

Usage:
    python3 make_400_repeatprobe2.py p4REPEAT2.plain
    python3 make_400_repeatprobe2.py --third p4REPEAT3.plain
    python3 taftool.py pack p4REPEAT2.plain <donor.taf> p4REPEAT2.taf
    (then, from ~/adrift-battle/runner/wine)
    sh fast.sh p4REPEAT2.taf cmdfile_rep2.txt run400
"""
import sys

SEP = "\xbd\xd0"

THIRD = "--third" in sys.argv[1:]

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Spent-task RepeatText probe %d." % (3 if THIRD else 2))
s(0)                     # StartRoom: Alpha
ml("You have won.")

# GLOBAL
s("Repeat Probe 400 III" if THIRD else "Repeat Probe 400 II")
s("SCARE probe")
s("NO IDEA.")
s(1)                     # Perspective
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player"); s(0); s("A test subject.")
s(0)                     # Task
s(0); s(0); s(0)         # Position, ParentObject, PlayerGender
s(100); s(100)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0); s(0); s(0)         # NoDebug, NoScoreNotify, NoMap
s(0); s(0); s(0)         # NoAutoComplete, NoControlPanel, NoMouse
s(0); s(0)               # Sound, Graphics
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS
def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0); s(0)           # Alts, HideOnMap

s(2)
room("Alpha", "The first room.", {1: 2})
room("Bravo", "The second room.", {3: 1})

# OBJECTS
def obj(short, room=None, static=False, prefix="a", aliases=(),
        surface=0, capacity=0, wearable=0):
    s(prefix); s(short)
    s(len(aliases))
    for a in aliases:
        s(a)
    s(1 if static else 0)
    s("A plain thing.")
    s(0 if static else room + 4)         # InitialPosition
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    if static:
        s(1); s(room + 1)                # Where: ROOM_LIST1 Type 1, 1-based
    s(0); s(surface); s(capacity)        # Container, Surface, Capacity
    if not static:
        s(wearable); s(0); s(0)          # Wearable, SizeWeight, Parent
    s(0)                 # Openable
    s(0)                 # SitLie
    if not static:
        s(0)             # Edible
    s(0)                 # Readable
    if not static:
        s(0)             # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(3)
obj("coin", room=0)                                        # 0 dynamic
obj("hat",  room=0, wearable=1)                            # 1 dynamic, worn
obj("desk", room=0, static=True, surface=1, capacity=5)    # 2 static surface

# TASKS
def task(cmd, n, where_type=3, where_room=None, repeattext=None):
    s(1); s(cmd)
    s("C%d %s." % (n, cmd))
    s("")
    s("" if repeattext is None else repeattext)
    s("")
    s(0)                 # ShowRoomDesc
    s(0)                 # Repeatable: NO
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand
    s(where_type)
    if where_type == 1:
        s(where_room)
    s("")                # Question
    s(0); s(0)           # Restrictions, Actions
    s("")                # RestrMask

CELLS = [
    ("i",                 1, 3, None),
    ("drop coin",         2, 3, None),
    ("put coin on desk",  3, 3, None),
    ("get out",           4, 3, None),
    ("ask bob about hat", 5, 3, None),
    ("talk",              6, 3, None),
    ("talk to bob",       7, 3, None),
    ("wear hat",          8, 3, None),
    ("look",              9, 3, None),
    ("wait",             10, 3, None),
    ("score",            11, 3, None),
    ("open desk",        12, 3, None),
    ("read desk",        13, 3, None),
    ("give coin to bob", 14, 3, None),
    ("x bob",            15, 3, None),
    ("zzz",              16, 1, 1),      # Where type 1: BRAVO only
]

CELLS3 = [
    ("inv",              1, 3, None),
    ("inventory",        2, 3, None),
    ("drop hat",         3, 3, None),
    ("put hat on desk",  4, 3, None),
    ("examine bob",      5, 3, None),
    ("look at bob",      6, 3, None),
    ("speak to bob",     7, 3, None),
    ("talk bob",         8, 3, None),
    ("bob, hello",       9, 3, None),
    ("ask bob for hat", 10, 3, None),
    ("x desk",          11, 3, None),
    ("x me",            12, 3, None),
    ("take all",        13, 3, None),
    ("drop all",        14, 3, None),
    ("put all on desk", 15, 3, None),
    ("turns",           16, 3, None),
]

if THIRD:
    CELLS = CELLS3

s(len(CELLS))
for cmd, n, wtype, wroom in CELLS:
    task(cmd, n, wtype, wroom, "R%d %s." % (n, cmd))

# EVENTS -- length 1, restarts immediately, so its FinishText marks every
# counted turn.
s(1)
s("Ticker")
s(1)                     # StarterType: 1 = immediate
s(1)                     # RestartType: 1 = restart immediately
s(0)                     # TaskFinished
s(1); s(1)               # Time1, Time2
s(""); s(""); s("TICK.") # StartText, LookText, FinishText
s(3)                     # Where: all rooms
s(0); s(0)               # PauseTask, PauserCompleted
s(0); s("")              # PrefTime1, PrefText1
s(0); s(0)               # ResumeTask, ResumerCompleted
s(0); s("")              # PrefTime2, PrefText2
s(0); s(0); s(0); s(0); s(0); s(0)   # Obj2/Dest Obj3/Dest Obj1/Dest
s(0)                     # TaskAffected

# NPCS
s(1)
s("Bob"); s(""); s(0)
s("A patient bystander.")
s(1)                     # StartRoom: Alpha
s(""); s(0); s(0); s(0)  # AltText, Task, Topics, Walks
s(0)                     # ShowEnterExit
s("Bob is here.")        # InRoomText
s(0)                     # Gender

s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
args = [a for a in sys.argv[1:] if not a.startswith("--")]
out = args[0] if args else ("p4REPEAT3.plain" if THIRD else "p4REPEAT2.plain")
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
