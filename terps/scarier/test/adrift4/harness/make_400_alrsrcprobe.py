#!/usr/bin/env python3
"""ADRIFT 4.0 probe: WHICH text does the Runner run the ALR list over twice?

make_400_alrprobe.py measured that run400 walks the ALR list twice where
run390 walks it once, using tasks for every cell.  The humbug (4.00)
transcripts then contradicted "twice, always": Adrift_30_humbug.txt has

    Okay.  Okay.  I put the sweet on the plinth.       (task 80's CompleteText)
    Okay.  I put the watch onto the rectangular table. (the library's own put)

from the one self-containing ALR `[I put ] -> [Okay.  I put ]`.  So the second
walk is not applied to everything the turn prints.  This probe separates the
kinds of text a turn can carry, with one self-containing ALR

    ball -> qball

and one room, one object and six tasks, so the q's count the walks:

    look        room Long "LONG ball RRR UUU AAA TOKEN.", printed by the
                library -- the extra chained tokens ask whether library text
                gets the same repeat-until-nothing-new walk task text gets
    take ball   the library's own take response, wording built by the engine
    x ball      object Description "TOKEN zebra."
    zulu        task CompleteText "You take the ball."
    yankee      CompleteText + AdditionalMessage, then a second typing of the
                non-repeatable task for its RepeatText
    xray        starts an event whose StartText and FinishText carry the token
    victor      "CT n=%n% TXT %w%." + AdditionalMessage, with an action that
                sets n to 9: does a task's own variable action reach the text
                it has already printed?
    uniform     "CTU ball." + AdditionalMessage, with an action that executes
                zulu: does a task an ACTION runs walk the whole buffer?
    tango       "CTT ball." with ShowRoomDesc, so the room's Long is printed
                as part of the same task

Two further cells ask how long an ALR stays "used up", which is the other half
of the model: SCARE marked each ALR as spent the first time it fired and would
not let it fire again for the rest of the filter call, and it filters the whole
turn's text in one call.  "zebra -> TOKEN" feeding "TOKEN -> tok" makes that
visible, since the second ALR only gets a chance on text the first one made:

    x ball      one string, "TOKEN zebra."     -> "tok TOKEN." if spent marks
                                                  exist, "tok tok." if not
    look        room Long "... TOKEN." and the object's InRoomDesc
                "EXTRA zebra." in the same turn -> "EXTRA TOKEN." if the mark
                is shared across the turn, "EXTRA tok." if each is filtered on
                its own

MEASURED 2026-08-24, run400.exe (Adrift_10 through Adrift_13.txt):

    look      Probe Room / LONG qball QQ done qAAA tok.  EXTRA tok.
    take ball Player take the qball.
    x ball    tok tok.
    zulu      You take the qqball.
    yankee    Y qqball.  ADD qqball.
    yankee    REP qball.
    xray      X.  EV qball.
    (2 turns) FIN qball.
    victor    CT n=9 TXT qqball.  AM n=9 TXT qqball.
    uniform   CTU qqqball.  You take the qqqball.  AMU qqball.
    tango     CTT qqball. / Probe Room / LONG qqball QQ done qqAAA tok.

Read as a model, cell by cell:

  * "tok tok." and "EXTRA tok." -- there are NO spent marks between different
    ALRs; a chain runs to its end, in one walk, wherever the text came from.
    Only a SELF-containing ALR is held back, and only for the walk it fired
    in (one q per walk on ball -> qball, one q per walk on AAA -> qAAA).
  * one q on the plain "look", two on everything a task printed, three on
    "uniform" -- the turn's whole accumulated buffer is walked once at the end
    of EVERY task that completes and once more at the flush.  "uniform" is
    two completions (itself and the zulu its action ran) plus the flush; its
    AdditionalMessage, printed after the actions, catches only two.
  * "REP qball." -- refusing to repeat a non-repeatable task is not a
    completion, so that turn gets the flush walk alone.
  * "CT n=9" -- a filter pass interpolates variables as well, and a task's own
    change-variable action reaches text the task has already printed.  So 4.0
    must NOT checkpoint the buffer before changing a variable, which is what
    pre-4.0 does (task_run_change_variable_action()).
  * "tango" -- text the library prints for a task (here ShowRoomDesc's room
    description) is inside that task's completion, hence two walks, and the
    second q on AAA proves the self-containing retirement is per-walk and not
    per-turn.

make_400_walkcountprobe.py takes the count further (nested three deep, and a
task run by an event's TaskAffected), and make_400_varfreezeprobe.py asks the
matching question of the variables.

Usage:
    python3 make_400_alrsrcprobe.py p4ALRSRC.plain
    python3 taftool.py pack p4ALRSRC.plain <donor.taf> p4ALRSRC.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("ALR source probe.")
s(0)
ml("You have won.")

# GLOBAL
s("ALR Source Probe 400")
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
s("LONG ball RRR UUU AAA TOKEN.")
for _ in range(8):
    s(0)
s(0)                     # Alts count
s(0)                     # HideOnMap

# OBJECTS -- one takeable ball, in the room.
s(1)
s("a")                   # Prefix
s("ball")                # Short
s(0)                     # V$Alias count
s(0)                     # Static
s("TOKEN zebra.")        # Description
s(4)                     # InitialPosition: 4 + room -> room 0
s(0); s(0); s("")        # Task, TaskNotDone, AltDesc
s(0); s(0); s(0)         # Container, Surface, Capacity
s(0); s(0); s(0)         # Wearable, SizeWeight, Parent
s(0); s(0); s(0); s(0); s(0)   # Openable, SitLie, Edible, Readable, Weapon
s(0); s(0)               # CurrentState, ListFlag
s("EXTRA zebra."); s(0)  # InRoomDesc, OnlyWhenNotMoved

# TASKS
def task(cmd, text, addmsg="", repeat=1, reptext="", actions=None, srd=0):
    s(1); s(cmd)
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s(reptext)           # RepeatText
    s(addmsg)            # AdditionalMessage
    s(srd)               # ShowRoomDesc
    s(repeat)            # Repeatable
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

# Action type 3 = change variable; Var1 = variable, Var2 = 0 ("Var ="),
# Var3 = the value, then $Expr and #Var5.
SET_N_TO_9 = (3, 0, 0, 9, "", 0)
# Action type 5 = execute/unset task; Var1 = 0 ("execute"), Var2 = the task.
# Task 0 is zulu, whose own CompleteText prints, so where that lands in the
# output says whether a task's actions run before or after its CompleteText
# is printed.
RUN_ZULU = (5, 0, 0)

s(6)
task("zulu", "You take the ball.")
task("yankee", "Y ball.", addmsg="ADD ball.", repeat=0, reptext="REP ball.")
task("xray", "X.")
task("victor", "CT n=%n% TXT %w%.", addmsg="AM n=%n% TXT %w%.",
     actions=[SET_N_TO_9])
task("uniform", "CTU ball.", addmsg="AMU ball.", actions=[RUN_ZULU])
task("tango", "CTT ball.", srd=1)

# EVENTS -- one, started by task 3 (xray), so it fires inside the transcript.
s(1)
s("Probe event")         # Short
s(3)                     # StarterType: 3 = after a task
s(3)                     # TaskNum (1-based)
s(0)                     # RestartType
s(0)                     # BTaskFinished
s(2); s(2)               # Time1, Time2 (length)
s("EV ball.")            # StartText
s("")                    # LookText
s("FIN ball.")           # FinishText
s(3)                     # Where: all rooms
s(0); s(0)               # PauseTask, BPauserCompleted
s(0); s("")              # PrefTime1, PrefText1
s(0); s(0)               # ResumeTask, BResumerCompleted
s(0); s("")              # PrefTime2, PrefText2
s(0); s(0)               # Obj2, Obj2Dest
s(0); s(0)               # Obj3, Obj3Dest
s(0); s(0)               # Obj1, Obj1Dest
s(0)                     # TaskAffected

s(0)                     # NPCS
s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES -- a number the victor task changes, and a text one holding an
# ALR original, so its interpolated value shows how many walks it caught.
s(2)
s("n"); s(0); s("5")
s("w"); s(1); s("ball")

ALRS = [("ball", "qball"),
        ("AAA", "qAAA"),
        ("PPPP", "QQ"),
        ("RRR", "PPPP"),
        ("WWWWW", "done"),
        ("VVVV", "WWWWW"),
        ("UUU", "VVVV"),
        ("TOKEN", "tok"),
        ("zebra", "TOKEN")]
s(len(ALRS))
for orig, repl in ALRS:
    s(orig); s(repl)

s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4ALRSRC.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
