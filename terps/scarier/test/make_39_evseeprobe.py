#!/usr/bin/env python3
"""ADRIFT 3.9 probe: does an event's text still print while the player is on
or inside an object?

RUNNER_TESTS_TODO.md section 8, the residual row.  Panic!'s wraith event fires
24 times for Scarier against the published transcript's 21; the three extra
are exactly the turns the player spends up the rope on the statue -- i.e.
parented to an object rather than standing on the room floor.  Scarier's
`evt_can_see_event()` (scevents.cpp) only compares `gs_playerroom()` against
the event's room list, so posture cannot suppress anything.  Nobody has asked
a Runner.

(The 21 comes from the same transcript that section 8 showed disagrees with
run390 on this very game, so it is a hint, not evidence.  This probe has to
stand on its own.)

One room, one always-restarting one-turn event limited to that room by a
single-room `Where` list, and two static objects to be parented to:

    chair -- surface,   SitLie = 3 (standable + lieable)
    crate -- container, SitLie = 3

The event carries all three texts, so one session separates them:

    StartText  "E START."   -- once, at the immediate start
    LookText   "E LOOK."    -- room descriptions only
    FinishText "E TICK."    -- every turn (section 8, variant b)

Session:

    z                 -- E TICK, on the floor (control)
    sit on chair      -- E TICK?  parented to a surface
    z
    look              -- does the LookText still appear?
    stand             -- back on the floor
    z                 -- E TICK (control again)
    sit in crate      -- E TICK?  parented to a container
    z
    stand
    z

Read-out: if every turn shows "E TICK." the Runner does not care about
posture and `evt_can_see_event()` is right as it stands; if the chair/crate
turns are mute, the Runner suppresses event text for a parented player and
Scarier needs a parent-object test in `evt_can_see_event()`.

Measured 2026-08-04: run390 prints "E TICK." on EVERY turn -- on the chair, in
the crate and on the floor alike -- and the LookText still appears in a `look`
taken while seated.  No posture test exists; no code change.  Scarier's output
on this file is identical.

Usage: python3 make_39_evseeprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("Event visibility probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("EvSee Probe 39"); s("SCARE probe"); s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0); s(100); s(100)  # Task Position ParentObject Gender MaxSize MaxWt
for _ in range(11): s(0)                # compass..iUnk2

# ROOMS
s(1)
s("Probe Room"); s("LONG."); s("")
for _ in range(8): s(0)
s(""); s(0); s(""); s(0); s(0); s(""); s(0); s(0)

# OBJECTS -- two statics in room 0, both sit/lie-able (SitLie = 1|2).
def static_object(short, description, container, surface, capacity):
    s("a"); s(short)
    s(short)             # [1]$Alias
    s(1)                 # BStatic
    s(description)
    s(0); s(0); s(0)     # InitialPosition Task TaskNotDone
    s("")                # AltDesc
    s(1); s(1)           # BStatic: Where = ROOM_LIST1 type 1, room 1 (ROOM_LIST1
                         # rooms are 1-based: index 0 is the extra "nowhere" slot)
    s(container); s(surface); s(capacity)
    # BStatic and Where type != 4, so no {OBJECT:#Parent} line.
    s(0)                 # Openable (the |V390_OBJECT| fixup reads no line)
    s(3)                 # SitLie: standable | lieable
    s(0)                 # BReadable (BEdible is dynamic-only)
    # The record ends here: CurrentState, ListFlag, InRoomDesc and
    # OnlyWhenNotMoved are Z/F/E defaults that 3.9 does not store, and Res1 and
    # Res2 are empty with Sound and Graphics both off.

s(2)
static_object("chair", "A plain wooden chair.", 0, 1, 0)
static_object("crate", "A big wooden crate.", 1, 0, 10)

# TASKS -- one control, so the transcript proves the file is wired.
s(1)
s(0); s("ping")
s("PING FIRED."); s(""); s(""); s("")
s(0); s(1); s(0)     # ShowRoomDesc Repeatable Reversible
s(0); s("")          # W$ReverseCommand
s(3)                 # Where: all rooms
s("")                # Question
s(0)                 # Restrictions
s(0)                 # Actions

# EVENTS -- always-restarting one-turn event, limited to room 0.
s(1)
s("Room Tick")
s(1)                     # StarterType: immediate
s(1)                     # RestartType: immediately
s(0)                     # TaskFinished
s(1); s(1)               # Time1 Time2
s("E START."); s("E LOOK."); s("E TICK.")
s(1); s(0)               # Where: ROOM_LIST0 type 1 (one room), room 0
s(0); s(0)               # PauseTask PauserCompleted
s(0); s("")              # PrefTime1 PrefText1
s(0); s(0)               # ResumeTask ResumerCompleted
s(0); s("")              # PrefTime2 PrefText2
s(0); s(0); s(0); s(0); s(0); s(0)  # Obj2/Dest Obj3/Dest Obj1/Dest
s(0)                     # TaskAffected (1-based; 0 = none)

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

out = sys.argv[1] if len(sys.argv) > 1 else "pEVSEE39.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
