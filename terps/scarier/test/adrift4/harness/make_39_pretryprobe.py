#!/usr/bin/env python3
"""ADRIFT 3.9 probe: the canonical prefixed retry, take half and put half.

The 3.9 twin of `p4WITHQ2`/`p4WITHQ3` (`make_400_withq2probe.py`), which
closed the 4.0 half of "two-object canonical prefixed retry" on 2026-09-14:
run400 answers `put bean in jar` against a task `put a bean in a jar` with
the library put (its rebuilt line is the DEFINITE "put the bean in the jar",
so the authored-article task never sees it), while `take pebble` against a
task `take a pebble` DOES run the task.  The run390 half was never measured
in one game -- `put39.taf` (Adrift_88) carries the put row alone and has no
take row at all.

Four take cells, so the retry's SHAPE is pinned and not just its presence:

    pebble   task `take a pebble`   typed `take pebble` and `get pebble`
    stone    task `get a stone`     typed `take stone`  and `get stone`

  * a retry built from the TYPED verb + the authored prefix runs the task
    for `take pebble` and `get stone`, and misses the crossed pair;
  * a retry built from the canonical "get" + the prefix runs it for both
    stone lines and neither pebble line;
  * no retry at all misses all four.

Then the two-object half (`put a bean in a jar`, the put39 row again, in the
same session) and the drop twin (`drop a coin`, typed `drop coin` and `put
down coin`).

zzq1-zzq4 report where each movable ended up, so a task that claimed the
line is told apart from a library action that ran: restriction Type 0,
Var1 = 3 + the object's 0-based dynamic index, Var2 1 Var3 0 = "held by the
player", Var2 4 Var3 1 = "inside container sublist entry 1" (the jar).
zzt/zzg/zzp/zzd are the aliveness controls, each task's second command.

Dimensions are put39's: SizeMultiple = WeightMultiple = 3, player MaxSize =
MaxWt = 902, jar capacity 100 (= 10 after unpacking), the movables
SizeWeight 0 (size 1, so they fit).

The 3.9 field layout is lifted verbatim from `make_39_putprobe.py`.

Usage: python3 make_39_pretryprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("Prefixed-retry probe, 3.9."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("PFX39 Probe"); s("SCARE probe"); s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0); s(902); s(902)  # Task Position ParentObject Gender MaxSize MaxWt
# BEightPointCompass bNoDebug BNoScoreNotify BNoMap bNoAutoComplete
# bNoControlPanel bNoMouse BSound BGraphics, then the two scale factors.
for _ in range(9): s(0)
s(3); s(3)                              # SizeMultiple WeightMultiple

# ROOMS
s(1)
s("Test Arena"); s("A bare arena."); s("")
for _ in range(8): s(0)
s(""); s(0); s(""); s(0); s(0); s(""); s(0); s(0)

# OBJECTS
def obj(prefix, short, position, container=0, capacity=0, sizeweight=0):
    s(prefix); s(short); s(short)
    s(0)                                # Static
    s("A probe object.")
    s(position)                         # 1 = held, 4 = room 0
    s(0); s(0); s("")                   # Task TaskNotDone AltDesc
    s(container); s(0); s(capacity)     # Container Surface Capacity
    s(0); s(sizeweight); s(0)           # Wearable SizeWeight Parent
    s(0); s(0); s(0); s(0); s(0)        # Openable SitLie Edible Readable Weapon

s(5)
obj("a", "pebble", 4)                                          # 0 -- Var1 3
obj("a", "jar",    4, container=1, capacity=100, sizeweight=2) # 1, sublist 1
obj("a", "bean",   1)                                          # 2 -- Var1 5
obj("a", "coin",   1)                                          # 3 -- Var1 6
obj("a", "stone",  4)                                          # 4 -- Var1 7

# TASKS
def task(cmds, complete, restrs):
    s(len(cmds) - 1)
    for c in cmds: s(c)
    s(complete); s(""); s(""); s("")
    s(0); s(1); s(0)     # ShowRoomDesc Repeatable Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: all rooms
    s("")                # Question
    s(len(restrs))
    for (v1, v2, v3, fail) in restrs:
        s(0); s(v1); s(v2); s(v3); s(fail)
    s(0)                 # Actions

s(9)
task(["take a pebble",       "zzt"], "TAKEPFX.", [])
task(["get a stone",         "zzg"], "GETPFX.",  [])
task(["put a bean in a jar", "zzp"], "PUTPFX.",  [])
task(["drop a coin",         "zzd"], "DROPPFX.", [])
task(["probe"], "PROBE OK.", [])
task(["zzq1"], "PEBBLE HELD.", [(3, 1, 0, "PEBBLE NOT HELD.")])
task(["zzq2"], "BEAN IN JAR.", [(5, 4, 1, "BEAN NOT IN JAR.")])
task(["zzq3"], "COIN HELD.",   [(6, 1, 0, "COIN NOT HELD.")])
task(["zzq4"], "STONE HELD.",  [(7, 1, 0, "STONE NOT HELD.")])

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

out = sys.argv[1] if len(sys.argv) > 1 else "p39PRETRY.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
