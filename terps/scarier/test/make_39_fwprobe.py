#!/usr/bin/env python3
"""ADRIFT 3.9 probe: the three 4.0 precedence cells, on the 3.9 side.

Companion to the 2026-08-02 run400 probes FM4-FM7 (RUNNER_TESTS_TODO.md
section 4).  One room, four dynamics, four tasks, one zero-length event:

    rock   -- on the floor, not wearable.
    pill   -- held.
    cup    -- held, a container.
    slime  -- on the floor (its never-held state is the always-fail gate).

    Task 1 `take * rock`      restr "slime held" -> FAIL, msg "TFAIL."
    Task 2 `wear * rock`      same restr, msg "WFAIL."
    Task 3 `put * pill in cup` + the choice form
           `[put/drop/mix]{the}[pill]{in/to/with}{the/a/an}[slime/goo]`,
           same restr, msg "XFAIL."
    Task 4 `zzpillcheck`      restr "pill inside cup", empty FailMessage,
           CompleteText "PILLCHECK FIRED.", repeatable.
    Event  starter=1, restart, Time1=Time2=0, TaskAffected=4 -- the
           zero-length always-restarting checker, TheADRIFTProject's
           `#Pill Check` shape.

Session (same for run390 and Scarier):
    take rock         TFAIL (task claims) or the library take?
    i
    wear rock         WFAIL or the library wear refusal?
    put pill in cup   XFAIL or the library put?  (the put-family
                      precedence cell, 3.9 half)
    put pill into cup matches NO task ("into" misses both patterns) ->
                      library put; then does PILLCHECK fire on THIS turn
                      (tick after the command) or the NEXT one?
    z
    z

Usage: python3 make_39_fwprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("Precedence probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("FW Probe 39"); s("SCARE probe"); s("I don't understand.")
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
    s(position)          # 1 = held, 2 = inside container `parent`, 4 = room 0
    s(0); s(0); s("")    # Task TaskNotDone AltDesc
    s(container); s(0); s(100 if container else 0)  # Container Surface Capacity
    s(wearable); s(0); s(parent)  # Wearable SizeWeight Parent
    s(0); s(0); s(0); s(0); s(0)  # Openable SitLie Edible Readable Weapon

# Variant "b": the pill STARTS inside the cup, so the checker's restriction
# is true from turn one -- proves the event machinery runs at all, and the
# `take pill` falling edge gives the tick-order cell.
# Variant "p": tasks 1-3 have NO restrictions, so they PASS -- does a passing
# take/wear/put task claim the command in run390, or does the library still
# run?  (The 2026-08-02 failing-task probe showed run390 running the library
# take; this pins whether that was restriction-fallback or a full bypass.)
VARIANT_B = len(sys.argv) > 2 and sys.argv[2] == "b"
VARIANT_P = len(sys.argv) > 2 and sys.argv[2] == "p"

s(4)
obj("a", "rock", "A grey rock.", 0, 4, 0)     # file Var1=3
if VARIANT_B:
    obj("a", "pill", "A small pill.", 0, 2, 0, 0)  # inside container 0 = cup
else:
    obj("a", "pill", "A small pill.", 0, 1, 0)     # file Var1=4
obj("a", "cup", "A tin cup.", 0, 1, 1)        # file Var1=5
obj("a", "slime", "Green slime.", 0, 4, 0)    # file Var1=6

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

s(4)
task(["take * rock"], "TAKEPASS.",
     [] if VARIANT_P else [(6, 1, 0, "TFAIL.")])
task(["wear * rock"], "WEARPASS.",
     [] if VARIANT_P else [(6, 1, 0, "WFAIL.")])
task(["put * pill in cup",
      "[put/drop/mix]{the}[pill]{in/to/with}{the/a/an}[slime/goo]"],
     "PUTPASS.", [] if VARIANT_P else [(6, 1, 0, "XFAIL.")])
task(["zzpillcheck"], "PILLCHECK FIRED.", [(4, 4, 1, "")])

# EVENTS -- the zero-length always-restarting checker.
s(1)
s("Pill Check")          # Short
s(1)                     # StarterType 1 = immediate
s(1)                     # RestartType 1 = restart when finished
s(0)                     # TaskFinished
s(0); s(0)               # Time1 Time2
s(""); s(""); s("")      # StartText LookText FinishText
s(3)                     # Where: all rooms
s(0); s(0)               # PauseTask PauserCompleted
s(0); s("")              # PrefTime1 PrefText1
s(0); s(0)               # ResumeTask ResumerCompleted
s(0); s("")              # PrefTime2 PrefText2
s(0); s(0); s(0); s(0); s(0); s(0)  # Obj2/Dest Obj3/Dest Obj1/Dest
s(4)                     # TaskAffected (1-based)

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

out = sys.argv[1] if len(sys.argv) > 1 else "pFW39.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
