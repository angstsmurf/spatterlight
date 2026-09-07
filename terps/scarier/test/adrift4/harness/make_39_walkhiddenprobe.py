#!/usr/bin/env python3
"""ADRIFT 3.9 probe: does a Hidden walk stop stamp the location field here too?

The 3.9 twin of make_400_walkhiddenprobe.py, which measured under run400 that a
walker sitting at the "never placed" location 0 when a Hidden stop comes round
still comes out of that stop able to announce its next arrival -- the Hidden
branch writes the &HFF marker whether or not the walker had anywhere to leave
(run400 loc_468D4A), and the arrival gate's "old <> 0" term then passes.

run390's arrival gate is the same shape (loc_45A99B: ShowEnterExit AND old <>
playerroom AND old <> 0) and its Hidden stamp sits inside the same counter ==
suffix-sum test (loc_45ABB8), so the 4.0 finding ought to carry -- but 3.9
differs from 4.0 in the neighbouring branch already (it has no hidden-departure
line at all, run390's Hidden branch being nothing but the stamp), so measure it
rather than assume.  ALEXIS.TAF is the corpus row that moves with the answer,
and it cannot be measured directly: it is seeded.

Three cells, all watched from Alpha, which the player never leaves.  Each
walk is started by its own task, because a NON-LOOPING game-start walk never
runs at all before 4.0 (see npc_start_walk_is_390_noop in scnpcs.cpp).

    Hid   StartRoom 0 (nowhere).  Two stops, Hidden 3 then Alpha 3, started
          by `go0`: nowhere already when the Hidden stop fires, so the stamp
          is the only thing that can carry it past the gate three turns later.
    Nev   StartRoom 0 (nowhere).  One stop, Alpha 3, started by `go1`.  Never
          hidden by a walk, so it arrives on a genuine 0 -- the control for
          "old <> 0" being in the gate at all.
    Base  StartRoom Bravo.  One stop, Alpha 3, started by `go2`.  A real room
          to leave: the baseline that must announce.

Session:  go0 / z / z / z / look / go1 / look / go2 / look

Usage: python3 make_39_walkhiddenprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("Walk hidden-stamp probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Walk Hidden Probe 39")
s("SCARE probe")
s("I don't understand.")
s(2)                     # Perspective
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player"); s(0); s("A test subject.")
s(0)                     # Task (0 -> no AltDesc)
s(0); s(0); s(0)         # Position, ParentObject, PlayerGender
s(100); s(100)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0); s(0); s(0)         # bNoDebug, NoScoreNotify, NoMap
s(0); s(0); s(0)         # bNoAutoComplete, bNoControlPanel, bNoMouse
s(0); s(0)               # Sound, Graphics
s(0); s(0)               # iUnk1, iUnk2

# ROOMS -- Alpha east to Bravo, Bravo west to Alpha, so Base's arrival can be
# given a direction the way the 4.0 probe's is.
def room(short, exits=()):
    s(short); s("LONG."); s("")
    for slot in range(8):
        dest = dict(exits).get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0)
    s(""); s(0); s(""); s(0)   # AddDesc1, Task1, AddDesc2, Task2
    s(0); s("")                # Obj, AltDesc
    s(0); s(0)                 # TypeHideObjects, HideOnMap

s(2)
room("Alpha", {1: 2})      # east -> Bravo
room("Bravo", {3: 1})      # west -> Alpha

# OBJECTS -- none.
s(0)

# TASKS -- the three walk starters, and nothing else.
def task(cmd, text):
    s(0); s(cmd)         # W$Command: count, then count+1 alternatives
    s(text)              # CompleteText
    s(""); s(""); s("")  # ReverseMessage, RepeatText, AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: all rooms
    s("")                # Question (no hints follow)
    s(0)                 # Restrictions
    s(0)                 # Actions

s(3)
task("go0", "GO0.")
task("go1", "GO1.")
task("go2", "GO2.")

# EVENTS -- none.
s(0)

# NPCS.  A walk stop's Rooms value is 0 = Hidden, 1 = follow the player, and
# 2 + the 0-based room for a fixed room.
def npc(name, startroom, stops, starttask):
    s(name)              # Name
    s("")                # Prefix
    s("")                # [1]$Alias
    s("A patient walker.")   # Descr
    s(startroom)         # StartRoom (1-based; 0 = nowhere)
    s("")                # AltText
    s(0)                 # Task
    s(0)                 # Topics
    s(1)                 # Walks
    s(len(stops))        # NumStops
    s(0)                 # Loop
    s(starttask)         # StartTask
    s(0)                 # CharTask
    s(0)                 # MeetObject
    s(0)                 # ObjectTask
    s(0)                 # StoppingTask
    s("")                # ChangedDesc  (MeetChar is not stored in 3.9)
    for dest, times in stops:
        s(dest); s(times)
    s(1)                 # ShowEnterExit
    s("wanders in")      # EnterText
    s("wanders off")     # ExitText
    s(name + " is here.")    # InRoomText
    s(0)                 # Gender

HIDDEN = 0
ALPHA = 2
BRAVO = 3

s(3)
npc("Hid",  0, [(HIDDEN, 3), (ALPHA, 3)], 1)
npc("Nev",  0, [(ALPHA, 3)], 2)
npc("Base", 2, [(ALPHA, 3)], 3)

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

out = sys.argv[1] if len(sys.argv) > 1 else "p39WKHID.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
