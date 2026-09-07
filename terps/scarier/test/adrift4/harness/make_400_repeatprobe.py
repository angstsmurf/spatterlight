#!/usr/bin/env python3
"""ADRIFT 4.0 probe: WHERE does a spent task's RepeatText sit relative to the
standard library?

The parked lead at the foot of WINE-TRANSCRIPTS-TODO.md.  Scarier runs the
whole standard library first and only then offers a done, non-repeatable
task's RepeatText, so the RepeatText is printed only when nothing in the
library claimed the line at all.  run400 prints it in five corpus games where
Scarier prints a library answer (crookedestate `write on wall`, jimpond `look
under desk`, onnafa `d`, humbug `push button`, thelasthour `talk`) and does
NOT print it in four others (lair `talk to X` twice, thepkgirl `kiss katryn`,
witchtale `north`).  Nothing in the census sorts the two halves -- not the
version, not the Where type, not the restrictions.

The live hypothesis is that the RepeatText is reached from the library's
REFUSAL EXITS: run400's handlers, when they are about to give up on a line,
pre-match it against the task table (Proc_19_35_453C50) and the fallback
Proc_19_68_45404C counts a spent task with a RepeatText as a hit, after which
the caller runs the dispatcher Proc_19_24_44CCE0 and the RepeatText is what
comes out.  A handler that ANSWERS never reaches its refusal exit, so the
library keeps talk/kiss.  That reading predicts the split above except for
onnafa (a `d` whose exit exists) and jimpond (`look under X` resolves).

One game, fourteen spent tasks, each typed twice.  The first typing prints
`C<n> ...` (the task ran); the second is the measurement, and prints either
`R<n> ...` (the RepeatText beat the library) or whatever the library says.
Every task is non-repeatable, non-reversible, restriction-free, and has a
RepeatText -- except task 11, the control that has none.

  n   command            what the library would do with it unaided
  --  -----------------  --------------------------------------------------
   1  write on wall      unhandled verb -> DontUnderstand      (crookedestate)
   2  listen             answers ("You hear nothing ...")
   3  talk to bob        answers (ask/talk format line)        (lair)
   4  kiss bob           answers                               (thepkgirl)
   5  look under desk    ?                                     (jimpond)
   6  take coin          answers -- takes it
   7  x coin             answers -- the description
   8  push button        ?                                     (humbug)
   9  smell              answers?
  10  frobnicate         unhandled verb -> DontUnderstand
  11  jump               CONTROL: spent, but no RepeatText at all
  12  * dance *          WILDCARD command, otherwise like 10
  13  north              movement, NO exit that way            (witchtale)
  14  east               movement, exit exists                 (onnafa)

Two of the cells carry a second dimension for free: task 1 and task 9 are
Where type 1 (Alpha only) where every other task is type 3 (all rooms), so
`write on wall` against `frobnicate` and `smell` against `listen` say whether
the Where type sorts the table after all.

13 and 14 are typed last because either one may move the player out of Alpha,
where the objects and the NPC are.

Usage:
    python3 make_400_repeatprobe.py p4REPEAT.plain
    python3 taftool.py pack p4REPEAT.plain <donor.taf> p4REPEAT.taf

Drive it with, from ~/adrift-battle/runner/wine:
    sh fast.sh p4REPEAT.taf cmdfile_rep1.txt run400
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Spent-task RepeatText probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Repeat Probe 400")
s("SCARE probe")
s("NO IDEA.")            # DontUnderstand -- deliberately unmistakable
s(1)                     # Perspective: second person
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
s(0); s(0)               # Sound, Graphics -- both off, so no resource fields
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS -- Alpha east to Bravo, Bravo west to Alpha, and nothing north.
def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0)                 # Alts
    s(0)                 # HideOnMap

s(2)
room("Alpha", "The first room.", {1: 2})
room("Bravo", "The second room.", {3: 1})

# OBJECTS -- see make_400_takeprobe.py for the InitialPosition mapping.
def obj(short, room=None, static=False, prefix="a", aliases=()):
    s(prefix)            # Prefix
    s(short)             # Short
    s(len(aliases))      # V$Alias count
    for a in aliases:
        s(a)
    s(1 if static else 0)
    s("A plain thing.")  # Description
    s(0 if static else room + 4)         # InitialPosition
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    if static:
        s(1); s(room + 1)                # Where: ROOM_LIST1 Type 1, 1-based
    s(0); s(0); s(0)     # Container, Surface, Capacity
    if not static:
        s(0); s(0); s(0) # Wearable, SizeWeight, Parent
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
obj("coin",   room=0)                    # 0 -> Alpha floor, for take/examine
obj("desk",   room=0, static=True, prefix="a")   # 1 -> for `look under desk`
obj("button", room=0, static=True, prefix="a")   # 2 -> for `push button`

# TASKS.  Every one of them completes on its first typing (no restrictions,
# no actions), is non-repeatable and non-reversible, and carries a RepeatText
# except the control.
def task(cmd, n, where_type=3, where_room=None, repeattext=None):
    s(1); s(cmd)         # V$Command
    s("C%d %s." % (n, cmd))                      # CompleteText
    s("")                                        # ReverseMessage
    s("" if repeattext is None else repeattext)  # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(0)                 # Repeatable: NO
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(where_type)        # Where: ROOM_LIST0 Type
    if where_type == 1:
        s(where_room)    # ... and its single room (0-based)
    s("")                # Question
    s(0)                 # Restrictions
    s(0)                 # Actions
    s("")                # RestrMask

CELLS = [
    ("write on wall",   1, 1, 0),      # Where type 1: Alpha only
    ("listen",          2, 3, None),
    ("talk to bob",     3, 3, None),
    ("kiss bob",        4, 3, None),
    ("look under desk", 5, 3, None),
    ("take coin",       6, 3, None),
    ("x coin",          7, 3, None),
    ("push button",     8, 3, None),
    ("smell",           9, 1, 0),      # Where type 1: Alpha only
    ("frobnicate",     10, 3, None),
    ("jump",           11, 3, None),   # the no-RepeatText control
    ("* dance *",      12, 3, None),
    ("north",          13, 3, None),
    ("east",           14, 3, None),
]

s(len(CELLS))
for cmd, n, wtype, wroom in CELLS:
    task(cmd, n, wtype, wroom,
         None if n == 11 else "R%d %s." % (n, cmd))

# EVENTS
s(0)

# NPCS -- one, in Alpha, with no topics and no walks, so `talk to bob` and
# `kiss bob` reach the library's own answers for a character with nothing to
# say.
s(1)
s("Bob")                 # Name
s("")                    # Prefix
s(0)                     # V$Alias count
s("A patient bystander.")# Descr
s(1)                     # StartRoom: 1-based, so Alpha
s("")                    # AltText
s(0)                     # Task
s(0)                     # Topics
s(0)                     # Walks
s(0)                     # ShowEnterExit -> no EnterText/ExitText
s("Bob is here.")        # InRoomText
s(0)                     # Gender: male

s(0); s(0)               # RoomGroups, Synonyms

s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4REPEAT.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
