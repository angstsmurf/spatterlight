#!/usr/bin/env python3
"""ADRIFT 4.0 twin of make_39_darkprobe.py: the same dark cave, the 4.00 layout.

The question this answers: does run400 have darkness at all?  Every pre-4.0
Runner recomputes one predicate at the top of examines() -- "the room's object
condition holds and its Hide objects box is ticked" -- into a byte it then
consults twice (run390 44B888, run380 43C708, run370 434E95), and p39DARK
measured all three of its answers under run390.  run400 kept the shape, but
the decompile shows Proc_19_87_471F94's dark byte is never assigned, so
scarier gates lib_room_object_alt_fires() to < 4.0 (library/sclibrar_print.cpp)
on the strength of the P-code alone.  This probe is the live measurement.

run400.exe will not load a 3.90 .taf ("Loading... Incorrect version."), so
the 3.9 file cannot simply be replayed; this is the same world rebuilt in the
4.0 record layout.  The one structural difference is the darkness itself:
4.0 has no room-level Obj/AltDesc/TypeHideObjects triple, only room alts, so
the Dark Cave carries a single alt of the shape parse_fixup_v390_v380_room_alt()
synthesises from the 3.9 triple -- Type 2 (player condition), Var2 0 ("player
isn't holding"), Var3 1 (the torch, 1-based over the dynamic objects),
HideObjects 1, DisplayRoom 0 (override everything), M1 "It is too dark to
see."  Scarier reads both files into the same alt, so any difference between
the two Runners' answers is the Runner's, not the data's.

The world, as in p39DARK: a Lit Room holding an unlit torch, north to a Dark
Cave that is dark until the torch is taken.  The player starts holding a brass
lamp; the cave holds a stone with a description, a pebble without one, and an
open box with a coin in it.

The rows, and what each Runner answers in the dark if it has darkness
(the run390 answers, runner_probes/dark.run390.{unseen,seen}.txt):

    x stone        "You can't see the stone very clearly."      (44BC37)
    x zzzz         "You can't see that very clearly."           (44C477)
    x me           "You can just make out that you are okay."   (44C430)
    look           "It is too dark to see." and no object list

and the same rows with the torch held, as their own controls.  If run400
answers the dark rows like the lit ones, the dead byte is confirmed.

Measured 2026-11-04 (runner_probes/dark.run400.{unseen,seen}.txt): it is.
No "can't see" answer appears.  But the alt still hides the objects from
the noun resolver, as run390's does: entering unlit, `x stone`, `x pebble`,
`x box` and `read stone` are "You see no such thing.", `take stone` is "Take
what?" and `get all from box` is "I don't understand where you want to get
things from."  After one lit visit, `take stone` works in the dark and `x
stone` is "A grey stone."; `get all from box` before `x box` is "Take
what?", since the coin was never listed.

Feeds (CRLF; same commands as the 3.9 ones, so the transcripts line up):

    cmdfile_p4dark.txt   = cmdfile_p39dark.txt
    cmdfile_p4dark2.txt  = cmdfile_p39dark2.txt

`probe` is a repeatable no-restriction task that must answer "PROBE OK." at
both ends, proving the file is wired; check that every command echoed before
believing any of it.

FOOTGUN (make_400_examprobe.py): Capacity 99 hangs run400 at "Loading...";
the box uses 52, as there.

Usage:
    python3 make_400_darkprobe.py p4DARK.plain
    python3 taftool.py pack p4DARK.plain p4BATT.taf p4DARK.taf
Session:
    sh fast.sh p4DARK.taf cmdfile_p4dark.txt run400.exe
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("A synthetic 4.0 darkness probe.")
s(0)                     # StartRoom: Lit Room (0-based)
ml("You have won.")

# GLOBAL
s("Probe 4DARK")
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
s(102); s(102)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0); s(0); s(0)         # NoDebug, NoScoreNotify, NoMap
s(0); s(0); s(0)         # NoAutoComplete, NoControlPanel, NoMouse
s(0); s(0)               # Sound, Graphics
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS.  An alt is "$M1 #Type $M2 #Var2 #HideObjects $Changed #Var3
# #DisplayRoom" with no resources (Sound and Graphics are off).
def room(short, long_, exits, alts=()):
    s(short); s(long_)
    for i in range(8):   # N E S W up down in out
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(len(alts))
    for m1, type_, var2, hide, var3, display in alts:
        s(m1); s(type_)
        s(""); s(var2)   # M2, Var2
        s(hide); s("")   # HideObjects, Changed
        s(var3); s(display)
    s(0)                 # HideOnMap

s(2)
room("Lit Room", "A bright room.", {0: 2})
room("Dark Cave", "A cave with rough walls.", {2: 1},
     alts=[("It is too dark to see.", 2, 0, 1, 1, 0)])

# OBJECTS -- all dynamic, so the dynamic index is the list order:
# torch 1, lamp 2, stone 3, pebble 4, box 5, coin 6 (1-based, as Var3 counts).
def obj(prefix, short, desc, position, container=0, capacity=0, openable=0,
        parent=0, sizeweight=0):
    s(prefix)            # Prefix
    s(short)             # Short
    s(0)                 # V$Alias count
    s(0)                 # Static
    s(desc)              # Description
    s(position)          # InitialPosition: 1 held, 2 in container,
                         #   4 + room index in a room
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(container); s(0); s(capacity)   # Container, Surface, Capacity
    s(0); s(sizeweight); s(parent)    # Wearable, SizeWeight, Parent
    s(openable)          # Openable: 4.0 stores 5 open, 6 closed, 7 locked
    if openable in (5, 6, 7):  # (3.9/3.8 swap 5 and 6 on disk)
        s(-1)            # Key: none
    s(0)                 # SitLie
    s(0); s(0)           # Edible, Readable
    s(0)                 # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(6)
obj("an", "torch",  "An unlit torch.", 4)                    # the light switch
obj("a",  "lamp",   "A brass lamp.",   1)                    # held from turn 1
obj("a",  "stone",  "A grey stone.",   5)                    # in the cave
obj("a",  "pebble", "",                5)                    # no description
obj("a",  "box",    "A wooden box.",   5, container=1,       # open, non-empty
    capacity=52, openable=5, sizeweight=21)
obj("a",  "coin",   "A gold coin.",    2, parent=0)          # inside the box

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
out = sys.argv[1] if len(sys.argv) > 1 else "p4DARK.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
