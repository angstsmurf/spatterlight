#!/usr/bin/env python3
"""ADRIFT 4.0 probe: does a NESTED task's completion freeze variables too?

make_400_walkcountprobe.py measured that a version 4.0 turn's accumulated text
is filtered once at the end of every task that completes -- including tasks an
action executes and tasks an event's TaskAffected runs -- and once more at the
flush.  Its "sierra" cell also measured that such a pass interpolates
variables, freezing them: the task printed "SV n=0." on a turn whose event
afterwards made n 1.

Ported, those two halves together predict something a real game trips over.
3monkeys.taf's "chimp" task prints [CHIMPSIGNAL=%signal_to_chimp%] -- an ALR
original built from the variable -- and only then, after an action that runs
another (silent) task, increments the variable.  If the silent task's
completion interpolates, the text freezes as "CHIMPSIGNAL=0", no ALR matches,
and the player is shown the raw token instead of the prose.  So either the
Runner really does print that, or variable interpolation is not part of the
per-task pass and only the ALR walk is.

The cells, n starting at 5 and each SET putting it to 9, with the one ALR
[ball] -> [qball] so the q's still count the walks:

    bravo     "B n=%n% ball."  + action SET.  The baseline, already measured on
              make_400_alrsrcprobe.py's "victor" cell as n=9: a task's own
              completion is late enough to see its own action's new value.
    alpha     "A n=%n% ball."  + actions [execute silent task, SET].  n=5 if
              the silent nested completion interpolated on its way out (the
              3monkeys shape), n=9 if it left the token alone for the flush.
    charlie   "C n=%n% ball."  + actions [execute PRINTING task, SET], to ask
              the same of a nested task that has text of its own -- the case
              the walk count was measured on.
    delta     "D n=%n% ball."  + actions [SET, execute silent task]: the change
              comes first, so n=9 whatever the answer, and a 5 here would mean
              the reading order is not what this probe assumes at all.

MEASURED 2026-08-24, run400.exe (Adrift_15.txt):

    bravo     B n=9 qqball.
    alpha     A n=5 qqqball.
    charlie   C n=5 qqqball.  PT qqqball.
    delta     D n=9 qqqball.

So the answer is the first one: a nested task's completion interpolates the
whole buffer, silent or not, and a variable changed after it is too late.  The
q counts agree -- two walks where nothing nested, three where something did.

And the Runner really does print the raw token in 3monkeys.  Measured the same
day on the game itself (Adrift_16.txt, run400.exe, the first 36 commands of
goldens/3monkeys_solution.txt, every one of them echoed):

    chimp, get coconut
    CHIMPSIGNAL=0
    The chimpanzee scans the ground immediately near his feet, but there are
    no fallen coconuts to be seen.

Task 145 prints [CHIMPSIGNAL=%signal_to_chimp%], runs the silent task 340 to
work out where the player is, and only then increments the variable -- so the
first signal is frozen at 0, no ALR has an original for that, and the prose the
author wrote for CHIMPSIGNAL=1 turns up one signal late.  The goldens were
re-blessed to match (2026-08-24); the walkthrough still wins.

Usage:
    python3 make_400_varfreezeprobe.py p4VARFREEZE.plain
    python3 taftool.py pack p4VARFREEZE.plain <donor.taf> p4VARFREEZE.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Variable freeze probe.")
s(0)
ml("You have won.")

# GLOBAL
s("Variable Freeze Probe 400")
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

# Action type 3 = change variable: Var1 = variable, Var2 = 0 ("Var ="),
# Var3 = the value, then $Expr and #Var5.
SET_9 = (3, 0, 0, 9, "", 0)
SET_5 = (3, 0, 0, 5, "", 0)
# Action type 5 = execute/unset task: Var1 = 0 ("execute"), Var2 = the task.
def run(task_index):
    return (5, 0, task_index)

s(7)
task("silentt", "")                                    # 0
task("printt", "PT ball.")                             # 1
task("alpha", "A n=%n% ball.", actions=[run(0), SET_9])    # 2
task("bravo", "B n=%n% ball.", actions=[SET_9])            # 3
task("charlie", "C n=%n% ball.", actions=[run(1), SET_9])  # 4
task("delta", "D n=%n% ball.", actions=[SET_9, run(0)])    # 5
task("reset", "R.", actions=[SET_5])                   # 6

# EVENTS -- none, so nothing but the typed task completes.
s(0)

s(0)                     # NPCS
s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES
s(1)
s("n"); s(0); s("5")

ALRS = [("ball", "qball")]
s(len(ALRS))
for orig, repl in ALRS:
    s(orig); s(repl)

s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4VARFREEZE.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
