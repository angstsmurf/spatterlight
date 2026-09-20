#!/usr/bin/env python3
"""ADRIFT 3.90 probe: which "<Verb> what?" rows store the question prefix.

run390's four object handlers each end with

    If msg = "" Then msg = "<Verb> what?" : MemVar_4681D0 = MemVar_468118

  takes   455890 / 455897      drops   445F0B / 445F12
  wears   43D27F / 43D286      removes 439FBF / 439FC6

i.e. they store the line AS TYPED, not only a line that is the bare verb --
unlike checkverb (42A4F4), which answers "<What> what?" for the bare verb
alone.  generaltasks 4601A5 then puts the stored line in front of the next
line nothing else answers.  So `wear zzz` / `hat` should put the hat on,
the same way bare `wear` / `hat` does.  Scarier's lib_what() stores only a
line equal to the verb, so the `<verb> zzz` half is the cell that decides.

    Probe Room: a hat (held, wearable), a stone (on the floor, dynamic)

Usage:
    python3 make_39_whatprobe.py p39WHAT.taf
Session:
    ./fast.sh p39WHAT.taf cmdfile_p39what.txt run390x.exe
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 question-prefix probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39WHAT"); s("SCARE probe"); s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0); s(100); s(100)  # Task Position ParentObject Gender MaxSize MaxWt
for _ in range(11): s(0)                # compass..iUnk2

# ROOMS
s(1)
s("Probe Room"); s("LONG."); s("")
for _ in range(8): s(0)
s(""); s(0); s(""); s(0); s(0); s(""); s(0); s(0)

# OBJECTS
def obj(prefix, short, desc, wearable, position, container, parent=0):
    s(prefix); s(short); s(short)
    s(0)                 # Static
    s(desc)
    s(position)          # 1 = held, 2 = inside container `parent`, 4 = room `parent`
    s(0); s(0); s("")    # Task TaskNotDone AltDesc
    s(container); s(0); s(100 if container else 0)  # Container Surface Capacity
    s(wearable); s(0); s(parent)  # Wearable SizeWeight Parent
    s(0); s(0); s(0); s(0); s(0)  # Openable SitLie Edible Readable Weapon

s(2)
obj("a", "hat", "A felt hat.", 1, 1, 0)      # held, wearable
obj("a", "stone", "A grey stone.", 0, 4, 0)  # on the floor

# TASKS, EVENTS, NPCS, tail
s(0)
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

out = sys.argv[1] if len(sys.argv) > 1 else "p39WHAT.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
