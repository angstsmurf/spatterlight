#!/usr/bin/env python3
"""ADRIFT 4.0 probe: five unmeasured corners from the "Still open" list.

  * `open X with Y` / `unlock X with Y` on a LOCKED box whose key is the
    coin (therest's open arm 48880F tests only the word; does a locked box
    with a key reach the lock code instead?).
  * A tie inside either with-half: two stones, `cut stone with knife` and
    `cut rope with stone`.
  * put's first noun: a seen-but-absent first noun (`put gem in jar`), and an
    unknown first noun with a seen-absent, ambiguous second (`put zzz in
    bag`, red and blue bags in Beta).
  * kiss's third buffer arm (47F81A): the reply is overwritten when the
    buffer holds " can't see " -- `kiss dave` with Dave seen and absent, and
    `kiss ed` with Ed never seen.
  * A mutual ALR pair, AAA -> BBB and BBB -> AAA (4.0 repeats the pass until
    nothing changes; Scarier's loop bound is a guard, not a model).

Rooms Alpha (north -> Beta) and Beta (south -> Alpha); Gamma is unreachable.
A length-1 self-restarting event prints TICK. on every counted turn.

Usage:
    python3 make_400_lockprobe.py p4LOCK.plain
    python3 taftool.py pack p4LOCK.plain p4TAKE.taf p4LOCK.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Lock probe.")
s(0)                     # StartRoom
ml("You have won.")

# GLOBAL
s("Lock Probe 400")
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

s(3)
room("Alpha", "The first room.", {0: 2})
room("Beta", "The second room.", {2: 1})
room("Gamma", "Unreachable.", {})

# OBJECTS
def obj(short, static=False, prefix="a", position=4, room=1,
        container=0, openable=0, key=-1, capacity=22):
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
    s(container); s(0); s(capacity if container else 0)
    if not static:
        s(0); s(0); s(0) # Wearable, SizeWeight, Parent
    s(openable)          # Openable: 0 none, 5 open, 6 closed, 7 locked
    if openable:
        s(key)           # Key: dynamic-object index, -1 none
    s(0)                 # SitLie
    if not static:
        s(0)             # Edible
    s(0)                 # Readable
    if not static:
        s(0)             # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

# Dynamic indices (the Key field counts these): knife 0, coin 1, red stone 2,
# blue stone 3, gem 4, red bag 5, blue bag 6.  The coin is deliberately NOT
# dynamic #0 (run400 prints no put confirmation for dynamic object #1).
s(10)
obj("rope", static=True, prefix="the")                              # knife..
obj("knife", position=1)                                            # dyn 0
obj("coin", position=1)                                             # dyn 1
obj("box", static=True, prefix="the", container=1, openable=7, key=1)
obj("red stone", position=4)                                        # dyn 2
obj("blue stone", position=4)                                       # dyn 3
obj("gem", position=5)                                              # dyn 4
obj("red bag", position=5, container=1)                             # dyn 5
obj("blue bag", position=5, container=1)                            # dyn 6
obj("jar", static=True, container=1)

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

s(2)
task("probe", "PROBE OK.")
task("ping", "AAA.")

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

# NPCS
def npc(name, room, desc, gender):
    s(name); s(""); s(0); s(desc)
    s(room)              # StartRoom, 1-based
    s(""); s(0); s(0); s(0)  # AltText, Task, Topics, Walks
    s(1); s("wanders in"); s("wanders off")
    s("%s is here." % name)
    s(gender)            # Gender: 0 male, 1 female, 2 neuter

s(2)
npc("Dave", 2, "A quiet man.", 0)
npc("Ed", 3, "A hidden man.", 0)

s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(2)                     # ALRs: a mutual pair
s("AAA"); s("BBB")
s("BBB"); s("AAA")
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4LOCK.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
