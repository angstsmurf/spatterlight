#!/usr/bin/env python3
"""ADRIFT 3.9 probe: what does "it" name after each kind of line?

crossworlds4 (3.90) differs from run390 three times, all in the pronoun echo:

    x microwave / open it / search it   Runner "(the microwave)",  Scarier "(a microwave)"
    open cabinet / get jet / read it    Runner "(the cabinet)",    Scarier "(a vial of jet)"
    ... / get it                        Runner "(a cabinet)",      Scarier "(a vial of jet)"

Reading run390: generaltasks runs co(obj, 0) over EVERY object on every line
(45F318-45F430) before any handler, and co() ends at 43B626-43B6B0 by storing
mode-0 "the X" for an object whose Short (or Alias) is typed, is obhere and
is seen -- so the last such object in index order wins.  Handlers write
mode 1 afterwards (takes 455067, drops 445BE6, examines 44BC53/44BE52, ...).
takes' auto-"from" rewrite (4552EE) turns `get jet` into `get jet from
cabinet` and returns unclaimed.

The objects copy crossworlds4's records: a static microwave (empty Prefix and
Alias) with an `open microwave` task, a static closed cabinet holding "a vial
of" "jet" (Alias "vial", readable), with the vial BEFORE the cabinet in index
order as in the game.  A static open crate comes BEFORE the "a" "coin" inside
it, so an index-order answer and a role answer tell themselves apart.  The
"a" "ball" is a loose control.  Every `search X` is task `search *`.

Measured (run390x, Adrift_p39it.txt, 2026-09-24): every guess above held --
`open it`/`search it` after `x microwave` "(the microwave)", `get jet` then
`read it` "(the cabinet)", `get coin` from the crate "(the coin)", and the
handlers' mode-1 "(a ball)"/"(a vial of jet)".  Ported as
uip_assign_antecedent_390() (scparser.cpp); identical on all 48 turns.

Usage:   python3 make_39_itprobe.py [out.taf]
Session: fast.sh p39IT.taf cmdfile_p39it.txt run390x.exe
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 antecedent probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("IT39 Probe"); s("SCARE probe"); s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0); s(902); s(902)  # Task Position ParentObject Gender MaxSize MaxWt
for _ in range(9): s(0)
s(3); s(3)                              # SizeMultiple WeightMultiple

# ROOMS
s(1)
s("Probe Room"); s("A bare room."); s("")
for _ in range(8): s(0)
s(""); s(0); s(""); s(0); s(0); s(""); s(0)   # the 3.9 inline Alts block
s(0)                                          # HideOnMap

# OBJECTS
def static(prefix, short, alias, desc, container=0, capacity=0, openable=0):
    s(prefix); s(short); s(alias)
    s(1)                 # BStatic
    s(desc)
    s(0); s(0); s(0)     # InitialPosition Task TaskNotDone
    s("")                # AltDesc
    s(1); s(1)           # Where: ROOM_LIST1 type 1, room 1
    s(container); s(0); s(capacity)
    s(openable)          # on disk 5 = closed, 6 = open
    s(0)                 # SitLie
    s(0)                 # BReadable

def dynamic(prefix, short, alias, desc, position, parent=0, readtext=None):
    s(prefix); s(short); s(alias)
    s(0)                 # BStatic
    s(desc)
    s(position)          # 2 = inside container `parent`, 4 = room 0
    s(0); s(0); s("")    # Task TaskNotDone AltDesc
    s(0); s(0); s(0)     # Container Surface Capacity
    s(0); s(0); s(parent)  # Wearable SizeWeight Parent
    s(0); s(0); s(0)     # Openable SitLie Edible
    if readtext is None:
        s(0)
    else:
        s(1); s(readtext)
    s(0)                 # Weapon

s(6)
static("", "crate", "", "A wooden crate.", container=1, capacity=100, openable=6)   # 0, container 0
dynamic("a", "coin", "", "A gold coin.", 2, parent=0)                              # 1
static("", "microwave", "", "A cheap microwave.")                                   # 2
dynamic("a vial of", "jet", "vial", "A small vial.", 2, parent=1,
        readtext="Property of the Chosen One.")                                    # 3
static("", "cabinet", "", "A cabinet.", container=1, capacity=100, openable=5)     # 4, container 1
dynamic("a", "ball", "", "A red ball.", 4)                                          # 5

# TASKS
def task(cmd, text):
    s(0); s(cmd)
    s(text); s(""); s(""); s("")
    s(0); s(1); s(0)     # ShowRoomDesc Repeatable Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: all rooms
    s("")                # Question
    s(0)                 # Restrictions
    s(0)                 # Actions

s(3)
task("open microwave", "MICROWAVE TASK.")
task("search *", "SEARCHED.")
task("probe", "PROBE OK.")

# EVENTS / NPCS
s(0)
s(0)

# tail
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

out = sys.argv[1] if len(sys.argv) > 1 else "p39IT.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
