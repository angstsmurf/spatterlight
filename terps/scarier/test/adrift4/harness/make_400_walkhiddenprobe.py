#!/usr/bin/env python3
"""ADRIFT 4.0 probe: what makes a walker's arrival announcement survive when it
comes out of "nowhere"?

The Runner's arrival gate is (run400 @468A5D, and the same shape in run380
@4416F4 / run390 loc_45A99B)

    ShowEnterExit AND old <> playerroom AND old <> 0

-- so an NPC arriving from location 0, the "never placed" room number, gets no
line.  The Skydiver (4.00) contradicts that at first sight: its Pelican has
StartRoom 0 and its only walk opens with a Hidden stop, and run400's transcript
of the wired walkthrough still prints

    Pelican A pelican flocked toward me..

on the turn the bird arrives.  The reading that fits is that the Hidden stop
does not merely leave the NPC nowhere -- it *stamps* the location field with
the &HFF the Runner uses for a walk-hidden walker (run400 loc_468D4A), and it
does so unconditionally, not only when the walker had somewhere to leave.
&HFF is not 0, so the gate passes on the next arrival.  Scarier stamps its
equivalent marker only inside its "did the NPC actually move" branch, so a
walker that was ALREADY nowhere when the Hidden stop came round never gets the
stamp and its arrival stays silent.

Three cells, all in one game, all watched from Alpha, which the player never
leaves.  Every NPC has ShowEnterExit on, EnterText "wanders in", ExitText
"wanders off".

    Hid   StartRoom 0 (nowhere).  Game-start walk, 2 stops: Hidden 3, then
          Alpha 3.  The Skydiver's exact shape: it is nowhere already when the
          Hidden stop fires, so the stamp -- if there is one -- is the only
          thing that can carry it past the gate four turns later.
    Nev   StartRoom 0 (nowhere).  One stop, Alpha 3, started by the task `go`.
          Never hidden by a walk, so it arrives with a genuine 0 in the field:
          the control that says whether "old <> 0" is really in the gate.
    Base  StartRoom Bravo.  One stop, Alpha 3, started by the task `go2`.  A
          real room to leave, so this one must announce -- the baseline that
          proves the probe's ShowEnterExit wiring works at all.

Reading it: Hid announcing and Nev silent confirms the stamp.  Both announcing
says the gate has no "old <> 0" term for a walk arrival and the Skydiver row is
something else.  Neither announcing says the Hidden stop is not what carries
it and the Pelican's line comes from somewhere else entirely.

Usage:
    python3 make_400_walkhiddenprobe.py p4WKHID.plain
    python3 taftool.py pack p4WKHID.plain <donor.taf> p4WKHID.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Walk hidden-stamp probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Walk Hidden Probe 400")
s("SCARE probe")
s("I don't understand.")
s(1)                     # Perspective: second person
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player"); s(0); s("A test subject.")
s(0)                     # Task
s(0); s(0); s(0)         # Position, ParentObject, PlayerGender
s(100); s(100)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0); s(0); s(0)         # NoDebug, NoScoreNotify, NoMap
s(0); s(0); s(0)         # NoAutoComplete, NoControlPanel, NoMouse
s(0); s(0)               # Sound, Graphics -- both off, so no resource fields
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS -- Alpha east to Bravo, Bravo west to Alpha.  Exit order is
# north, east, south, west, up, down, in, out; a zero means no exit, and a
# real one is Dest (1-based) Var1 Var2 Var3.
def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0)                 # Alts
    s(0)                 # HideOnMap

s(2)
room("Alpha", "The first room.", {1: 2})
room("Bravo", "The second room.", {3: 1})

# OBJECTS -- none needed.
s(0)

# TASKS -- the two walk starters, and nothing else.
def task(cmd, text):
    s(1); s(cmd)
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(3)                 # Where: all rooms
    s("")                # Question
    s(0)                 # Restrictions
    s(0)                 # Actions
    s("")                # RestrMask

s(2)
task("go", "GO.")        # task 1 (1-based): starts Nev's walk
task("go2", "GO2.")      # task 2 (1-based): starts Base's walk

# EVENTS -- none.
s(0)

# NPCS.  A walk stop's Rooms value is 0 = Hidden, 1 = follow the player,
# and 2 + the 0-based room for a fixed room.
def npc(name, startroom, walks):
    s(name)              # Name
    s("")                # Prefix
    s(0)                 # V$Alias count
    s("A patient walker.")   # Descr
    s(startroom)         # StartRoom (1-based; 0 = nowhere)
    s("")                # AltText
    s(0)                 # Task
    s(0)                 # Topics
    s(len(walks))        # Walks
    for stops, starttask in walks:
        s(len(stops))    # NumStops
        s(0)             # Loop
        s(starttask)     # StartTask
        s(0)             # CharTask
        s(0)             # MeetObject
        s(0)             # ObjectTask
        s(0)             # StoppingTask
        s(0)             # MeetChar
        s("")            # ChangedDesc
        for dest, times in stops:
            s(dest); s(times)
    s(1)                 # ShowEnterExit
    s("wanders in")      # EnterText
    s("wanders off")     # ExitText
    s(name + " is here.")    # InRoomText
    s(0)                 # Gender
    # [4] Res: nothing, sound and graphics both being off.

HIDDEN = 0
ALPHA = 2
BRAVO = 3

s(3)
npc("Hid",  0, [([(HIDDEN, 3), (ALPHA, 3)], 0)])
npc("Nev",  0, [([(ALPHA, 3)], 1)])
npc("Base", 2, [([(ALPHA, 3)], 2)])

s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES -- none.
s(0)

s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WKHID.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
