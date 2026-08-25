#!/usr/bin/env python3
"""ADRIFT 4.0 twin of make_39_examprobe.py: the same world, the 4.00 layout.

run400.exe will NOT load a 3.90 .taf -- it prints "Loading... Incorrect
version." and never starts the game (measured 2026-08-25), so the 3.9 probe
cannot be replayed under 4.0 to get the other side of a version split.  Each
Runner plays only its own file version, which is exactly why scarier keys its
splits on the .taf version and not on anything else.

Same three rooms, same four objects, same control task as the 3.9 probe:

    Test Room   "A bare room."        north -> North Room, east -> Void Room
    North Room  "Another bare room."  south -> Test Room
    Void Room   ""                    west  -> Test Room      <- NO Long

    a stone     no Description, in the Test Room
    a crate     no Description, openable container, in the Test Room
    a coin      "A gold coin.", inside the crate
    a statue    "A marble statue.", in the North Room

The rows this was built to answer, and what it answered.  MEASURED 2026-08-25,
Adrift_1_p4exam.txt, all 32 commands echoed:

    row           3.90 (run390)                | 4.00 (run400)
    x stone       Nothing special.             | You see nothing special about the stone.
    read zzzz     Nothing special.             | You see no such thing.
    open          You can't open that.         | You can't open that.
    open door     You can't open that.         | You can't open that.
    close stone   You can't close the stone.   | You can't close the stone!
    e             There is nothing of interest here.  | (the exits alone)

Four splits, all now ported.  The one row that is NOT a split is `open`: both
Runners say "You can't open that.", so scrunner.cpp's `open *` row was simply
asymmetric with the `close *` row beneath it, and "Open what?" -- which upstream
SCARE printed -- is a sentence no Runner has ever produced.

What this probe leaves open is the 4.0 seen-but-absent resolver, which it also
measured for the first time:

    x statue      You can't see the statue from here!
    open statue   You can't see the statue.      (definite article)
    close statue  You can't see a statue.        (indefinite article!)
    buy statue    You can't see the statue.

FOOTGUN, cost half an hour: **Capacity 99 is invalid** and makes run400 hang for
ever at "Loading...", indistinguishable from a broken feed (measure.sh reports
only "first command never reached the game").  Capacity packs as tens = object
count, units = SIZE INDEX (scobjcts.cpp:674-706), and size index 9 is out of
range; use 52.  Bisect a suspect .taf with loadtest.sh, which prints the window
title -- the title carries the game name only if the file actually loaded.
The same bisect showed p4INON.taf had never once loaded, for the same reason.

Usage:
    python3 make_400_examprobe.py p4EXAM.plain
    python3 taftool.py pack p4EXAM.plain p4STATE.taf p4EXAM.taf
Session:
    sh measure.sh p4EXAM.taf cmdfile_p4exam.txt run400.exe
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("A synthetic 4.0 examine-refusal probe.")
s(0)                     # StartRoom: Test Room (0-based)
ml("You have won.")

# GLOBAL
s("Probe 4EXAM")
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
s(0); s(0)               # Sound, Graphics
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS
def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0)                 # Alts
    s(0)                 # HideOnMap

s(3)
room("Test Room",  "A bare room.",       {0: 2, 1: 3})
room("North Room", "Another bare room.", {2: 1})
room("Void Room",  "",                   {3: 1})

# OBJECTS
def obj(short, desc, position, container=0, capacity=0, openable=0, parent=0):
    s("a")               # Prefix
    s(short)             # Short
    s(0)                 # V$Alias count
    s(0)                 # Static
    s(desc)              # Description
    s(position)          # InitialPosition
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(container); s(0); s(capacity)   # Container, Surface, Capacity
    s(0); s(0); s(parent)             # Wearable, SizeWeight, Parent
    s(openable)          # Openable
    if openable == 6:
        s(0)             # Key
    s(0)                 # SitLie
    s(0); s(0)           # Edible, Readable
    s(0)                 # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(4)
obj("stone",  "",                 4)
obj("crate",  "",                 4, container=1, capacity=52, openable=6)
obj("coin",   "A gold coin.",     2, parent=0)
obj("statue", "A marble statue.", 5)

# TASKS -- one control, so the transcript proves the file is wired.
s(1)
s(1); s("probe")
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

s(0)                     # EVENTS
s(0)                     # NPCS
s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # VARIABLES
s(0)                     # ALRS
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4EXAM.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
