#!/usr/bin/env python3
"""ADRIFT 4.0 probe: does text ALREADY in the turn buffer make a silent task speak?

run400's task dispatcher Proc_19_24_44CCE0 reports the line handled when the
message buffer MemVar_4941B0 is non-empty after its one task ran (44CCC0) --
not when the task grew it.  Scarier ports the "grew" half (run_task_run_speaks,
baroo T107).  This probe asks what the other half costs: whatever sits in the
buffer BEFORE the dispatch -- the inventory listing (45C304 runs above the
dispatcher), the "(the coin)" pronoun echo, an earlier clause of a split line
-- would, if the buffer is not reset, let a silent task claim the line and
keep the library quiet.

Two silent, repeatable tasks (no CompleteText, no actions):
   frob           the library has nothing, so alone it is DontUnderstand
                  ("NO IDEA.")
   x {the} coin   the library examine would print "A plain thing."
plus a silent `i` task, which sits under the inventory listing.

The feed (cmdfile_psilent.txt) types each alone first, then behind a clause
that prints, and behind the pronoun echo.

Usage:
    python3 make_400_silentprobe.py p4SILENT.plain
    python3 make_400_silentprobe.py --moves p4SILENT2.plain
    python3 taftool.py pack p4SILENT.plain p4REPEAT2.taf p4SILENT.taf
    (then, from ~/adrift-battle/runner/wine)
    sh fast.sh p4SILENT.taf cmdfile_psilent.txt run400
"""
import sys

MOVES = "--moves" in sys.argv[1:]

SEP = "\xbd\xd0"


L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Silent-task scope probe.")
s(0)                     # StartRoom: Alpha
ml("You have won.")

# GLOBAL
s("Silent Probe 400")
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

s(5 if MOVES else 3)
obj("coin", room=0)                                        # 0 dynamic
obj("hat",  room=0, wearable=1)                            # 1 dynamic, worn
obj("desk", room=0, static=True, surface=1, capacity=5)    # 2 static surface
if MOVES:
    obj("pebble", room=0)                                  # 3, dynamic 2
    obj("stone", room=0)                                   # 4, dynamic 3

# TASKS -- silent and repeatable
def task(cmd, hide=None):
    s(1); s(cmd)
    s("")                # CompleteText: none
    s("")
    s("")                # RepeatText
    s("")
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand
    s(3)                 # Where: all rooms
    s("")                # Question
    s(0)                 # Restrictions
    if hide is None:
        s(0)             # Actions
    else:
        s(1)
        s(0)             # Type 0 = move object
        s(3 + hide)      # Var1: 3+ = dynamic object
        s(0); s(0)       # Var2 to room, Var3 room 0 = hidden
    s("")                # RestrMask

# --moves: the examine tasks hide the pebble / stone, so a later `look`
# shows whether the silent task matched at all (x it: the "(the coin)" echo).
CELLS = [("frob", None), ("x {the} coin", 2 if MOVES else None),
         ("x {the} hat", 3 if MOVES else None), ("i", None)]
s(len(CELLS))
for cmd, hide in CELLS:
    task(cmd, hide)

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
out = args[0] if args else ("p4SILENT2.plain" if MOVES else "p4SILENT.plain")
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
