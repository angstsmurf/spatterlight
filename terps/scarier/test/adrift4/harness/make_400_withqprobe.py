#!/usr/bin/env python3
"""ADRIFT 4.0 probe: questions that leave the typed line pending.

run400 keeps one pending-question prefix, MemVar_494234; generaltasks
(48AFF3) runs the next line nothing answered as prefix & " " & line, and
48B5FC spends it.  The Wear/Remove what? writers are ported
(lib_question_prefix_from_line).  This probe measures the rest:

  * 48B4E3-48B530, reached by EVERY line, claimed or not: a turn whose
    message is "With what?" or ends in "with?" (Right 5) stores
    prefix = line & " with " AND sets the not-a-turn byte MemVar_494281.
    Authored task text is in scope: Shadowpeak "What with?", Hamper, rking,
    Blood_Relatives, MikeDesert, Hunting Ground, Ghost town, Uncle Grumble.
  * therest's give (488A7C): no held object named -> "Give what?", prefix =
    line; named but no present NPC -> "Give <the obj> to who?", prefix = line.
  * checkverb (4455F8): the bare verb -> "<Verb> what?", prefix = line.

One room, Dave in it; a held coin and knife; static rope and button; a
stone present but not held; a gem in an unreachable second room (never
seen, for therest's " with " split, 4883C5-48860D).  A length-1
self-restarting event prints TICK. on every counted turn.

Usage:
    python3 make_400_withqprobe.py p4WITHQ.plain
    python3 taftool.py pack p4WITHQ.plain p4TAKE.taf p4WITHQ.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("With-question probe.")
s(0)                     # StartRoom
ml("You have won.")

# GLOBAL
s("WithQ Probe 400")
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
s(2)
s("Alpha"); s("The only room.")
for i in range(8):
    s(0)
s(0); s(0)               # Alts, HideOnMap
s("Beta"); s("Unreachable.")
for i in range(8):
    s(0)
s(0); s(0)

# OBJECTS
def obj(short, static=False, prefix="a", position=4):
    s(prefix); s(short)
    s(0)                 # aliases
    s(1 if static else 0)
    s("A plain thing.")
    if static:
        s(0)
    else:
        s(position)      # InitialPosition: 1 held, 4 = room 0
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    if static:
        s(1); s(1)       # Where: room 1
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

s(6)
obj("rope", static=True, prefix="the")
obj("button", static=True, prefix="the")
obj("coin", position=1)
obj("knife", position=1)
obj("stone", position=4)   # present, not held
obj("gem", position=5)     # room Beta: never seen

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

s(8)
task("cut rope", "What do you want to cut it with?")
task("cut rope with knife", "T1 CUT.")
task("saw rope", "What with?")
task("saw rope with knife", "T3 SAWN.")
task("hum", "With what?")
task("hum with knife", "T5 HUMMED.")
task("probe", "PROBE OK.")
task("whittle rope", "Whittle it with what?")

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
s(1)
s("Dave"); s(""); s(0); s("A quiet man.")
s(1)                     # StartRoom
s(""); s(0); s(0); s(0)  # AltText, Task, Topics, Walks
s(1); s("wanders in"); s("wanders off")
s("Dave is here.")
s(0)                     # Gender

s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WITHQ.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
