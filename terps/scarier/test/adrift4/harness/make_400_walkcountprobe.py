#!/usr/bin/env python3
"""ADRIFT 4.0 probe: HOW MANY times does a turn's text walk the ALR list?

make_400_alrsrcprobe.py measured that a version 4.0 turn's text is filtered
once at the end of every task that completes and once more at the flush -- its
"uniform" cell, one task whose action executes another, answered "CTU qqqball."
for the self-containing ALR [ball] -> [qball], three walks for two completions.
That model, ported, multiplies a self-containing ALR by however many tasks a
turn completes, which for sophie.taf (its own ALR [north] -> [north (to the
farmhouse)]) means eight repetitions in one room description.  Before blessing
that, the count itself has to be measured beyond the depth-2 case, and so does
the other half of the same rule: a filter pass interpolates variables as well,
so it freezes their values at the completing task rather than at the flush.

The cells, all with the one ALR [ball] -> [qball] so the q's count the walks:

    sierra    "SV n=%n%."  with an every-turn event running the silent "romeo"
              task, whose one action is n += 1.  n starts at 0, so the first
              "SV n=" reading says WHEN the value was interpolated: 0 if the
              task froze it on its way out, 1 if the whole turn's text waited
              for the flush, by which time the event had run.
    kilo      "K ball." -- one lone task, but the event's own task completes
              in the same turn, so this is the baseline: 2 walks if a task an
              event runs does not filter, 3 if it does.
    oscar     "O ball." plus three actions executing the silent sil1/sil2/sil3.
              Four completions and the event's: 6 walks under the ported model,
              2 if the Runner filters a fixed twice.
    november  "N ball." plus actions executing tex1 ("T1 ball.") and tex2
              ("T2 ball."), the sophie shape -- a sequence of completions that
              each print.
    mike      "M ball." -> lima "L ball." -> kilo "K ball.", executes nested
              three deep rather than in sequence.
    look      the room's Long "LONG ball.", library-printed, with no task of
              its own beyond the event's.

MEASURED 2026-08-24, run400.exe (Adrift_14.txt):

    sierra    SV n=0.
    sierra    SV n=1.                    (the same command, one turn later)
    kilo      K qqqball.
    oscar     O qqqqqqball.
    november  N qqqqqball.  T1 qqqqqball.  T2 qqqqball.
    mike      M qqqqqball.  L qqqqqball.  K qqqqqball.
    look      Probe Room / LONG qqball.
    papa      P qqqball.

Every count is "one walk per task that completed this turn, plus one for the
flush", with the event's silent romeo completing on every turn:

    look      romeo + flush                                       = 2
    kilo/papa the typed task + romeo + flush                      = 3
    oscar     itself + sil1 + sil2 + sil3 + romeo + flush          = 6
    november  itself + tex1 + tex2 + romeo + flush                = 5, and T2,
              printed one completion later than T1, catches 4
    mike      itself + lima + kilo + romeo + flush                = 5 for all
              three lines, which are all in the buffer by the time the
              innermost task returns

so a task an EVENT runs counts too (kilo is 3, not 2), nesting counts at every
depth, and the walk is over the turn's whole accumulated buffer rather than
over the text the completing task itself printed.  "SV n=0." on the first turn
adds the other half: the pass interpolates variables, so sierra's text froze n
before the event that turn had incremented it.

Scarier reproduces every cell.  The engine side is pf_refilter(), called at the
end of task_run_task_unrestricted() for 4.0 games, and the sophie.taf
consequence is real: its self-containing [north] -> [north (to the farmhouse)]
really is repeated once per completing task.

Usage:
    python3 make_400_walkcountprobe.py p4WALKCOUNT.plain
    python3 taftool.py pack p4WALKCOUNT.plain <donor.taf> p4WALKCOUNT.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("ALR walk-count probe.")
s(0)
ml("You have won.")

# GLOBAL
s("ALR Walk Count Probe 400")
s("SCARE probe")
s("I don't understand.")
s(2); s(0); s(0); s(1); s(0); s(0)
s("Player"); s(0); s("A test subject.")
s(0); s(0); s(0); s(0)
s(100); s(100)
s(0); s(0); s(0); s(0); s(0); s(0); s(0); s(0); s(0); s(0)
s(""); s(3); s(3); s(0)

# ROOMS -- one.
s(1)
s("Probe Room")
s("LONG ball.")
for _ in range(8):
    s(0)
s(0)                     # Alts count
s(0)                     # HideOnMap

# OBJECTS -- none.
s(0)

# TASKS
def task(cmd, text, actions=None):
    s(1); s(cmd)
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(3)                 # Where: all rooms
    s("")                # Question
    s(0)                 # Restrictions
    s(len(actions or []))
    for a in (actions or []):
        for field in a:
            s(field)
    s("")                # RestrMask

# Action type 3 = change variable: Var1 = variable, Var2 = 1 ("Var +="),
# Var3 = the amount, then $Expr and #Var5.
INC_N = (3, 0, 1, 1, "", 0)
# Action type 5 = execute/unset task: Var1 = 0 ("execute"), Var2 = the task.
def run(task_index):
    return (5, 0, task_index)

s(13)
task("romeo", "", actions=[INC_N])          # 0  -- the event's task, silent
task("sierra", "SV n=%n%.")                 # 1
task("sil1", "")                            # 2
task("sil2", "")                            # 3
task("sil3", "")                            # 4
task("oscar", "O ball.",
     actions=[run(2), run(3), run(4)])      # 5
task("tex1", "T1 ball.")                    # 6
task("tex2", "T2 ball.")                    # 7
task("november", "N ball.",
     actions=[run(6), run(7)])              # 8
task("kilo", "K ball.")                     # 9
task("lima", "L ball.", actions=[run(9)])   # 10
task("mike", "M ball.", actions=[run(10)])  # 11
task("papa", "P ball.")                     # 12

# EVENTS -- one, restarting every turn, running the silent "romeo".
s(1)
s("Turn event")          # Short
s(1)                     # StarterType: 1 = immediate.  The schema carries
                         # TaskNum only for StarterType 3, so none here.
s(1)                     # RestartType: 1 = restart immediately
s(0)                     # BTaskFinished
s(1); s(1)               # Time1, Time2 (length)
s("")                    # StartText
s("")                    # LookText
s("")                    # FinishText
s(3)                     # Where: all rooms
s(0); s(0)               # PauseTask, BPauserCompleted
s(0); s("")              # PrefTime1, PrefText1
s(0); s(0)               # ResumeTask, BResumerCompleted
s(0); s("")              # PrefTime2, PrefText2
s(0); s(0)               # Obj2, Obj2Dest
s(0); s(0)               # Obj3, Obj3Dest
s(0); s(0)               # Obj1, Obj1Dest
s(1)                     # TaskAffected: 1-based, task 0 ("romeo")

s(0)                     # NPCS
s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES
s(1)
s("n"); s(0); s("0")

ALRS = [("ball", "qball")]
s(len(ALRS))
for orig, repl in ALRS:
    s(orig); s(repl)

s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WALKCOUNT.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
