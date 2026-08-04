#!/usr/bin/env python3
"""ADRIFT 4.0 probe: is a task whose Where room-list Type is 0 ("No rooms")
runnable by the player typing its command?

SCARE says no (sctasks.cpp task_can_run_task_directional, ROOMLIST_NO_ROOMS ->
FALSE).  The question came up on *The Plague - Redux*, whose whole combat
system ([F]ight / [E]scape) is authored as Where-type-0 tasks that nothing
ExecTasks -- so on SCARE's reading the fights cannot be entered at all, and the
author's own walkthrough cannot be replayed.  The same game also contains an
obviously-dead duplicate movement task (`* n *` -> room 6, no restrictions)
parked at Where type 0, which would hijack every `n` in the game if type 0 were
runnable.  Both cannot be true; this settles it on the real Runner.

Two rooms, no NPCs, three tasks:

    alpha   Where type 0 (No rooms)    -> "ALPHA FIRED."
    beta    Where type 3 (All rooms)   -> "BETA FIRED."     (control)
    gamma   Where type 1 (room 2 only) -> "GAMMA FIRED."    (control)

Session:  alpha / beta / gamma        (all typed in room 1)

    BETA FIRED.                      always -- proves the probe is wired
    GAMMA FIRED. absent               always -- proves room scoping works
    ALPHA FIRED. present or absent    <- the answer

Output is the PLAIN body only; produce a Runner-valid .taf with:

    python3 make_400_whereprobe.py p4WHERE.plain
    python3 taftool.py pack p4WHERE.plain <any valid 4.0 donor.taf> p4WHERE.taf

(taftool.py lives in ~/adrift-battle/runner/wine/; the donor supplies the
"Wild" password trailer run400 validates.)
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Where probe.")
s(0)                     # StartRoom (0-based)
ml("You have won.")

# GLOBAL
s("Where Probe 400")
s("SCARE probe")
s("I don't understand.")
s(2)                     # Perspective
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player")
s(0)                     # PromptName
s("A test subject.")
s(0); s(0); s(0); s(0)   # Task, Position, ParentObject, PlayerGender
s(100); s(100)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
s(0)                     # StatusBox
s("")                    # StatusBoxText
s(0); s(0)               # iUnk1, iUnk2
s(0)                     # Embedded

# ROOMS -- two, unconnected.
def room(short):
    s(short)
    s("LONG.")
    for _ in range(8):
        s(0)
    s(0)                 # Alts count
    s(0)                 # HideOnMap

s(2)
room("Probe Room")
room("Far Room")

# OBJECTS
s(0)

# TASKS
def task(cmd, text, where_type, where_room=None):
    s(1); s(cmd)         # V$Command
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(where_type)        # Where: ROOM_LIST0 Type
    if where_type == 1:
        s(where_room)    # ... and its single room (0-based)
    s("")                # Question
    s(0)                 # Restrictions
    s(0)                 # Actions
    s("")                # RestrMask

s(3)
task("alpha", "ALPHA FIRED.", 0)
task("beta",  "BETA FIRED.",  3)
task("gamma", "GAMMA FIRED.", 1, 1)

# EVENTS, NPCS
s(0)
s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WHERE.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
