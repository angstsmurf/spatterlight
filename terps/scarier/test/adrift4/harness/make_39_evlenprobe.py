#!/usr/bin/env python3
"""ADRIFT 3.9 probe: is the event-length roll inclusive of Time2 when
Time1 != Time2?  And the same question for the StartTime..EndTime delay roll.

RUNNER_TESTS_TODO.md section 10.  This is the V390 twin of config EL in
make_arena_probe.py -- the same three events, so one 24-z session measures
every roll family at once:

    E1 "1S./1F.":  starter 1 (immediate), restart 1 (immediately),
                   Time1=1 Time2=3.  Successive-1F gaps = fresh length rolls.
    E2 "2S./2F.":  starter 2 (delay 1..1), restart 2 (after the same delay),
                   Time1=1 Time2=3.  2S->2F spans = fresh length rolls.
    E3 "3S./3F.":  starter 2 (delay 1..3), restart 2, Time1=Time2=1.
                   Successive-3S gaps = fresh delay rolls.

Signatures on a 1..3 range, ~15 draws per family per session:
    exclusive hi (lo + Int(Rnd*(hi-lo)))      -> values {1,2}
    inclusive    (lo + Int(Rnd*(hi-lo+1)))    -> values {1,2,3}
    the +1 form  (lo+1 + Int(Rnd*(hi-lo)))    -> values {2,3}

run400 result (2026-08-17, 3 sessions, ~145 draws): every draw in {1,2},
both values abundant in every family -- exclusive hi everywhere.
run390 result (same day, sessions r1/r2, ~95 draws): identical -- every draw
in {1,2}, E1 lengths, E2 lengths and E3 delays all mixing 1s and 2s freely,
never a 3.
Both Runners roll lo + Int(Rnd*(hi-lo)); Scarier's inclusive scr_randomint
at the three event-timing call sites was a genuine divergence.

Usage: python3 make_39_evlenprobe.py [out.taf]
Session: sh el_session.sh <tag> run390 pEL39   (24 x z, screenshots every 6)
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("Event length probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("EvLen Probe 39"); s("SCARE probe"); s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0); s(100); s(100)  # Task Position ParentObject Gender MaxSize MaxWt
for _ in range(11): s(0)                # compass..iUnk2

# ROOMS
s(1)
s("Test Arena"); s("A bare arena."); s("")
for _ in range(8): s(0)
s(""); s(0); s(""); s(0); s(0); s(""); s(0); s(0)

# OBJECTS
s(0)

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

# EVENTS -- the three roll families.
def event(short, starter, restart, time1, time2, start_txt, finish_txt,
          delay_lo=0, delay_hi=0):
    s(short)
    s(starter)               # StarterType (1 = immediate, 2 = random delay)
    if starter == 2:
        s(delay_lo); s(delay_hi)   # StartTime EndTime
    s(restart)               # RestartType (1 = immediately, 2 = after a delay)
    s(0)                     # TaskFinished
    s(time1); s(time2)       # Time1 Time2
    s(start_txt); s(""); s(finish_txt)
    s(3)                     # Where: all rooms
    s(0); s(0)               # PauseTask PauserCompleted
    s(0); s("")              # PrefTime1 PrefText1
    s(0); s(0)               # ResumeTask ResumerCompleted
    s(0); s("")              # PrefTime2 PrefText2
    s(0); s(0); s(0); s(0); s(0); s(0)  # Obj2/Dest Obj3/Dest Obj1/Dest
    s(0)                     # TaskAffected (1-based; 0 = none)

s(3)
event("Len Imm Restart", 1, 1, 1, 3, "1S.", "1F.")
event("Len Delay",       2, 2, 1, 3, "2S.", "2F.", delay_lo=1, delay_hi=1)
event("Delay Range",     2, 2, 1, 1, "3S.", "3F.", delay_lo=1, delay_hi=3)

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

out = sys.argv[1] if len(sys.argv) > 1 else "pEL39.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
