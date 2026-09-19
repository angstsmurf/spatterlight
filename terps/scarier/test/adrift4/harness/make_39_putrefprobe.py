#!/usr/bin/env python3
"""ADRIFT 3.9 probe: do insides()'s container refusals answer before a
matching task, as its count<2 refusal does?

run390 insides() (460EDC) settles the container before its task look-up at
461A6C.  With two objects named it refuses, with no checktask of its own:

  - no named object after the preposition (var_8C = -1): "You can't put
    anything inside that!" / "onto that!" (461769), straight to 4624BF;
  - a target of the wrong kind for the preposition, or a shut container:
    "You can't put anything inside <the X>." / "... as it is closed!"
    (4623F8-46249E, the var_CA = 0 arm);

and then runs the post-put sweep (462550), whose claimant runs the tasks
QUIET.  So a matching FAILING task should leave the refusal standing, and a
matching PASSING one should replace it.

All objects dynamic, one room, everything held except gem and ring, which
lie in the open bag.  Restriction of every failing task: the pebble inside
the box, never true.

    T1 `put coin in lamp`            lamp no container        T1 FAIL.
    T2 `put coin in box`             box shut                 T2 FAIL.
    T3 `put coin on box`             box no surface           T3 FAIL.
    T4 `put coin and stone in junk`  nothing after "in"       T4 FAIL.
    T5 `put stone in lamp`           PASSING control          T5 PASS.
    T6 `put coin in bag`             bag FULL, task fails     T6 FAIL.
    T7 `put gem in ring`             no claimant (both in the bag) T7 FAIL.
    T8 `put pebble and stone in junk`   nothing after "in"    T8 FAIL.
    T9 `put pebble and stone on junk`   nothing after "on"    T9 FAIL.
    T10 `put stone in box`           box shut, PASSING task   T10 PASS.

T4's line is not the case it was written for: the Runner finds the
preposition with InStr(line, "in"), which hits the "in" inside "coin", so the
stone counts as named after it and becomes the target.  T8/T9 avoid it.

T6 is a capacity case, not a claim: 3.9 reads only the first digit of
Capacity's count, so the bag's 100 holds one size-1 object and the gem and
ring have filled it.  The capacity refusal moves nothing, so no sweep runs
and the LOUD pass's FailMessage replaces it (Adrift_pputrefv4.txt; see
make_39_putclmprobe.py for puts that fit).  The scale factors are 3, as the
editor writes them.

Feeds: cmdfile_pputref.txt (Adrift_pputref39.txt) and cmdfile_pputref2.txt
in ~/adrift-battle/runner/wine/.

Usage: python3 make_39_putrefprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("Put refusal probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("PUTREF39 Probe"); s("SCARE probe"); s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0); s(902); s(902)  # Task Position ParentObject Gender MaxSize MaxWt
for _ in range(9): s(0)                 # compass..StatusBox flags
s(3); s(3)                              # SizeMultiple WeightMultiple

# ROOMS
s(1)
s("Test Arena"); s("A bare arena."); s("")
for _ in range(8): s(0)
s(""); s(0); s(""); s(0); s(0); s(""); s(0); s(0)

# OBJECTS
def obj(short, position, container=0, surface=0, capacity=0, openable=0,
        parent=0):
    s("a"); s(short); s("")
    s(0)                                # Static
    s("A " + short + ".")
    s(position)                         # 1 held, 2 in container, 4 room 0
    s(0); s(0); s("")                   # Task TaskNotDone AltDesc
    s(container); s(surface); s(capacity)
    s(0); s(0); s(parent)               # Wearable SizeWeight Parent
    s(openable)                         # on disk: 5 shut, 6 open
    s(0); s(0); s(0); s(0)              # SitLie Edible Readable Weapon

s(9)
obj("coin", 1)                                          # 0
obj("stone", 1)                                         # 1
obj("lamp", 1)                                          # 2
obj("box", 1, container=1, capacity=100, openable=5)    # 3, container 1
obj("tray", 1, surface=1, capacity=100)                 # 4
obj("pebble", 1)                                        # 5
obj("bag", 1, container=1, capacity=100, openable=6)    # 6, container 2
obj("gem", 2, parent=1)                                 # 7, in the bag
obj("ring", 2, parent=1)                                # 8, in the bag

# TASKS
NEVER = [(3 + 5, 4, 1, None)]           # pebble inside box
def task(cmds, complete, fail=None):
    s(len(cmds) - 1)
    for c in cmds: s(c)
    s(complete); s(""); s(""); s("")
    s(0); s(1); s(0)     # ShowRoomDesc Repeatable Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: all rooms
    s("")                # Question
    if fail is None:
        s(0)
    else:
        s(1)
        s(0); s(3 + 5); s(4); s(1); s(fail)
    s(0)                 # Actions

s(11)
task(["probe"], "PROBE OK.")
task(["put coin in lamp"], "T1 PASS.", "T1 FAIL.")
task(["put coin in box"], "T2 PASS.", "T2 FAIL.")
task(["put coin on box"], "T3 PASS.", "T3 FAIL.")
task(["put coin and stone in junk"], "T4 PASS.", "T4 FAIL.")
task(["put stone in lamp"], "T5 PASS.")
task(["put coin in bag"], "T6 PASS.", "T6 FAIL.")
task(["put gem in ring"], "T7 PASS.", "T7 FAIL.")
task(["put pebble and stone in junk"], "T8 PASS.", "T8 FAIL.")
task(["put pebble and stone on junk"], "T9 PASS.", "T9 FAIL.")
task(["put stone in box"], "T10 PASS.")

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

out = sys.argv[1] if len(sys.argv) > 1 else "pPUTREF39.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
