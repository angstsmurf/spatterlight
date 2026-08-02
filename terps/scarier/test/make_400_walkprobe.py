#!/usr/bin/env python3
"""ADRIFT 4.0 twin of make_39_walkprobe.py: NPC-walk CharTask dispatch.

Same layout as the 3.9 probe -- two rooms, NPC Bob walking from room 2 to the
player's room with CharTask = the "#met" task, plus a `*` wildcard task --
but authored against the V400 schema.  Statically, run400's walk handler
(Sub_20_2 at 00068B8E/00068BED) calls the direct by-index task runner
Sub_20_22, so no wildcard interception is expected in the 4.0 Runner,
matching its direct event-task execution.

    A (default)   task 1 = `*`, task 2 = `#met`, CharTask=2 (steal test)
    B (control)   task order swapped, CharTask=1
    H             multi-turn stay: Times = 3 in the player's room, 2 away.
                  Scarier runs the CharTask on every co-located tick (the
                  check sits outside the `start != dest` guard in
                  npc_tick_npc_walk); if run400 fires it only on the arrival
                  turn, that guard is missing.

Output is the PLAIN body only; produce a Runner-valid .taf with:

    python3 make_400_walkprobe.py A p4WKA.plain
    python3 taftool.py pack p4WKA.plain <any valid 4.0 donor.taf> p4WKA.taf

(taftool.py lives in ~/adrift-battle/runner/wine/; the donor supplies the
"Wild" password trailer run400 validates.)

Usage: python3 make_400_walkprobe.py [A|B] [out.plain]
"""
import sys

variant = (sys.argv[1] if len(sys.argv) > 1 else "A").upper()

# v4.0 multiline-string separator (sctafpar.cpp V400_SEPARATOR = bd d0 00).
SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)   # M-multiline: content then separator

# HEADER: MStartupText #StartRoom MWinText
ml("Walk probe.")
s(0)                     # StartRoom (0-based)
ml("You have won.")

# GLOBAL (BattleSystem off -> no BATTLE block)
s("Walk Probe 400")      # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test subject.")     # PlayerDesc
s(0)                     # Task (0 -> no AltDesc)
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(100)                   # MaxSize
s(100)                   # MaxWt
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
# IntroRes, WinRes: nothing (sound+graphics off)
s(0)                     # StatusBox
s("")                    # StatusBoxText
s(0)                     # iUnk1
s(0)                     # iUnk2
s(0)                     # Embedded

# ROOMS -- two rooms, no exits.
def room(short):
    s(short)             # Short
    s("LONG.")           # Long
    for _ in range(8): s(0)  # exits
    # RESOURCE Res: nothing
    s(0)                 # Alts count
    s(0)                 # HideOnMap

s(2)
room("Probe Room")
room("Far Room")

# OBJECTS -- none.
s(0)

# TASKS
def task(cmd, text, restr=None):
    s(1); s(cmd)         # V$Command: count, then count lines
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(3)                 # Where: ROOM_LIST0 Type 3 = all rooms
    s("")                # Question (no hints follow)
    if restr is None:
        s(0)             # Restrictions
    else:
        gate, failmsg = restr
        s(1)             # Restrictions
        s(2)             # type 2: task state
        s(gate)          # Var1: 1-based task index
        s(0)             # Var2: 0 = task must be done (gate never is -> FAIL)
        s(failmsg)       # FailMessage
    s(0)                 # Actions
    s("#" if restr else "")  # RestrMask ("#" = the single restriction)
    # RESOURCE Res: nothing

if variant in ("A", "E"):
    s(2)
    task("*", "WILDCARD FIRED.")
    task("#met", "CHARTASK FIRED.")
    chartask = 2
elif variant in ("B", "F"):
    s(2)
    task("#met", "CHARTASK FIRED.")
    task("*", "WILDCARD FIRED.")
    chartask = 1
elif variant == "G":             # G: restricted #met -- silent skip or loud?
    s(2)
    task("#met", "CHARTASK FIRED.", restr=(2, "METFAIL."))
    task("xyzzygate", "GATE DONE.")
    chartask = 1
else:                            # C/D/H: no wildcard -- walk wiring only
    s(1)
    task("#met", "CHARTASK FIRED.")
    chartask = 1

# D/E/F/G/H: visible looping walk (a 1-stop walk never runs in the Runners).
looped = variant in ("D", "E", "F", "G", "H")

# How many turns the walker stays at each stop.  Only H uses a stay longer
# than one turn -- that is the whole point of H.
times = (3, 2) if variant == "H" else (1, 1)

# EVENTS
s(0)

# NPCS -- Bob, starting in room 2 (1-based), one walk to room 1.
s(1)
s("Bob")                 # Name
s("a")                   # Prefix
s(0)                     # V$Alias count
s("A test NPC.")         # Descr
s(2)                     # StartRoom (1-based; room index 1)
s("")                    # AltText
s(0)                     # Task
s(0)                     # Topics
s(1)                     # Walks
s(2 if looped else 1)    # NumStops
s(1 if looped else 0)    # Loop
s(0)                     # StartTask (0 = start at game start)
s(chartask)              # CharTask (1-based task index of #met)
s(0)                     # MeetObject
s(0)                     # ObjectTask
s(0)                     # StoppingTask
s(0)                     # MeetChar (0 = the player)
s("")                    # ChangedDesc
s(2)                     # Rooms[0]: 0=hidden 1=follow n+2=room n -> room 0
s(times[0])              # Times[0]
if looped:
    s(3)                 # Rooms[1]: room 1 (Far Room)
    s(times[1])          # Times[1]
if looped:
    s(1)                 # ShowEnterExit
    s("BOB ENTERS.")     # EnterText
    s("BOB LEAVES.")     # ExitText
else:
    s(0)                 # ShowEnterExit
s("Bob is standing here." if looped else "")  # InRoomText
s(0)                     # Gender
# [4] RESOURCE Res: nothing

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[2] if len(sys.argv) > 2 else "p4WK%s.plain" % variant
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
