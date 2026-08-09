#!/usr/bin/env python3
"""ADRIFT 3.9 room-alt probe 3: object alt versus task alt priority.

The room has both an object-conditioned AltDesc ("OBJALT.", condition "player
isn't holding the rock", which is true from the start) and a task-gated
AddDesc1 ("ADDDESC1.", gated on `xyzzy`).  Once both conditions hold, exactly
one of them wins, and which one tells us where the object alt belongs in the
converted 4.0 alt array: the object alt is a DisplayRoom=0 "override all
others" alt, so if it wins it replaces the room's Long description too.

    look     -> LONG. + OBJALT. or just OBJALT. (task not yet done)
    xyzzy
    look     -> OBJALT.    => object alt outranks the task alt (goes last)
                ADDDESC1.  => task alt outranks the object alt (goes first)

See RUNNER_TESTS_TODO.md section 3(a).

Usage: python3 make_39_altprobe3.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("Alt probe 3."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Alt Probe 39 III")    # GameName
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

# ROOMS -- one room.
s(1)
s("Probe Room")          # Short
s("LONG.")               # Long
s("LASTDESC.")           # LastDesc
for _ in range(8): s(0)  # exits (4-point compass -> 8 slots)
s("ADDDESC1.")           # AddDesc1
s(1)                     # Task1 (1-based -> task 0, "xyzzy")
s("")                    # AddDesc2
s(0)                     # Task2
s(1)                     # Obj (1-based dynamic object -> the rock)
s("OBJALT.")             # AltDesc
s(0)                     # TypeHideObjects: 0 -> condition 0 ("isn't
                         #   holding"), hide-objects 0
s(0)                     # HideOnMap

# OBJECTS -- one dynamic object, never held by the player.
s(1)
s("a")                   # Prefix
s("rock")                # Short
s("rock")                # [1]$Alias
s(0)                     # Static
s("A grey rock.")        # Description
s(0)                     # InitialPosition (hidden)
s(0)                     # Task
s(0)                     # TaskNotDone
s("")                    # AltDesc
s(0)                     # Container
s(0)                     # Surface
s(0)                     # Capacity
s(0)                     # Wearable
s(0)                     # SizeWeight
s(0)                     # Parent
s(0)                     # Openable
s(0)                     # SitLie
s(0)                     # Edible
s(0)                     # Readable
s(0)                     # Weapon

# TASKS -- one bare magic word.
s(1)
s(0); s("xyzzy")         # W$Command: count, then count+1 alternatives
s("Xyzzy!")              # CompleteText
s("")                    # ReverseMessage
s("")                    # RepeatText
s("")                    # AdditionalMessage
s(0)                     # ShowRoomDesc
s(0)                     # Repeatable
s(0)                     # Reversible
s(0); s("")              # W$ReverseCommand
s(3)                     # Where: ROOM_LIST0 Type 3 = all rooms
s("")                    # Question (no hints follow)
s(0)                     # Restrictions
s(0)                     # Actions

# EVENTS, NPCS
s(0); s(0)

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

out = sys.argv[1] if len(sys.argv) > 1 else "pALT3.taf"
open(out, "wb").write(SIG + obf)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
