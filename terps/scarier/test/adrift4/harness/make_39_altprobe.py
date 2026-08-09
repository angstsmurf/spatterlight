#!/usr/bin/env python3
"""Minimal ADRIFT 3.9 room-description-alt probe.

Answers: when a 3.9 room has BOTH a task-gated AddDesc and a LastDesc, does
the real 3.9 Runner print the AddDesc *instead of* LastDesc, or *as well as*?

Scarier's parse_fixup_v390_v380_room_alts() synthesises 4.0 alts in the order
  [Obj alt] [Task2 alt] [Task1 alt] [LastDesc catch-all]
with the catch-all last and DisplayRoom=2, which makes it print unconditionally
alongside whichever task alt fired.  gen400's own 3.9 -> 4.0 conversion emits
the reverse order (catch-all first, then Task1, then Task2), which under
lib_find_starting_alt()'s backwards scan makes the task alts *suppress* the
catch-all.  See RUNNER_TESTS_TODO.md section 3(a).

The probe: one room whose Long is empty, AddDesc1 = "ADDDESC1." gated on a
task the player fires with `xyzzy`, and LastDesc = "LASTDESC.".  Run it
in run390:

    look                 -> LASTDESC. only (task undone)
    xyzzy
    look                 -> ADDDESC1. only     => catch-all is suppressed
                            ADDDESC1. LASTDESC. => catch-all always prints

Usage: python3 make_39_altprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("Alt-ordering probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Alt Probe 39")       # GameName
s("SCARE probe")        # GameAuthor
s("I don't understand.")# DontUnderstand
s(2)                    # Perspective
s(0)                    # ShowExits
s(0)                    # WaitTurns
s(1)                    # DispFirstRoom
s(0)                    # BattleSystem
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
# BattleSystem off -> no <BATTLE>
s(0)                    # EightPointCompass
s(0)                    # bNoDebug
s(0)                    # NoScoreNotify
s(0)                    # NoMap
s(0)                    # bNoAutoComplete
s(0)                    # bNoControlPanel
s(0)                    # bNoMouse
s(0)                    # Sound
s(0)                    # Graphics
s(0)                    # iUnk1
s(0)                    # iUnk2

# ROOMS -- one room, empty Long so the alt texts stand alone.
s(1)
s("Probe Room")         # Short
s("")                   # Long
s("LASTDESC.")          # LastDesc
for _ in range(8): s(0) # exits (8-point compass off)
s("ADDDESC1.")          # AddDesc1
s(1)                    # Task1 (1-based -> task 0, "xyzzy")
s("ADDDESC2.")          # AddDesc2
s(2)                    # Task2 (1-based -> task 1, "plugh")
s(0)                    # Obj (no object-gated alt)
s("")                   # AltDesc
s(0)                    # TypeHideObjects
s(0)                    # HideOnMap

# OBJECTS -- none; the probe tasks are bare magic words.
s(0)

# TASKS -- two, each a bare command with no restrictions or actions.
s(2)
for cmd, done in (("xyzzy", "Xyzzy!"),
                  ("plugh",   "Plugh!")):
    s(0); s(cmd)        # W$Command: count, then count+1 alternatives
    s(done)             # CompleteText
    s("")               # ReverseMessage
    s("")               # RepeatText
    s("")               # AdditionalMessage
    s(0)                # ShowRoomDesc
    s(0)                # Repeatable
    s(0)                # Reversible
    s(0); s("")         # W$ReverseCommand (count 0 -> one empty alternative)
    s(3)                # Where: ROOM_LIST0 Type 3 = all rooms
    s("")               # Question (no hints follow)
    s(0)                # Restrictions
    s(0)                # Actions

# EVENTS, NPCS
s(0); s(0)

# tail
s(0)                    # RoomGroups
s(0)                    # Synonyms
s(0)                    # Variables
s(0)                    # ALRs
s(0)                    # CustomFont
s("2026")               # CompileDate
s("    Wild    ")       # sPassword: pw[0:4]+"Wild"+pw[4:8]

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "pALT.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
