#!/usr/bin/env python3
"""ADRIFT 4.0 probe: `put X in/on Y` when Y, or X, or both name nothing.

Main Course's `Adrift_35` probe showed run400 answering `put zzz in yyy` with
"I don't understand what you want to put things inside." where Scarier says
"It is not clear which object you are referring to."  The p-code (put_drop_list
Proc_19_40_459DB4 -> name_object Proc_19_41_46E5D8) resolves the CONTAINER
phrase first, and its three exits do not all look alike:

  container names nothing      -> "I don't understand what you want to put
                                   things inside." / "... onto."  (not a turn)
  present, but not a container -> "<You> can't put anything inside <the X>!"
                                  (a turn)
  `on` spelling, nothing found -> "Where do you want to put <the X>?" /
                                  "... put that?"                 (not a turn)
  container found, object not  -> "It is not clear which object you are
                                   referring to." / "Drop what?"

This probe measures every cell against a held coin, a hat (dynamic, not a
container), a static surface (desk), a present open container (box), a
seen-but-absent container (crate, in Bravo), an NPC (Bob) and two unknown
words.  No tasks at all, so nothing prematches.  A length-1 self-restarting
event prints `TICK.` on every counted turn.

A second cell set (`--v2`, p4PUT2) adds a dynamic container listed in
Bravo (bag), so it is seen the way a room listing makes an object seen, and
two same-named static containers in Alpha (jar, jar) for the tie.

Usage:
    python3 make_400_putprobe.py p4PUT.plain
    python3 make_400_putprobe.py --v2 p4PUT2.plain
    python3 taftool.py pack p4PUT.plain <donor.taf> p4PUT.taf
    (then, from ~/adrift-battle/runner/wine)
    sh fast.sh p4PUT.taf cmdfile_put.txt run400
"""
import sys

V2 = "--v2" in sys.argv[1:]

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Put-in-unknown-container probe.")
s(0)                     # StartRoom: Alpha
ml("You have won.")

# GLOBAL
s("Put Probe 400")
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
        container=0, surface=0, capacity=0, wearable=0):
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
    s(container); s(surface); s(capacity)
    if not static:
        s(wearable); s(0); s(0)          # Wearable, SizeWeight, Parent
    s(0)                 # Openable (0: a container is simply open)
    s(0)                 # SitLie
    if not static:
        s(0)             # Edible
    s(0)                 # Readable
    if not static:
        s(0)             # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(8 if V2 else 5)
obj("coin",  room=0)                                          # 0 dynamic
obj("hat",   room=0, wearable=1)                              # 1 dynamic
obj("desk",  room=0, static=True, surface=1, capacity=5)      # 2 surface
obj("box",   room=0, static=True, container=1, capacity=5)    # 3 container
obj("crate", room=1, static=True, container=1, capacity=5)    # 4 in Bravo
if V2:
    obj("bag", room=1, container=1, capacity=5)               # 5 dynamic, Bravo
    obj("jar", room=0, static=True, container=1, capacity=5)  # 6 tie
    obj("jar", room=0, static=True, container=1, capacity=5)  # 7 tie

# TASKS: none
s(0)

# EVENTS -- length 1, restarts immediately: FinishText marks every counted turn.
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
out = args[0] if args else ("p4PUT2.plain" if V2 else "p4PUT.plain")
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
