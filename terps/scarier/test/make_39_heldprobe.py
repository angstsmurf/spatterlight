#!/usr/bin/env python3
"""Minimal ADRIFT 3.9 probe for "object held by the player" through a container.

One room, one openable container ("box", starts held by the player and OPEN),
one loose "key", and one repeatable task `probe` whose sole restriction is
"key (dynamic object 1) is held by the player" -- Type 0, Var1 4, Var2 1,
Var3 0.  CompleteText and FailMessage report the verdict, so a session reads:

    probe                 -> NOT HELD   (key on the floor)
    take key / put key in box / probe   -> ?   (open container, carried)
    close box / probe     -> ?   (CLOSED container, carried -- the question)
    drop box / probe      -> ?   (closed container, on the floor)

Placement is done by the parser at run time rather than by InitialPosition, so
nothing here depends on how either engine reads the container-sublist index.

Emits both the obfuscated .taf (for run390) and the plain body (debug)."""
import sys

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("A synthetic 3.9 held-by-player probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39HELD")       # GameName
s("SCARE probe")        # GameAuthor
s("I don't understand.")# DontUnderstand
s(2)                    # Perspective
s(1)                    # ShowExits
s(0)                    # WaitTurns
s(1)                    # DispFirstRoom
s(1)                    # BattleSystem
s(0)                    # MaxScore
s("Player")             # PlayerName
s(0)                    # PromptName
s("A test subject.")    # PlayerDesc
s(0)                    # Task (0 -> no AltDesc)
s(0)                    # Position
s(0)                    # ParentObject
s(0)                    # PlayerGender
s(100)                  # MaxSize
s(100)                  # MaxWt
# BATTLE (3.9 player): #Stamina #Strength #Defense
s(100); s(10); s(0)
s(0)                    # EightPointCompass
s(0)                    # bNoDebug (ignored, consumed)
s(0)                    # NoScoreNotify
s(0)                    # NoMap
s(0)                    # bNoAutoComplete
s(0)                    # bNoControlPanel
s(0)                    # bNoMouse
s(0)                    # Sound
s(0)                    # Graphics
# IntroRes WinRes: nothing (sound+graphics off); FStatusBox EStatusBoxText: no lines
s(0)                    # iUnk1
s(0)                    # iUnk2
# FEmbedded: no line

# ROOMS
s(1)
s("Test Room")          # Short
s("A bare room.")       # Long
s("")                   # LastDesc
for _ in range(8): s(0) # exits (compass off -> 8)
s("")                   # AddDesc1
s(0)                    # Task1
s("")                   # AddDesc2
s(0)                    # Task2
s(0)                    # Obj
s("")                   # AltDesc
s(0)                    # TypeHideObjects
# Res/LastRes/Task1Res/Task2Res/AltRes: nothing; NoMap==0 -> BHideOnMap
s(0)                    # HideOnMap

# OBJECTS
s(2)

# --- object 0 (dynamic 0): the box, an open container held by the player
s("a")                  # Prefix
s("box")                # Short
s("")                   # [1] Alias
s(0)                    # Static (dynamic)
s("A wooden box.")      # Description
s(1)                    # InitialPosition (1 = held), Parent 0 = by player
s(0)                    # Task
s(0)                    # TaskNotDone
s("")                   # AltDesc
s(1)                    # Container
s(0)                    # Surface
s(52)                   # Capacity (as inverness' own riddle box)
s(0)                    # Wearable
s(21)                   # SizeWeight (as inverness' riddle box; run390
                        # refuses "put key in box" for a size-0 container)
s(0)                    # Parent
s(6)                    # Openable (on-disk 6 <-> internal 5 = OPEN)
s(0)                    # SitLie
s(0)                    # Edible
s(0)                    # Readable
s(0)                    # Weapon
s(0); s(0); s(0)        # OBJ_BATTLE: Protection Hit Method

# --- object 1 (dynamic 1): the key, loose in room 1
s("a")                  # Prefix
s("key")                # Short
s("")                   # [1] Alias
s(0)                    # Static (dynamic)
s("A small brass key.") # Description
s(2)                    # InitialPosition (2 = inside a container)
s(0)                    # Task
s(0)                    # TaskNotDone
s("")                   # AltDesc
s(0)                    # Container
s(0)                    # Surface
s(0)                    # Capacity
s(0)                    # Wearable
s(0)                    # SizeWeight
s(0)                    # Parent (container sublist index -> the box)
s(0)                    # Openable
s(0)                    # SitLie
s(0)                    # Edible
s(0)                    # Readable
s(0)                    # Weapon
s(0); s(0); s(0)        # OBJ_BATTLE

# TASKS
s(1)
s(0)                    # W$Command: count 0 -> count+1 = 1 command
s("probe")
s("RESULT: KEY IS HELD.")       # CompleteText
s("")                   # ReverseMessage
s("")                   # RepeatText (must stay empty: a non-empty
                        # RepeatText makes even a repeatable task un-rerunnable)
s("")                   # AdditionalMessage
s(0)                    # ShowRoomDesc
s(1)                    # Repeatable
s(0)                    # Reversible
s(0); s("")             # W$ReverseCommand: count 0 -> 1 string
s(3)                    # Where: all rooms
s("")                   # Question
s(1)                    # Restrictions
s(0)                    #   Type 0 = object location
s(4)                    #   Var1 = 3 + dynamic index 1 (the key)
s(1)                    #   Var2 = 1 (held by)
s(0)                    #   Var3 = 0 (the player)
s("RESULT: KEY IS NOT HELD.")   #   FailMessage
s(0)                    # Actions
# RestrMask is a parse fixup in 3.9 (no input field); Res: nothing

# EVENTS
s(0)

# NPCS
s(0)

# tail
s(0)                    # RoomGroups
s(0)                    # Synonyms
s(0)                    # Variables
s(0)                    # ALRs
s(0)                    # CustomFont
s("2026")               # CompileDate
s("    Wild    ")       # sPassword: pw[0:4]+"Wild"+pw[4:8], Runner checks Mid(5,4)

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

out = sys.argv[1] if len(sys.argv) > 1 else "p39held.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
