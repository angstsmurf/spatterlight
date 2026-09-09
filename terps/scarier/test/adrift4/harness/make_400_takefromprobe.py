#!/usr/bin/env python3
"""ADRIFT 4.0 probe: the take-from handler's own refusals.

The pre-4.0 Runners answer a `get/take/remove/pick` line that also carries
`from` (or `all`) out of one block -- run390 `insides()` from `loc_4627E2`,
run380 `loc_4468B3` -- whose refusals scarier does not have:

    get all from <noun naming nothing>   You can't get anything from that.
    get <absent noun> from <ditto>       You can't do that!
    get all from <not a container>       You can't take anything from the X!
    get <held thing> from <noun>         The X isn't in or on anything!
    get <thing> from <container>         The X is not inside the box!
    empty <container>                    (NOT a take-from synonym at all)

Three of those literals are absent from run400 (" can't get anything from
that.", " isn't in or on anything!", " can't do that!") and the fourth is
there with a full stop instead of the "!"  -- so every row needs its 4.0
arm measured before any of it can be ported behind a version gate.  This
file is the 4.00 twin of the world `make_39_darkprobe.py` builds, minus the
darkness: one room, a torch, a held lamp, a stone, a pebble, an open
openable box and a coin, and one repeatable `probe` task so the transcript
proves the file is wired.

Usage:
    python3 make_400_takefromprobe.py
    python3 taftool.py pack p4TFROM.taf.plain <donor.taf> p4TFROM.taf
Session (from ~/adrift-battle/runner/wine):
    sh fast.sh p4TFROM.taf cmdfile_p4tfrom.txt run400.exe
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("A synthetic 4.0 take-from probe.")
s(0)                     # StartRoom (0-based)
ml("You have won.")

# GLOBAL
s("Probe 400TFROM")
s("SCARE probe")
s("I don't understand.")  # DontUnderstand
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

# ROOMS -- one, no exits.
s(1)
s("Probe Room")
s("A plain room.")
for _ in range(8):
    s(0)
s(0)                     # Alts
s(0)                     # HideOnMap

# OBJECTS -- all dynamic.  InitialPosition 4 + room is "in room 0", 1 with
# Parent 0 is "held by the player".
def obj(short, desc="A plain thing.", position=4, parent=0,
        container=0, capacity=0, openable=0, prefix="a"):
    s(prefix); s(short)
    s(0)                 # V$Alias count
    s(0)                 # Static
    s(desc)
    s(position)          # InitialPosition
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(container); s(0)   # Container, Surface
    s(capacity)          # Capacity: tens = object count, units = size index
    s(0); s(0)           # Wearable, SizeWeight
    s(parent)            # Parent
    s(openable)          # Openable: 0 = none, 5 = open, 6 = closed
    if openable:
        s(0)             # Key: 0 = none
    s(0)                 # SitLie
    s(0); s(0)           # Edible, Readable
    s(0)                 # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(6)
obj("torch",  "An unlit torch.")                              # 0 in room
obj("lamp",   "A brass lamp.", position=1)                    # 1 held
obj("stone",  "A grey stone.")                                # 2 in room
obj("pebble", "")                                             # 3 no desc
obj("box",    "A wooden box.", container=1, capacity=52,      # 4 open box
    openable=5)
obj("coin",   "A gold coin.")                                 # 5 in room

# TASKS -- one control, repeatable, everywhere.
s(1)
s(1); s("probe")         # V$Command count, then the one command line
s("PROBE OK.")           # CompleteText
s(""); s(""); s("")      # ReverseMessage, RepeatText, AdditionalMessage
s(0)                     # ShowRoomDesc
s(1)                     # Repeatable
s(0)                     # Reversible
s(0)                     # V$ReverseCommand count
s(3)                     # Where: ROOM_LIST0 Type 3 = all rooms
s("")                    # Question
s(0)                     # Restrictions
s(0)                     # Actions
s("")                    # RestrMask

s(0)                     # Events
s(0)                     # NPCs
s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
args = [a for a in sys.argv[1:] if not a.startswith("--")]
out = args[0] if args else "p4TFROM.taf.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
