#!/usr/bin/env python3
"""ADRIFT 4.0 probe: the corners p4WITHQ left of therest's " with " split,
plus three put/take leads from the same notes section.

  * An instrument or object that is SEEN but ABSENT.  463640 (mode 0) scores
    present+seen objects first and falls back to seen-only, so therest's
    split should resolve the gem after the player has visited Beta and come
    back.  The "With what?" arm at 488505 tests not-present AND not-seen,
    which 463640 never returns -- so the arm reads as dead code.
  * `open`/`close X with Y` on an openable X (closed box, open chest).
  * The verbs of run400's with-suffix list that Scarier sends to handlers
    with no with-split: read, fix, repair, mend, feel, clear; and the
    lib_cant_do_other ones never probed one by one.
  * Two-object prefixed task retry: task `put a bean in a jar`, typed
    `put bean in jar`; single-object: task `take a pebble`, typed
    `take pebble`.
  * `put all in X` with nothing carried.

Rooms Alpha (north -> Beta) and Beta (south -> Alpha).  A length-1
self-restarting event prints TICK. on every counted turn.

Capacity is packed count*10 + size class, so the jar's 5 holds nothing and
the bean is "too big" (Adrift_1159).  A second argument sets it; p4WITHQ3 is
the same game with 22 (two class-2 items), for the two-object retry
(cmdfile_withq5.txt).  run400 will not load 99: "[Overflow,6,9]".

Usage:
    python3 make_400_withq2probe.py p4WITHQ2.plain
    python3 make_400_withq2probe.py p4WITHQ3.plain 22
    python3 taftool.py pack p4WITHQ2.plain p4TAKE.taf p4WITHQ2.taf
"""
import sys

JAR = int(sys.argv[2]) if len(sys.argv) > 2 else 5

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("With-question probe 2.")
s(0)                     # StartRoom
ml("You have won.")

# GLOBAL
s("WithQ2 Probe 400")
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

# ROOMS -- exits: 8 slots (N E S W up down in out), 0 or Dest Var1 Var2 Var3.
def room(short, long_, exits):
    s(short); s(long_)
    for slot in range(8):
        dest = exits.get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0); s(0)
    s(0); s(0)           # Alts, HideOnMap

s(2)
room("Alpha", "The first room.", {0: 2})
room("Beta", "The second room.", {2: 1})

# OBJECTS
def obj(short, static=False, prefix="a", position=4, room=1,
        container=0, openable=0, readtext=None, capacity=5):
    s(prefix); s(short)
    s(0)                 # aliases
    s(1 if static else 0)
    s("A plain thing.")
    if static:
        s(0)
    else:
        s(position)      # InitialPosition: 1 held, 4 = room 0, 5 = room 1
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    if static:
        s(1); s(room)    # Where: one room, 1-based
    s(container); s(0); s(capacity if container else 0)  # Container, Surface, Capacity
    if not static:
        s(0); s(0); s(0) # Wearable, SizeWeight, Parent
    s(openable)          # Openable: 0 none, 5 open, 6 closed
    if openable:
        s(0)             # Key
    s(0)                 # SitLie
    if not static:
        s(0)             # Edible
    if readtext is None:
        s(0)             # Readable
    else:
        s(1); s(readtext)
    if not static:
        s(0)             # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(12)
obj("rope", static=True, prefix="the")                          # 0
obj("button", static=True, prefix="the")                        # 1
obj("coin", position=1)                                         # 2 held
obj("knife", position=1)                                        # 3 held
obj("stone", position=4)                                        # 4 Alpha
obj("gem", position=5)                                          # 5 Beta
obj("box", static=True, prefix="the", container=1, openable=6)  # 6 closed
obj("chest", static=True, prefix="the", container=1, openable=5)  # 7 open
obj("book", static=True, prefix="the", readtext="BOOK TEXT.")   # 8
obj("pebble", position=4)                                       # 9 Alpha
obj("bean", position=1)                                         # 10 held
obj("jar", static=True, container=1, capacity=JAR)              # 11 open

# TASKS
def task(cmd, text):
    s(1); s(cmd)
    s(text)
    s(""); s(""); s("")  # Reverse/Repeat/Additional
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand
    s(3)                 # Where: anywhere
    s("")                # Question
    s(0)                 # Restrictions
    s(0)                 # Actions
    s("")                # RestrMask

s(3)
task("probe", "PROBE OK.")
task("put a bean in a jar", "T PUTPREF.")
task("take a pebble", "T TAKEPREF.")

# EVENTS -- length 1, restarts immediately: FinishText marks every counted turn.
s(1)
s("Ticker")
s(1)                     # StarterType: immediate
s(1)                     # RestartType: restart immediately
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

s(0)                     # NPCs
s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WITHQ2.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
