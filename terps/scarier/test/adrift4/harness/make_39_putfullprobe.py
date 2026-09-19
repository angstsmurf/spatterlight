#!/usr/bin/env python3
"""ADRIFT 3.9 probe: the rest of run390 insides()' put arms.

  - " is full." (461E59-461EA7): after the count and its refusals, an
    `inside` put whose container's contents sum (var_D0, taken BEFORE the
    move) equals its capacity overwrites the answer with "<The X> is full.";
  - static targets: a static container or surface here, one elsewhere
    ("can't see" arm 4617FA), a static object as the thing put;
  - an object lying on a floor supporter;
  - `put all in <nothing>`, `drop X in <nothing>`;
  - the onto and-arm with a supporter over capacity (the count at
    461AF8-461BDB adds every candidate for "onto").

Two rooms.  SizeWeight 0 everywhere, so every object is one size unit;
Capacity decodes its FIRST digit (3.9), so 200 holds two, 100 one, 900 nine.

  0 coin, 1 stone   held
  2 plate           dynamic surface on the arena floor
  3 apple           on the plate
  4 bag             dynamic open container, held, capacity 2
  5 gem, 6 ring     held
  7 cupboard        static open container, arena
  8 table           static surface, arena
  9 shelf           static open container, side room
 10 statue          static, arena
 11 crate           dynamic open container, side room floor
 12 tray            dynamic surface, held, capacity 1

Feed: ~/adrift-battle/runner/wine/cmdfile_pputfull.txt.

Usage: python3 make_39_putfullprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("Put full probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("PUTFULL39 Probe"); s("SCARE probe"); s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0); s(902); s(902)  # Task Position ParentObject Gender MaxSize MaxWt
for _ in range(9): s(0)                 # compass..StatusBox flags
s(3); s(3)                              # SizeMultiple WeightMultiple

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
        parent=0):
    s("a"); s(short); s("")
    s(0)                                # Static
    s("A " + short + ".")
    s(position)                         # 1 held, 2 in, 3 on, 4+n room n
    s(0); s(0); s("")                   # Task TaskNotDone AltDesc
    s(container); s(surface); s(capacity)
    s(0); s(0); s(parent)               # Wearable SizeWeight Parent
    s(openable)                         # on disk: 5 shut, 6 open
    s(0); s(0); s(0); s(0)              # SitLie Edible Readable Weapon

def static(short, room_, container=0, surface=0, capacity=0, openable=0):
    s("a"); s(short); s("")
    s(1)                                # Static
    s("A " + short + ".")
    s(0); s(0); s(0); s("")             # InitialPosition Task TaskNotDone AltDesc
    s(1); s(room_)                      # Where: one room (1-based)
    s(container); s(surface); s(capacity)
    s(openable)
    s(0); s(0)                          # SitLie Readable

s(13)
obj("coin", 1)                                          # 0
obj("stone", 1)                                         # 1
obj("plate", 4, surface=1, capacity=900)                # 2, surface 0
obj("apple", 3, parent=0)                               # 3, on the plate
obj("bag", 1, container=1, capacity=200, openable=6)    # 4, container 0
obj("gem", 1)                                           # 5
obj("ring", 1)                                          # 6
static("cupboard", 1, container=1, capacity=900, openable=6)  # 7, container 1
static("table", 1, surface=1, capacity=900)             # 8, surface 1
static("shelf", 2, container=1, capacity=900, openable=6)     # 9, container 2
static("statue", 1)                                     # 10
obj("crate", 5, container=1, capacity=900, openable=6)  # 11, container 3
obj("tray", 1, surface=1, capacity=100)                 # 12, surface 2

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

out = sys.argv[1] if len(sys.argv) > 1 else "pPUTFULL39.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
