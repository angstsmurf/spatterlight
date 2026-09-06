#!/usr/bin/env python3
"""ADRIFT 3.9 probe: the put-family precedence cell, as a native 3.90 file.

The 3.9 twin of `PUT4` + `PUT5` in `make_arena_probe.py`, written to settle
one question: run400 (measured 2026-09-05, Adrift_81/82) lets a put the
library can COMPLETE claim the line even over a matching, PASSING task, and
lets a put it REFUSES on size print the refusal and then fall through to the
task.  The 2026-08-02 run390 record says the opposite for the passing half --
a passing `put * pill in cup` task claimed and the cup stayed empty -- but
that probe used only the wildcard spelling and left SizeMultiple/
WeightMultiple at 0, so the disagreement might have been an artifact rather
than a version split.

Generators only convert UPWARD ([[adrift-generator-upconversion]]), so the
4.00 put5.taf cannot be handed to gen390.  This writes the same game
directly in the 3.9 schema instead; the field layout is lifted verbatim from
`make_39_fwprobe.py`, which is known to load in run390.

Eight objects in four pairs, one question each:

    pill / cup    task `put * pill in cup`     wildcard
    bean / jar    task `put a bean in a jar`   the canonical prefixed form
    coin / box    task `put coin in box`       literal, as typed
    rock / slot   task `put rock in slot`      rock is 27, slot holds 10

All four tasks are unrestricted, so all four PASS.  zzw/zzp/zzl/zzbig are the
aliveness controls (each task's second command), and zzin1-4 report where each
movable actually ended up -- restriction Type 0, Var1 = 3 + the object's
0-based dynamic index, Var2 4 = "inside", Var3 = the 1-based index into the
CONTAINER sublist (cup 1, jar 2, box 3, slot 4).

Dimensions match PUT5/PUT4 exactly: SizeMultiple = WeightMultiple = 3, player
MaxSize = MaxWt = 902, containers capacity 100 (= 10 after unpacking), the
three small movables SizeWeight 0 (size 1, so they fit) and the rock
SizeWeight 30 (size 3**3 = 27, so it does not).

Usage: python3 make_39_putprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("Precedence probe, 3.9."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("PUT39 Probe"); s("SCARE probe"); s("I don't understand.")
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

s(8)
obj("a", "pill", 1)                          # 0 -- Var1 3
obj("a", "cup",  4, container=1, capacity=100, sizeweight=2)   # 1, sublist 1
obj("a", "bean", 1)                          # 2 -- Var1 5
obj("a", "jar",  4, container=1, capacity=100, sizeweight=2)   # 3, sublist 2
obj("a", "coin", 1)                          # 4 -- Var1 7
obj("a", "box",  4, container=1, capacity=100, sizeweight=2)   # 5, sublist 3
obj("a", "rock", 1, sizeweight=30)           # 6 -- Var1 9, size 27
obj("a", "slot", 4, container=1, capacity=100, sizeweight=2)   # 7, sublist 4

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

s(8)
task(["put * pill in cup",   "zzw"],   "WILDPASS.", [])
task(["put a bean in a jar", "zzp"],   "PREFPASS.", [])
task(["put coin in box",     "zzl"],   "LITPASS.",  [])
task(["put rock in slot",    "zzbig"], "PUTBIG.",   [])
task(["zzin1"], "PILL IN CUP.",  [(3, 4, 1, "PILL NOT IN CUP.")])
task(["zzin2"], "BEAN IN JAR.",  [(5, 4, 2, "BEAN NOT IN JAR.")])
task(["zzin3"], "COIN IN BOX.",  [(7, 4, 3, "COIN NOT IN BOX.")])
task(["zzin4"], "ROCK IN SLOT.", [(9, 4, 4, "ROCK NOT IN SLOT.")])

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

out = sys.argv[1] if len(sys.argv) > 1 else "put39.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
