#!/usr/bin/env python3
"""ADRIFT 3.9 probe: a reverse command of a task that has not been done.

matt (3.90) T12: in the Downstairs Hallway `out` gets "You have already done
that." from the Runner, and the player stays put; Scarier takes the Out exit.
Task "enter boss room" (one room, not repeatable, reversible) has reverse
commands "leave boss room"/"leave"/"back"/"out".

Reading run390 checktask's reverse pass (44B1B9-44B4D7), for each reverse
command that matches:

    done (218) or repeatable (219)   in the room (224) -> -2, the reversal
                                     elsewhere          -> nothing
    otherwise                        an EMPTY buffer gets RepeatText (44B4B2)
                                     -- no room test, no `running` test

The reverse pass is skipped when a forward command matched and the task is
not done (44B18F).

Cells (R0 Probe Room, E -> R1 Box Room; R1 W and Out -> R0):

    A  enter box   R1 only, once, reversible: out / leave box
    B  zap         all rooms, once, reversible: unzap, RepeatText BREP.
    C  poke        R1 only, repeatable, reversible: unpoke
    G  jump        all rooms, once, RepeatText GREP.         (G before H)
    H  hop         all rooms, once, reversible: jump, RepeatText HREP.
    E  bounce      all rooms, once, reversible: skip, RepeatText EREP.
    F  skip        all rooms, repeatable                     (E before F)
    D  dance       all rooms, once, reversible: undance, RepeatText DREP.

Measured (run390x, Adrift_p39rev.txt, 2026-09-24), every guess held:

    `out` in R0 before A          "You have already done that." (A's RepeatText
                                  default), though A is R1-only -- no room test
    `unzap` before zap            BREP.
    `unpoke` in R0, C undone      "I don't understand." (repeatable: the
                                  reversal branch, out of its room)
    `jump` twice                  JUMPED., then GREP. (G's forward claim wins
                                  the order; H's reverse never runs)
    `skip`                        SKIPPED. (E's reverse is skipped: F matched
                                  forward and is not done, 44B18F)
    `undance` twice               UNDANCED., then DREP. (reverse_task 4283C8
                                  has no done check of its own)
    `out` in R1 before A          RepeatText, not the Out exit
    `enter box`, `out`            ENTERED., LEFT BOX.
    `unpoke` in R1, C undone      UNPOKED. (a repeatable task reverses undone)
    `leave box` in R0, A undone   RepeatText again
    zap / unzap / unzap           ZAPPED., UNZAPPED., BREP.

Ported as scrunner.cpp run_spent_task_390() (the RepeatText claim) and the
TAF_VERSION_390 arms of sctasks.cpp task_state_allows_run() and
task_run_task_unrestricted(); identical on all 27 turns.  matt then needs
`go out` where its route typed `out` (Runner win 161/161, Adrift_282_matt_rt).

Usage:   python3 make_39_reverseprobe.py [out.taf]
Session: fast.sh p39REV.taf cmdfile_p39rev.txt run390x.exe
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 reverse-command probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("REV39 Probe"); s("SCARE probe"); s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)      # Persp ShowExits WaitTurns DispFirstRoom BattleSystem MaxScore
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0); s(902); s(902)  # Task Position ParentObject Gender MaxSize MaxWt
for _ in range(9): s(0)
s(3); s(3)                              # SizeMultiple WeightMultiple

# ROOMS: exits N E S W U D In Out, a destination is 1-based then two zeros
def room(name, desc, exits):
    s(name); s(desc); s("")
    for d in ("n", "e", "s", "w", "u", "d", "in", "out"):
        dest = exits.get(d, 0)
        s(dest)
        if dest:
            s(0); s(0)
    s(""); s(0); s(""); s(0); s(0); s(""); s(0)   # the 3.9 inline Alts block
    s(0)                                          # HideOnMap

s(2)
room("Probe Room", "A bare room.", {"e": 2})
room("Box Room", "A room with a box.", {"w": 1, "out": 1})

# OBJECTS
s(0)

# TASKS
def task(cmds, text, reverse_text="", repeat_text="", repeatable=0,
         reverse_cmds=(), room=None):
    s(len(cmds) - 1)
    for c in cmds: s(c)
    s(text); s(reverse_text); s(repeat_text); s("")
    s(0); s(repeatable); s(1 if reverse_cmds else 0)
    if reverse_cmds:
        s(len(reverse_cmds) - 1)
        for c in reverse_cmds: s(c)
    else:
        s(0); s("")
    if room is None:
        s(3)                 # Where: all rooms
    else:
        s(1); s(room)        # Where: one room, 0-based
    s("")                    # Question
    s(0)                     # Restrictions
    s(0)                     # Actions

s(9)
task(["probe"], "PROBE OK.", repeatable=1)
task(["enter box"], "ENTERED.", reverse_text="LEFT BOX.",
     reverse_cmds=["out", "leave box"], room=1)                         # A
task(["zap"], "ZAPPED.", reverse_text="UNZAPPED.", repeat_text="BREP.",
     reverse_cmds=["unzap"])                                            # B
task(["poke"], "POKED.", reverse_text="UNPOKED.", repeatable=1,
     reverse_cmds=["unpoke"], room=1)                                   # C
task(["jump"], "JUMPED.", repeat_text="GREP.")                          # G
task(["hop"], "HOPPED.", reverse_text="UNHOPPED.", repeat_text="HREP.",
     reverse_cmds=["jump"])                                             # H
task(["bounce"], "BOUNCED.", reverse_text="UNBOUNCED.", repeat_text="EREP.",
     reverse_cmds=["skip"])                                             # E
task(["skip"], "SKIPPED.", repeatable=1)                                # F
task(["dance"], "DANCED.", reverse_text="UNDANCED.", repeat_text="DREP.",
     reverse_cmds=["undance"])                                          # D

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

out = sys.argv[1] if len(sys.argv) > 1 else "p39REV.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
