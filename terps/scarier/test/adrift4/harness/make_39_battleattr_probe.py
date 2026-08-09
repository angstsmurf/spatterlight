#!/usr/bin/env python3
"""ADRIFT 3.9 probe: what do the "Change battle attribute" indices mean?

A 3.9 character record holds only Stamina, Strength, Defence (and, for NPCs,
Attitude and Speed) -- there is no Accuracy and no Agility.  So the 3.9
Generator's attribute dropdown must be eight entries, and 4.0's twelve-entry
list is that list with the Accuracy and Agility pairs spliced in:

    3.9  0 Attitude 1 Stamina 2 MaxStamina 3 Strength 4 MaxStrength
         5 Defence  6 MaxDefence  7 Speed
    4.0  0 Attitude 1 Stamina 2 MaxStamina 3 Str 4 MaxStr 5 Acc 6 MaxAcc
         7 Def      8 MaxDef      9 Agi 10 MaxAgi 11 Speed

i.e. 5 -> 7, 6 -> 8, 7 -> 11.  gen400's own conversion agrees on 6 and 7 but
maps 5 to 11 too, which looks like a cascade of un-chained `If`s (5 -> 7, then
that 7 -> 11).  This probe asks run390 directly.

The arena is rigged so the *damage messages* answer the question with no need
to read a status table:

    Player Defence 0, Robot Strength 20 -> every hit takes 20 stamina.
    `zop` adds 40 to attribute 6.  If that is Max Defence, damage is unchanged.
    `zap` adds 40 to attribute 5.  If that is Defence, damage drops to zero and
          the Runner prints "doesn't seem to do any damage".

Suggested session:  wait / wait / zop / wait / wait / zap / wait / wait

See RUNNER_TESTS_TODO.md section 3(a).

Usage: python3 make_39_battleattr_probe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("Battle attribute probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Attr Probe 39")       # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(1)                     # BattleSystem
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test fighter.")     # PlayerDesc
s(0)                     # Task (0 -> no AltDesc)
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(100)                   # MaxSize
s(100)                   # MaxWt
# BATTLE (3.9 player): #Stamina #Strength #Defense
s(500); s(0); s(0)
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

# ROOMS -- one bare arena, no objects to auto-select as a weapon.
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

# TASKS -- two attribute pokes at the player (Var2 = 0 = the player).
def attr_task(cmd, complete, attribute, delta):
    s(0); s(cmd)         # W$Command: count, then count+1 alternatives
    s(complete)          # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(0)                 # Repeatable
    s(0)                 # Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: ROOM_LIST0 Type 3 = all rooms
    s("")                # Question (no hints follow)
    s(0)                 # Restrictions
    s(1)                 # Actions
    s(6)                 # 3.9 action type 6 -> 4.0 type 7, change battle attr
    s(attribute); s(0); s(delta)

s(2)
attr_task("zap", "ZAP -- attribute five plus forty.", 5, 40)
attr_task("zop", "ZOP -- attribute six plus forty.", 6, 40)

# EVENTS
s(0)

# NPCS -- one enemy that hits for a flat 20 every turn.
s(1)
s("Robot")               # Name
s("a")                   # Prefix
s("")                    # [1] Alias
s("A hostile robot.")    # Descr
s(1)                     # StartRoom (1-based)
s("")                    # AltText
s(0)                     # Task
s(0)                     # Topics
s(0)                     # Walks
s(0)                     # ShowEnterExit
s("Robot is here.")      # InRoomText
s(0)                     # Gender
# NPC_BATTLE (3.9): #Attitude #Stamina #Strength #Defense #Speed #KilledTask
s(2); s(9999); s(20); s(0); s(1); s(0)

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

out = sys.argv[1] if len(sys.argv) > 1 else "pATTR.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
