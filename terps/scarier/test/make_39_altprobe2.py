#!/usr/bin/env python3
"""ADRIFT 3.9 room-alt probe 2: alts with no gating task and no gating object.

parse_fixup_v390_v380_room_alts() only synthesises a task alt when Task1/Task2
is non-zero, and an object alt when Obj is non-zero.  gen400's own 3.9 -> 4.0
conversion synthesises them anyway, with Var2/Var3 left at zero, whenever the
description text is non-empty.  This probe asks the real 3.9 Runner which is
right.  See RUNNER_TESTS_TODO.md section 3(a).

    Room 0: AddDesc1 = "ADD1." with Task1 = 0     (no gating task)
    Room 1: AltDesc  = "ALTD." with Obj  = 0      (no gating object)

Both rooms have a Long and a LastDesc, so each of the three texts is
individually identifiable in the transcript.

    look        -> which of LONG0 / ADD1. / LAST0. appear?
    north
    look        -> which of LONG1 / ALTD. / LAST1. appear?

Usage: python3 make_39_altprobe2.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("Alt probe 2."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Alt Probe 39 II")     # GameName
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
s(0)                     # iUnk1
s(0)                     # iUnk2

# ROOMS.  Exit order for the 4-point compass is N E S W U D In Out; only the
# first (north) is used, and only from room 0.
def room(short, long_, lastdesc, north, add1, task1, add2, task2,
         obj, altdesc, typehide):
    s(short); s(long_); s(lastdesc)
    for i in range(8):
        if i == 0 and north:
            s(north); s(0); s(0)         # Dest Var1 Var2 (Var3 is defaulted)
        else:
            s(0)
    s(add1); s(task1); s(add2); s(task2)
    s(obj); s(altdesc); s(typehide)
    s(0)                                 # HideOnMap

s(2)
room("Room Zero", "LONG0.", "LAST0.", north=2,
     add1="ADD1.", task1=0, add2="", task2=0,
     obj=0, altdesc="", typehide=0)
room("Room One", "LONG1.", "LAST1.", north=0,
     add1="", task1=0, add2="", task2=0,
     obj=0, altdesc="ALTD.", typehide=0)

# OBJECTS, TASKS, EVENTS, NPCS -- none.
s(0); s(0); s(0); s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate
s("    Wild    ")        # sPassword: pw[0:4]+"Wild"+pw[4:8]

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "pALT2.taf"
open(out, "wb").write(SIG + obf)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
