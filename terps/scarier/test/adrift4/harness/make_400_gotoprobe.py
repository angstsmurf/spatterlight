#!/usr/bin/env python3
"""ADRIFT 4.0 goto probe: gotoplace() (run400 464998) and its route finder
Proc_21_9_465C14.  4.0 differs from 3.9 in one way on paper: both the room
names it matches and the rooms a route runs through must have been visited
(the room record's global_84).

    First Room -N- Hall -E- Kitchen
                    |-N- Garden   (exit usable only while `probe` is NOT done)
                    |-W- Red Hall
                    |-U- Blue Hall
    Tower (no exits)

Usage:
    python3 make_400_gotoprobe.py p4GOTO.plain
    python3 taftool.py pack p4GOTO.plain <any valid 4.0 donor.taf> p4GOTO.taf
Session:
    cmdfile_p4goto.txt under run400x.exe
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Goto probe.")
s(0)                     # StartRoom (0-based)
ml("You have won.")

# GLOBAL
s("Probe 4GOTO")
s("SCARE probe")
s("NO IDEA.")
s(2)                     # Perspective
s(1)                     # ShowExits
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

# ROOMS.  A slot is 0 for no exit, else Dest (1-based) Var1 Var2 Var3.
def room(short, long_, exits=()):
    s(short)
    s(long_)
    for slot in range(8):            # N E S W up down in out
        dest = dict(exits).get(slot)
        if dest is None:
            s(0)
        elif isinstance(dest, tuple):
            s(dest[0]); s(dest[1]); s(dest[2]); s(0)
        else:
            s(dest); s(0); s(0); s(0)
    s(0)                 # Alts count
    s(0)                 # HideOnMap

s(7)
room("First Room", "The first room.", {0: 2})
room("Hall", "A hall.", {2: 1, 1: 3, 0: (4, 1, 1), 3: 5, 4: 6})
room("Kitchen", "A kitchen.", {3: 2})
room("Garden", "A garden.", {2: 2})
room("Red Hall", "A red hall.", {1: 2})
room("Blue Hall", "A blue hall.", {5: 2})
room("Tower", "A tower.")

# OBJECTS
s(0)

# TASKS -- one control.
s(1)
s(1); s("probe")         # V$Command
s("PROBE OK.")           # CompleteText
s("")                    # ReverseMessage
s("")                    # RepeatText
s("")                    # AdditionalMessage
s(0)                     # ShowRoomDesc
s(1)                     # Repeatable
s(0)                     # Reversible
s(0)                     # V$ReverseCommand count
s(3)                     # Where: all rooms
s("")                    # Question
s(0)                     # Restrictions
s(0)                     # Actions
s("")                    # RestrMask

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
out = sys.argv[1] if len(sys.argv) > 1 else "p4GOTO.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
