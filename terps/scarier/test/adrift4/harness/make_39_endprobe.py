#!/usr/bin/env python3
"""ADRIFT 3.9 probe: what does the Runner print when an EndGame action fires?

The 4.0 answer was measured with the `SC` arena config in make_arena_probe.py
(see RUNNER_TESTS_TODO.md section 4, "End-of-game score summary"): run400 adds
a two- or three-line score summary that Scarier prints for no ending at all.
A UTF-16LE census of the four Runner binaries says the summary strings exist in
run370/380/390 too, so the port must not be gated on 4.0 -- but "the strings are
present" is not "this code path prints them", and in 3.7/3.9 the summary pair
sits in a *different* literal block from the endgame command's own copy.  This
probe asks run390 directly.

3.9 task actions are the 4.0 ones with every Type > 4 shifted down by one
(sctafpar.cpp, |V390_TASK_ACTION:Type>4?#Type++|), and the 3.9 EndGame action
carries only Var1 (Var2/Var3 are not stored):

    type 4, Var1 = n     -> score + n
    type 5, Var1 = 0..3  -> end game: win / lose / dead / just stop

MaxScore is 8 and every ending task scores 3 first, so the percentage line has
a truncating case to show (3/8 = 37.5% -> "37%") and "points short" has a
non-zero value.  `scpoint` scores without ending, to separate "never notifies"
from "does not notify on the final turn".

Suggested session -- ONE ending per Runner launch:

    scpoint / score / scwin        (and again with sclose, scdead, scend)

Usage: python3 make_39_endprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("A synthetic 3.9 endgame probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39END")         # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(1)                     # Perspective (second person, as the 4.0 SC probe)
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(8)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test player.")      # PlayerDesc
s(0)                     # Task (0 -> no AltDesc)
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(100)                   # MaxSize
s(100)                   # MaxWt
# no BATTLE block: BattleSystem is off
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

# ROOMS -- one bare arena.
s(1)
s("Test Arena")          # Short
s("A bare arena.")       # Long
s("")                    # LastDesc
for _ in range(8): s(0)  # exits
s("")                    # AddDesc1
s(0)                     # Task1
s("")                    # AddDesc2
s(0)                     # Task2
s(0)                     # Obj
s("")                    # AltDesc
s(0)                     # TypeHideObjects
s(0)                     # HideOnMap

# OBJECTS -- none.
s(0)

# TASKS
def task(cmd, complete, actions):
    s(0); s(cmd)         # W$Command: count, then count+1 alternatives
    s(complete)          # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: ROOM_LIST0 Type 3 = all rooms
    s("")                # Question (no hints follow)
    s(0)                 # Restrictions
    s(len(actions))      # Actions
    for a in actions:
        for f in a: s(f)

SCORE3 = (4, 3)          # type 4: score + 3
s(5)
task("scpoint", "END scpoint.", [SCORE3])
task("scwin",   "END scwin.",   [SCORE3, (5, 0)])
task("sclose",  "END sclose.",  [SCORE3, (5, 1)])
task("scdead",  "END scdead.",  [SCORE3, (5, 2)])
task("scend",   "END scend.",   [SCORE3, (5, 3)])

# EVENTS
s(0)

# NPCS
s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate
s("    Wild    ")        # sPassword: pw[0:4]+"Wild"+pw[4:8], Runner checks Mid(5,4)

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

# VB PRNG stream, skipping the 14 signature draws
state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p39end.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
