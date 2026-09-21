"""Does a task an EVENT ran suppress the pre-4.0 end-of-turn Which prompt?

WINE-TRANSCRIPTS-TODO's "An event's task sets the task-ran flag from 3.80" entry.
run380 clears `MemVar_44F12C` at the top of generaltasks (441A28), sets it in
tasks() 44D0BA whenever a task actually runs, and at the end of the turn
4431B0 tests `(MemVar_44F124 < 0) Or (CInt(MemVar_44F12C) = 1)` -- the second
disjunct is what stops the whole turn being replaced by the object-ambiguity
prompt `Which <term>.  <list>?`.  Reading says an event reaches that flag:
generaltasks calls characters() 443179 and events() 44317E BEFORE the guard,
and checkevent 43A753 sets MemVar_44F0B0 to the affected task's command and
dispatches tasks(CByte(1)) 43A762.  run370's guard 43C8D3 is
`If (MemVar_446140 < 0)` alone -- no flag exists at 3.70 -- so the same world
should keep its prompt there.  The 3.90 rule is already ported (gated
>= 3.90); this settles whether 3.80 joins it and 3.70 stays out.

The world: two namesake hats in the start room, Short "red hat"/"blue hat"
and Alias "hat" apiece, so a line saying `hat` picks the Alias as the term
and leaves two present candidates.  The line is an unknown verb, `poke hat`,
because at 3.70 the end-of-turn prompt only fires for therest/put lines
(examines 4359D5 asks its own "Which hat would you like to examine.")
whereas at 3.80 every handler feeds the same scan.

Two files per version, identical but for the event:

    p3xEVQ    the event runs task 2 (`zzev` -> "EVENT TASK RAN.") every turn
    p3xEVQC   the same event with TaskAffected 0 -- it ticks and runs nothing

so the control shows the prompt is there to be suppressed.  The event is
StarterType 1 (immediate) with RestartType 1 and Time1 = Time2 = 1, which
finishes and restarts on every turn.

Usage:
    python3 make_3738_eventflagprobe.py [370|380|390|all]

Session (from ~/adrift-battle/runner/wine):
    ./fast.sh p38EVQ.taf  cmdfile_evq.txt run380.exe
    ./fast.sh p38EVQC.taf cmdfile_evq.txt run380.exe
    ./fast.sh p37EVQ.taf  cmdfile_evq.txt run370.exe
"""
import sys

import make_surfprobe as surf

LIT = surf.LIT

surf.TASKS = [
    ("probe", "PROBE OK."),
    ("zzev",  "EVENT TASK RAN."),
]

# Shape A: the adjective lives in the Short and the bare word is the Alias.
ALIAS_SHAPE = [
    ("red hat",  "a", "A red hat.",  ("room", LIT), "", 0, 0, 0),
    ("blue hat", "a", "A blue hat.", ("room", LIT), "", 0, 0, 0),
]
# Shape B, the canonical one make_3738_taskprobe.py used: both Shorts are
# "hat" and the adjective is the last word of the Prefix, which is also the
# &HFE escape hatch run380's co() honours.
SHORT_SHAPE = [
    ("hat", "a red",  "A red hat.",  ("room", LIT), "", 0, 0, 0),
    ("hat", "a blue", "A blue hat.", ("room", LIT), "", 0, 0, 0),
]

WORLDS = [
    (ALIAS_SHAPE, {"red hat": "hat", "blue hat": "hat"}, "EVQ"),
    (SHORT_SHAPE, {}, "EVQ2"),
]

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390] if arg == "all" else [int(arg)]):
        for objects, aliases, tag in WORLDS:
            surf.OBJECTS = objects
            surf.NAMES = [o[0] for o in objects]
            surf.ALIASES = aliases
            for affected, suffix in ((2, ""), (0, "C")):
                surf.EVENTS = [surf.event(short="ticker",
                                          task_affected=affected)]
                surf.OUT = {370: "p37%s%s.taf" % (tag, suffix),
                            380: "p38%s%s.taf" % (tag, suffix),
                            390: "p39%s%s.taf" % (tag, suffix)}
                surf.emit(v)
