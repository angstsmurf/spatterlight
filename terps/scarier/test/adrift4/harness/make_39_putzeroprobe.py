#!/usr/bin/env python3
"""ADRIFT 3.9 probe: the size-0 object, and run390's " is full." arm.

The last open arm of run390 insides()' put (WINE-TRANSCRIPTS-TODO.md,
"Put: the ' is full.' arm at 461E59"):

    If var_D0 = capacity(36) And flag(29) = 1 And var_E0 = "inside" Then
        msg = <The X> & " is full."

var_D0 is the size already inside the target, taken BEFORE the move, and
the test sits AFTER the count pass -- so it speaks only for a container
that is exactly full and still has something that fits.  pPUTFULL39 could
not raise it: every object there is one size unit, and an exactly full bag
answers "Nothing will fit inside the bag." from the count instead.

A size-0 object is the only way in, and the only way to a size-0 object is
the global SizeMultiple.  Size is multiple^(SizeWeight \\ 10), so with
SizeMultiple = 0 a SizeWeight of 0 is still 1 (0^0) and a SizeWeight of 10
is 0.  scobjcts.cpp's obj_scale() has said so since it was written -- "that
is not a special case worth defending against, since it is precisely what
the Runner computes" -- with nothing measured behind it, so this probe
proves the decode as well as the arm.

MaxSize/MaxWt are 900 and the bag's Capacity 200: a units digit of 0 makes
the limit multiple^0 = 1 times its count, so the player still carries 90
size units and the bag still holds 2 with the size scale collapsed.

    0 coin     held   SizeWeight 0   size 1
    1 stone    held   SizeWeight 0   size 1
    2 feather  held   SizeWeight 10  size 0
    3 mote     held   SizeWeight 10  size 0
    4 bag      held   container, capacity 2, open
    5 pebble   held   SizeWeight 0   size 1
    6 twig     held   SizeWeight 0   size 1
    7 speck    held   SizeWeight 10  size 0
    8 plate    held   surface, capacity 2   -- the "onto" control, which
                                               the arm's third test bars

Feed: ~/adrift-battle/runner/wine/cmdfile_pputzero.txt.

Usage: python3 make_39_putzeroprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("Put zero probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL -- SizeMultiple 0 is the whole point; WeightMultiple stays 3.
s("PUTZERO39 Probe"); s("SCARE probe"); s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0); s(900); s(900)  # Task Position ParentObject Gender MaxSize MaxWt
for _ in range(9): s(0)                 # compass..StatusBox flags
s(0); s(3)                              # SizeMultiple WeightMultiple

# ROOMS
def room(short, long_, exits):
    s(short); s(long_); s("")
    for slot in range(8):               # N E S W up down in out
        dest = exits.get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0)
    s(""); s(0); s(""); s(0); s(0); s(""); s(0)
    s(0)                                # HideOnMap

s(2)
room("Test Arena", "A bare arena.", {0: 2})
room("Side Room", "A side room.", {2: 1})

# OBJECTS
def obj(short, position, container=0, surface=0, capacity=0, openable=0,
        parent=0, sizeweight=0):
    s("a"); s(short); s("")
    s(0)                                # Static
    s("A " + short + ".")
    s(position)                         # 1 held, 2 in, 3 on, 4+n room n
    s(0); s(0); s("")                   # Task TaskNotDone AltDesc
    s(container); s(surface); s(capacity)
    s(0); s(sizeweight); s(parent)      # Wearable SizeWeight Parent
    s(openable)                         # on disk: 5 shut, 6 open
    s(0); s(0); s(0); s(0)              # SitLie Edible Readable Weapon

s(9)
obj("coin", 1)                                          # 0
obj("stone", 1)                                         # 1
obj("feather", 1, sizeweight=10)                        # 2
obj("mote", 1, sizeweight=10)                           # 3
obj("bag", 1, container=1, capacity=200, openable=6)    # 4, container 0
obj("pebble", 1)                                        # 5
obj("twig", 1)                                          # 6
obj("speck", 1, sizeweight=10)                          # 7
obj("plate", 1, surface=1, capacity=200)                # 8, surface 0

# TASKS
s(1)
s(0); s("probe")
s("PROBE OK."); s(""); s(""); s("")
s(0); s(1); s(0)     # ShowRoomDesc Repeatable Reversible
s(0); s("")          # W$ReverseCommand
s(3)                 # Where: all rooms
s("")                # Question
s(0)                 # Restrictions
s(0)                 # Actions

# EVENTS, NPCS, tail
s(0)
s(0)
s(0); s(0); s(0); s(0); s(0)
s("2026")
s("    Wild    ")

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "pPUTZERO39.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
