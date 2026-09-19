#!/usr/bin/env python3
"""ADRIFT 3.9 probe: does a pre-4.0 event whose clock ROLLS 0 park, and is a
restart-after-delay event with an immediate starter a one-shot?

run390's checkevent (448EB8) has run400's shape (see the "rolls 0 parks" and
"restart-after-delay one-shot" rules, both measured at 4.0 only):

  - the waiting block (448358) decrements, and on 0 stores
    Int(Rnd * (Time2 - Time1)) + Time1 with NO +1 (448395), then falls into
    the running block in the same call;
  - the running block decrements (44892B) BEFORE the finish test, which is
    `clock = 0` (448A6B), so a clock of 0 goes to -1 and never finishes;
  - RestartType 1 stores the roll with no +1 (448E05);
  - RestartType 2 sets waiting and stores Int(Rnd * (EndTime - StartTime))
    + StartTime (448E7F); those two are only in the taf for a random-delay
    starter.

Rolls exclude the upper bound, so Time1 = 0, Time2 = 1 always rolls 0 while
the event is NOT authored zero-length.  One room, one `ping` task, three
events, each with START/LOOK/FINISH texts:

    A  starter 3 (`ping`), restart 1, Time 0..1.  Start = 0 + 1, the same
       call's decrement finishes it on the ping turn; the restart rolls 0
       and it parks: "A FINISH." once, "A LOOK." in every later look.
    B  starter 2 (delay 3), no restart, Time 0..1.  Starts off the waiting
       clock with a 0 and parks: "B START.", no "B FINISH.", "B LOOK." later.
    C  starter 1 (immediate), restart 2, Time 2..2.  Finishes on turn 2, then
       waits on a clock of 0 that goes to -1: "C FINISH." once, no "C LOOK."
       afterwards.

Session: z z z ping z z z z look z z look.

Usage: python3 make_39_evrollprobe.py [out.taf] [38]

With "38" the file is a 3.80 twin (make_38_darkprobe.py shapes; the event
record is the same), for run380, whose checkevent has the same blocks.
"""
import sys

V38 = len(sys.argv) > 2 and sys.argv[2] == "38"

L = []
def s(x): L.append(str(x))

# HEADER
s("Event roll probe."); s("**")
s(0)
s("You have won."); s("**")

if V38:
    # GLOBAL, ROOMS, OBJECTS, TASKS in the 3.8 shapes of make_38_darkprobe.py.
    s("EvRoll Probe 38"); s("SCARE probe"); s(10); s("I don't understand.")
    s(2); s(0); s(0)                    # Perspective ShowExits WaitTurns
    s(1)
    s("Probe Room"); s("LONG."); s("")
    for _ in range(8): s(0)
    s(""); s(0); s(""); s(0)            # AddDesc1 Task1 AddDesc2 Task2
    s(0); s(""); s(0)                   # Obj AltDesc TypeHideObjects
    s(0)
    s(1)
    s(0); s("ping")
    s("PING FIRED."); s(""); s(""); s("")
    s(0); s(0); s(0); s(0)              # ShowRoomDesc Repeatable Score SingleScore
    for _ in range(6): s(0); s(0); s(0) # Movements
    s(0); s(0); s("")                   # Reversible W$ReverseCommand
    s(0); s(0); s(0); s(0); s(0)        # WearObj1/2 HoldObj1/2/3
    s(0); s(0); s(0)                    # Obj1 Task TaskNotDone
    s(""); s(""); s(""); s("")          # TaskMsg HoldMsg WearMsg CompanyMsg
    s(0); s(0); s(""); s(0)             # NotInSameRoom NPC Obj1Msg Obj1Room
    s(3); s(0); s(0); s("")             # Where KillsPlayer HoldingSameRoom Question
    s(0); s(0)                          # Obj2 WinGame
else:
    # GLOBAL
    s("EvRoll Probe 39"); s("SCARE probe"); s("I don't understand.")
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
    s(0)

    # TASKS -- `ping` starts event A.
    s(1)
    s(0); s("ping")
    s("PING FIRED."); s(""); s(""); s("")
    s(0); s(0); s(0)     # ShowRoomDesc Repeatable Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: all rooms
    s("")                # Question
    s(0)                 # Restrictions
    s(0)                 # Actions

# EVENTS
def event(tag, starter, restart, t1, t2, delay=3):
    s("Event " + tag)
    s(starter)               # StarterType (1 immediate, 2 random delay, 3 task)
    if starter == 2:
        s(delay); s(delay)   # StartTime EndTime
    elif starter == 3:
        s(1)                 # TaskNum (1-based: `ping`)
    s(restart)               # RestartType (0 never, 1 immediately, 2 after a delay)
    s(0)                     # TaskFinished
    s(t1); s(t2)             # Time1 Time2
    s(tag + " START."); s(tag + " LOOK."); s(tag + " FINISH.")
    s(3)                     # Where: all rooms
    s(0); s(0)               # PauseTask PauserCompleted
    s(0); s("")              # PrefTime1 PrefText1
    s(0); s(0)               # ResumeTask ResumerCompleted
    s(0); s("")              # PrefTime2 PrefText2
    s(0); s(0); s(0); s(0); s(0); s(0)  # Obj2/Dest Obj3/Dest Obj1/Dest
    s(0)                     # TaskAffected

s(3)
event("A", 3, 1, 0, 1)
event("B", 2, 0, 0, 1)
event("C", 1, 2, 2, 2)

# NPCS, tail
s(0)
s(0); s(0)
if not V38:
    s(0); s(0); s(0)
s("2026")
s("    Wild    ")

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x36 if V38 else 0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else ("pEVROLL38.taf" if V38 else "pEVROLL39.taf")
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
