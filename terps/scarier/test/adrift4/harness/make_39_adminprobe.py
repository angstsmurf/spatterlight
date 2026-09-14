#!/usr/bin/env python3
"""ADRIFT 3.9 probe: which built-in commands are administrative in run390,
and does `save` / `restore` tick the event clock?

run390 generaltasks sets its not-a-turn flag MemVar_468219 (read by the
characters()+events() guard at 460669) only for status (44C52B, battle),
history/past (45F52F), score (45F6C7), count/num (45F7D6),
about/info/author/information (45FB3D), quit/bye/end (45FB6E) and turns
(45FD4D); it is cleared per line at 45EC74.  Scarier's lib_set_admin() also
marks hint, help, clear, license and statusline administrative from 3.90, and
a dozen 4.0-era verbs (again, verbose, brief, notify, time, version, ...)
unconditionally.  The TODO's second lead: FarFromHome's event clock moved one
tick per echoed `save` under run390.

One room, one control task, and a length-1 immediately-restarting event whose
FinishText "TICK." marks every turn on which events() ran.

Usage: python3 make_39_adminprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 admin probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39ADMIN")       # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(1)                     # Perspective
s(0)                     # ShowExits
s(0)                     # WaitTurns
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

# ROOMS
s(1)
s("Test Arena")          # Short
s("A bare arena.")       # Long
s("")                    # LastDesc
for _ in range(8): s(0)  # exits
s("")                    # AddDesc1
s(0)                     # Task1
s("")                    # AddDesc2
s(0)                     # Task2
s(0)                     # Obj
s("")                    # AltDesc
s(0)                     # TypeHideObjects
s(0)                     # HideOnMap

# OBJECTS
s(0)

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

out = sys.argv[1] if len(sys.argv) > 1 else "p39ADMIN.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
