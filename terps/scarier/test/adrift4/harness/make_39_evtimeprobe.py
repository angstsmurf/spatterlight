#!/usr/bin/env python3
"""ADRIFT 3.9 probe: how long is the period of an immediately-restarting event,
and does the restart re-run the start actions?

RUNNER_TESTS_TODO.md section 8.  SCARE has always claimed that 3.9 and 3.8
"'miss' the event start actions and move one step into the event without
comment", so `evt_fixup_v390_v380_immediate_restart()` re-armed silently AND
took one turn off the clock.  An earlier 2026-08-04 pass, reading Panic!'s
published transcript, flipped the first half (make the StartText print) and
kept the second.  This probe measured both against run390 and found the
opposite of that pass on BOTH counts: the restart IS silent, and the restarted
event keeps its FULL authored length.  Panic!'s own events are all
Time1=Time2=1, where a period of 1 and a period of 0 are indistinguishable,
which is why the timing half had gone untested.

This probe makes the two readings four turns apart.  One room, one control
task, one event:

    starter 1 (immediate), restart 1 (immediately), Time1 = Time2 = 5,
    StartText "E START.", FinishText "E FINISH.", no LookText, no task.

Immediate events are started during load in both Runners (section 4's turn-0
row), so the first StartText is lost to the pre-intro screen and the first
period runs from turn 0.  After that:

    one-turn-short clock (period 4):  FINISH on turns 5, 9, 13
    full authored length (period 5):  FINISH on turns 5, 10, 15

Session: 15 x `z`, count the turns between the "E FINISH." lines.
run390 printed 5, 10, 15 -- the full length -- and no "E START." at all after
the first, so the fixup now re-arms silently and leaves the clock alone.

Usage: python3 make_39_evtimeprobe.py [out.taf] [variant]

Variants -- the first run of the base probe answered the period question and
raised a second one, because run390 printed NO "E START." on any restart, which
is the opposite of what the Panic! transcript was read to say.  Every variant
below was played in run390; the results are tabulated in section 8:

    (none)  starter 1 (immediate), restart 1, Time1=Time2=5
    "b"     the same with Time1=Time2=1 -- the always-restarting one-turn
            event of thetest and of Panic!'s ambience, where the StartText is
            the only text the player can see.  Session: 8 x z.
    "c"     starter 2 (a 3-turn delay), restart 1, Time1=Time2=5 -- Panic!'s
            Stigmata/Ghost Shimmers shape, in case the starter type is what
            decides whether a restart re-runs the start actions.
    "e"     starter 3 (after the `ping` task, made non-repeatable), restart 1,
            Time1=Time2=1 -- Panic!'s Priest Coughs / Crying Amelia shape,
            the one whose published transcript motivated the whole section.
            Session: z, z, ping, then 6 x z.
    "f"     the exact Panic! "Priest Coughs" shape: starter 3, restart 1,
            Time1=Time2=1, StartText + LookText, NO FinishText.  Variant "e"
            showed the restart is silent, so if the coughs are not the
            StartText they can only be a per-turn LookText.  Session as "e".
    "d"     starter 2 (a 3-turn delay), restart 2 ("restart after a delay"),
            Time1=Time2=5 -- the control: this is the one restart the Runner
            certainly does route back through its start path, so it shows what
            a re-run start looks like in the transcript.

The 4.0 twin is config EV9 in make_arena_probe.py -- the fixup is version-gated,
so 4.0 is expected to show the unshortened period 5.
"""
import sys

VARIANT = sys.argv[2] if len(sys.argv) > 2 else ""
TIME = 1 if VARIANT in ("b", "e", "f") else 5
STARTER = 3 if VARIANT in ("e", "f") else 2 if VARIANT in ("c", "d") else 1
RESTART = 2 if VARIANT == "d" else 1
DELAY = 3

L = []
def s(x): L.append(str(x))

# HEADER
s("Event period probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("EvTime Probe 39"); s("SCARE probe"); s("I don't understand.")
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

# TASKS -- one control, so the transcript proves the file is wired.
s(1)
s(0); s("ping")
s("PING FIRED."); s(""); s(""); s("")
s(0); s(0 if VARIANT in ("e", "f") else 1); s(0)  # ShowRoomDesc Repeatable Reversible
s(0); s("")          # W$ReverseCommand
s(3)                 # Where: all rooms
s("")                # Question
s(0)                 # Restrictions
s(0)                 # Actions

# EVENTS -- the length-5 always-restarting event.
s(1)
s("Period Five")
s(STARTER)               # StarterType (1 = immediate, 2 = random delay)
if STARTER == 2:
    s(DELAY); s(DELAY)   # StartTime EndTime
elif STARTER == 3:
    s(1)                 # TaskNum (1-based: the `ping` task)
s(RESTART)               # RestartType (1 = immediately, 2 = after a delay)
s(0)                     # TaskFinished
s(TIME); s(TIME)         # Time1 Time2
s("E START."); s("E LOOK." if VARIANT == "f" else ""); s("" if VARIANT == "f" else "E FINISH.")
s(3)                     # Where: all rooms
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

out = sys.argv[1] if len(sys.argv) > 1 else "pEVT39.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
