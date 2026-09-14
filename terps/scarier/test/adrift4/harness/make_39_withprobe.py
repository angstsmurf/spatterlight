#!/usr/bin/env python3
"""ADRIFT 3.9 probe: run390's " with " twin and its turn counter.

  * therest's " with " arm (45D12C): an instrument that is not here and not
    held asks "With what?" and saves the line up to "with " as the question
    prefix; the next line is prefix + line (4601A5), which jumps back above
    the per-element increment (45EC5B) -- does a continued line count twice?
  * The instrument present but not held ("don't have"), and held.
  * `again` -- does run390 echo "(<cmd>)"?
  * `wait` with WaitTurns 3 -- one turn or three?
  * `both` (45FEB6) re-runs the saved line: counted twice?

`turns` is administrative in run390 and prints the counter.  A length-1
self-restarting event prints TICK. whenever events() ran.

Rooms Alpha (north -> Beta) and Beta.  The rope and the stone lie in Alpha,
the knife and the coin are held, the gem is in Beta.

Usage: python3 make_39_withprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 with probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39WITH")        # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(1)                     # Perspective
s(0)                     # ShowExits
s(3)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(8)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test player.")      # PlayerDesc
s(0)                     # Task
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(100)                   # MaxSize
s(100)                   # MaxWt
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
s(0)                     # iUnk1
s(0)                     # iUnk2

# ROOMS -- 8 exit slots (N E S W up down in out): 0, or Dest Var1 Var2.
def room(short, long_, exits):
    s(short); s(long_); s("")
    for d in range(8):
        if d in exits:
            s(exits[d]); s(0); s(0)
        else:
            s(0)
    s(""); s(0); s(""); s(0)  # AddDesc1 Task1 AddDesc2 Task2
    s(0); s("")               # Obj AltDesc
    s(0); s(0)                # TypeHideObjects HideOnMap

s(2)
room("Alpha", "The first room.", {0: 2})
room("Beta", "The second room.", {2: 1})

# OBJECTS -- all dynamic, written the way make_39_putprobe.py writes them.
def obj(short, position):
    s("a"); s(short); s(short)
    s(0)                                # Static
    s("A probe object.")
    s(position)                         # 1 held, 4 room 0, 5 room 1
    s(0); s(0); s("")                   # Task TaskNotDone AltDesc
    s(0); s(0); s(0)                    # Container Surface Capacity
    s(0); s(0); s(0)                    # Wearable SizeWeight Parent
    s(0); s(0); s(0); s(0); s(0)        # Openable SitLie Edible Readable Weapon

s(5)
obj("rope", 4)
obj("knife", 1)
obj("coin", 1)
obj("stone", 4)
obj("gem", 5)

# TASKS
s(1)
s(0); s("probe")         # W$Command
s("PROBE OK.")           # CompleteText
s(""); s(""); s("")      # Reverse/Repeat/Additional
s(0)                     # ShowRoomDesc
s(1)                     # Repeatable
s(0)                     # Reversible
s(0); s("")              # W$ReverseCommand
s(3)                     # Where: all rooms
s("")                    # Question
s(0)                     # Restrictions
s(0)                     # Actions

# EVENTS -- length 1, restarts immediately.
s(1)
s("Ticker")
s(1)                     # StarterType: immediate
s(1)                     # RestartType: immediately
s(0)                     # TaskFinished
s(1); s(1)               # Time1 Time2
s(""); s(""); s("TICK.") # StartText LookText FinishText
s(3)                     # Where: all rooms
s(0); s(0)               # PauseTask PauserCompleted
s(0); s("")              # PrefTime1 PrefText1
s(0); s(0)               # ResumeTask ResumerCompleted
s(0); s("")              # PrefTime2 PrefText2
s(0); s(0); s(0); s(0); s(0); s(0)  # Obj2/Dest Obj3/Dest Obj1/Dest
s(0)                     # TaskAffected

# NPCS, tail
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

out = sys.argv[1] if len(sys.argv) > 1 else "p39WITH.taf"
open(out, "wb").write(SIG + obf)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
