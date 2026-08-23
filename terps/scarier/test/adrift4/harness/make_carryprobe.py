#!/usr/bin/env python3
"""Carried-total (`count`) accounting probe for run400.

The question this settles: which position changes move the Runner's running
Size/Weight totals, and by how much.

run400 keeps two counters on the player record (global_36 = size, global_38 =
weight) and `count` merely prints them.  They are maintained incrementally by
scattered write sites -- the loader (Proc_19_5), get (Proc_19_6), drop
(Proc_19_7), the task mover (Proc_19_10) and battle (Proc_11_1) -- with no
recompute anywhere, so the two numbers are whatever that bookkeeping has
accumulated.  Replaying provenance's 650-command walkthrough left Scarier at
Size 76 / Weight 92 where run400 reports Size -60 / Weight 89, and a linear
solve over the nine position-change classes the walkthrough exercises admits
66 different rule sets -- the walkthrough simply does not separate them.

So separate them by hand: every class gets its own command, with `count` after
each one, and the deltas are read straight off the transcript.  Objects have
distinct size and weight indices so a delta names its object unambiguously.

    name    size  weight   role
    bag        9       9   carriable container
    pack       3       9   carriable WEARABLE container
    tray       9       9   carriable surface
    hat        9       3   wearable
    rock      27       3
    coin       1      27
    pin        1       1
    box       81       9   container left on the floor
    slab      81       9   surface left on the floor

Player MaxSize/MaxWt are cranked to the editor maximum so no carry gate ever
refuses and every command lands.

Usage:
    python3 make_carryprobe.py pcarry400.plain
    python3 taftool.py pack pcarry400.plain <donor.taf> pcarry400.taf
"""
import sys

MAXSIZE = MAXWT = 994
PARENT = -1
SIZE_MULTIPLE = WEIGHT_MULTIPLE = 3
SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x):
    L.append(x)
    L.append(SEP)

# ---------------------------------------------------------------- HEADER
ml("Carried-total accounting probe.")
s(0)                          # StartRoom
ml("You have won.")

# ---------------------------------------------------------------- GLOBAL
s("Carry Probe 400")
s("SCARE probe")
s("I don't understand.")
s(2)                          # Perspective
s(0)                          # ShowExits
s(0)                          # WaitTurns
s(1)                          # DispFirstRoom
s(0)                          # BattleSystem
s(0)                          # MaxScore
s("Player")
s(0)                          # PromptName
s("A test subject.")
s(0)                          # Task
s(0)                          # Position
s(0)                          # ParentObject
s(0)                          # PlayerGender
s(MAXSIZE)
s(MAXWT)
s(0)                          # EightPointCompass
s(0)                          # bNoDebug
s(0)                          # NoScoreNotify
s(0)                          # NoMap
s(0)                          # bNoAutoComplete
s(0)                          # bNoControlPanel
s(0)                          # bNoMouse
s(0)                          # Sound
s(0)                          # Graphics
s(0)                          # StatusBox
s("")                         # StatusBoxText
s(SIZE_MULTIPLE)              # iUnk1
s(WEIGHT_MULTIPLE)            # iUnk2
s(1)                          # Embedded

# ---------------------------------------------------------------- ROOMS
s(1)
s("Test Room")
s("A bare room.")
for _ in range(8):
    s(0)
s(0)                          # Alts count
s(0)                          # HideOnMap

# ---------------------------------------------------------------- OBJECTS
# (name, container, surface, wearable, capacity, sizeweight)
OBJECTS = [
    ("dummy", 0, 0, 0,  0, 22),   # index 0 is unusable -- never referenced
    ("bag",   1, 0, 0, 92, 22),   # size 9  weight 9
    ("pack",  1, 0, 1, 92, 12),   # size 3  weight 9, wearable container
    ("tray",  0, 1, 0, 92, 22),   # size 9  weight 9
    ("hat",   0, 0, 1,  0, 21),   # size 9  weight 3
    ("rock",  0, 0, 0,  0, 31),   # size 27 weight 3
    ("coin",  0, 0, 0,  0,  3),   # size 1  weight 27
    ("pin",   0, 0, 0,  0,  0),   # size 1  weight 1
    ("box",   1, 0, 0, 92, 42),   # size 81 -- stays on the floor
    ("slab",  0, 1, 0, 92, 42),   # size 81 -- stays on the floor
]

s(len(OBJECTS))
for name, container, surface, wearable, capacity, sizeweight in OBJECTS:
    s("a")                    # Prefix
    s(name)                   # Short
    s(0)                      # V$Alias count
    s(0)                      # Static
    s("A probe %s." % name)   # Description
    s(4)                      # InitialPosition: 4 + room -> room 0
    s(0)                      # Task
    s(0)                      # TaskNotDone
    s("")                     # AltDesc
    s(container)
    s(surface)
    s(capacity)
    s(wearable)
    s(sizeweight)
    s(PARENT)
    s(0)                      # Openable (always accessible)
    s(0)                      # SitLie
    s(0)                      # Edible
    s(0)                      # Readable
    s(0)                      # Weapon
    s(0)                      # CurrentState
    s(0)                      # ListFlag
    s("")                     # InRoomDesc
    s(0)                      # OnlyWhenNotMoved

# ---------------------------------------------------------------- rest
s(0)                          # Tasks
s(0)                          # Events
s(0)                          # NPCs
s(0)                          # RoomGroups
s(0)                          # Synonyms
s(0)                          # Variables
s(0)                          # ALRs
s(0)                          # CustomFont
s("2026")                     # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "pcarry400.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
