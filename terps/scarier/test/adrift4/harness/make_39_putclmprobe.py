#!/usr/bin/env python3
"""ADRIFT 3.9 probe: does a matching FAILING task claim a put the library
could complete?

No.  Every container is open, held and empty, and each put line has a
matching task whose restriction (the pebble inside the shut box) fails:

    C1 `put coin in bag`        literal command
    C2 `* stone * jar`          wildcard
    C3 `put lamp in *`          trailing wildcard
    C4 `put gem in pot`         literal, EMPTY FailMessage
    C5 `put %object% in pan`    %object% reference

run390x, 2026-09-19 (Adrift_pputclm39.txt): every one answers "You put the X
inside the Y." and the object moves; no FailMessage prints.  (pPUTREF39's
"T6 FAIL." was a capacity refusal: see make_39_putrefprobe.py.)  The scale
factors are left at 0 here, which is harmless for empty containers.

Feed: cmdfile_pputclm.txt in ~/adrift-battle/runner/wine/.

Usage: python3 make_39_putclmprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("Put refusal probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("PUTCLM39 Probe"); s("SCARE probe"); s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0); s(902); s(902)  # Task Position ParentObject Gender MaxSize MaxWt
for _ in range(11): s(0)                # compass..scale factors

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

s(12)
obj("coin", 1)                                          # 0
obj("stone", 1)                                         # 1
obj("lamp", 1)                                          # 2
obj("box", 1, container=1, capacity=100, openable=5)    # 3, container 1
obj("gem", 1)                                           # 4
obj("pebble", 1)                                        # 5
obj("bag", 1, container=1, capacity=100, openable=6)    # 6
obj("jar", 1, container=1, capacity=100, openable=6)    # 7
obj("cup", 1, container=1, capacity=100, openable=6)    # 8
obj("pot", 1, container=1, capacity=100, openable=6)    # 9
obj("pan", 1, container=1, capacity=100, openable=6)    # 10
obj("ring", 1)                                          # 11

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

s(6)
task(["probe"], "PROBE OK.")
task(["put coin in bag"], "C1 PASS.", "C1 FAIL.")
task(["* stone * jar"], "C2 PASS.", "C2 FAIL.")
task(["put lamp in *"], "C3 PASS.", "C3 FAIL.")
task(["put gem in pot"], "C4 PASS.", "")
task(["put %object% in pan"], "C5 PASS.", "C5 FAIL.")

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

out = sys.argv[1] if len(sys.argv) > 1 else "pPUTCLM39.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
