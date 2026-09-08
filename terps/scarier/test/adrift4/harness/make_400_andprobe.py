#!/usr/bin/env python3
"""ADRIFT 4.0 probe: where does the Runner cut a typed line in two?

The p4PUT probes left two cells unexplained: `put coin in zzz and yyy`
printed "I don't understand what you want to put things inside." AND the
DontUnderstand text, and `put coin in jar and zzz` printed the ambiguity
prompt AND "That is still ambiguous!".  The WINE-TRANSCRIPTS-TODO write-up
blamed put_drop_list's own clause loop (459C75), but that loop keeps its
leftover in a local and could never have reached the answer slot.

The real cutter is one step higher.  generaltasks calls the line splitter
Proc_19_60_459764 four times, once per separator (48A0DA-48A10C):

    ","      ". "      " and "      " then "

Each call finds the FIRST occurrence of its separator, cuts the line there,
and pushes the tail onto the pending-command string MemVar_4942E4 (joined
with ", " when something is queued already), so the tail is run as its own
command with its own turn.  What makes it interesting is the suppression
loop at 45951A-4595EA: before it cuts, it takes the first word of the tail
(Proc_19_59_449980) and walks the WHOLE object table -- every object, no
scope test -- comparing that word against

    obj.Short          (field 4, whole)
    words of obj.Prefix (field 0, Split on " ")
    every obj.Alias    (field 8, count field 12)

and if any of them matches it gives up on that occurrence and looks for the
next one.  That is what keeps `get coin and hat` a single command: an object
list continues with a name, an alias or an article, so a separator followed
by one of those is not a separator.  The comparisons carry no LCase (the
character rewrite at 48A159 has one, so the omission is deliberate) and the
line was lower-cased at read, so an authored capital in a Short should make
the object invisible to the suppression test.

Cells (a length-1 self-restarting event prints `TICK.` at the end of every
counted turn, so the transcript also says how many turns each line became):

   1  get coin and hat          both objects -- expect ONE command
   2  get coin and zzz          tail is not an object -- expect a cut
   3  x coin then x hat         " then "
   4  x coin and x hat          tail starts with a verb, not an object
   5  x coin, x hat             comma control
   6  x coin then x hat and x box   two cuts, and the order they come back
   7  x sphere and ball         alias, then a Short's second word
   8  x coin and a hat          "a" is every object's Prefix here
   9  x coin and the hat        "the" is the ball's Prefix
  10  x coin and widget         Short authored "Widget", line is lower case
  11  x coin and Widget         the same line the player cannot type
  12  wave coin and hat         a task whose command holds " and " + object
  13  wave zzz and yyy          a task whose command holds " and " + unknown
  14  put coin in box and hat in desk   put_drop_list's clause loop proper
  15  put coin in box and hat   a trailing clause with no preposition
  16  x large                   a bare Prefix word
  17  drop coin and hat         the drop side of the list

Measured 2026-09-08, Adrift_955 / Adrift_956 / Adrift_957.  The suppression
loop is real and case-sensitive, the cuts are separate TURNS, and a failed
element does not discard the rest of the line (the queue is drained below
every DontUnderstand exit).  put_drop_list's own loop turned out to be much
narrower: it sees only an " and " at or beyond the preposition split, runs
its clauses inside ONE turn with no separator between their answers, drops a
trailing clause that has no preposition of its own, and hoists an implicit
take above everything (name_object prints that straight to the textbox).
Left unported: name_object's in-command "and"/"all" list loops at 46E04E /
46E0B2.  Write-up in notes/WINE-TRANSCRIPTS-TODO.md.

Usage:
    python3 make_400_andprobe.py p4AND.plain
    python3 taftool.py pack p4AND.plain <donor.taf> p4AND.taf
    (then, from ~/adrift-battle/runner/wine)
    sh fast.sh p4AND.taf cmdfile_and.txt run400
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Line-splitter probe.")
s(0)                     # StartRoom: Alpha
ml("You have won.")

# GLOBAL
s("And Probe 400")
s("SCARE probe")
s("NO IDEA.")
s(1)                     # Perspective
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player"); s(0); s("A test subject.")
s(0)                     # Task
s(0); s(0); s(0)         # Position, ParentObject, PlayerGender
s(100); s(100)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0); s(0); s(0)         # NoDebug, NoScoreNotify, NoMap
s(0); s(0); s(0)         # NoAutoComplete, NoControlPanel, NoMouse
s(0); s(0)               # Sound, Graphics
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS
def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0); s(0)           # Alts, HideOnMap

s(2)
room("Alpha", "The first room.", {1: 2})
room("Bravo", "The second room.", {3: 1})

# OBJECTS
def obj(short, room=None, static=False, prefix="a", aliases=(),
        container=0, surface=0, capacity=0, wearable=0):
    s(prefix); s(short)
    s(len(aliases))
    for a in aliases:
        s(a)
    s(1 if static else 0)
    s("A plain thing.")
    s(0 if static else room + 4)         # InitialPosition
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    if static:
        s(1); s(room + 1)                # Where: ROOM_LIST1 Type 1, 1-based
    s(container); s(surface); s(capacity)
    if not static:
        s(wearable); s(0); s(0)          # Wearable, SizeWeight, Parent
    s(0)                 # Openable (0: a container is simply open)
    s(0)                 # SitLie
    if not static:
        s(0)             # Edible
    s(0)                 # Readable
    if not static:
        s(0)             # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(5)
obj("coin",     room=0)                                        # 0 dynamic
obj("hat",      room=0, wearable=1)                            # 1 dynamic
obj("desk",     room=0, static=True, surface=1, capacity=5)    # 2 surface
obj("box",      room=0, static=True, container=1, capacity=5)  # 3 container
obj("red ball", room=0, prefix="the large",                    # 4 alias/prefix
    aliases=("sphere", "Widget"))

# TASKS -- two commands carrying " and ", one continued by an object and one
# not.  Nothing else in the game matches, so a task that never fires is
# visible as the DontUnderstand text.
def task(cmd, text):
    s(1); s(cmd)
    s(text)
    s(""); s(""); s("")  # Reverse/Repeat/Additional
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable: YES
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand
    s(3)                 # Where: anywhere
    s("")                # Question
    s(0); s(0)           # Restrictions, Actions
    s("")                # RestrMask

s(2)
task("wave coin and hat", "C1 waved both.")
task("wave zzz and yyy",  "C2 waved neither.")

# EVENTS -- length 1, restarts immediately: FinishText marks every counted turn.
s(1)
s("Ticker")
s(1)                     # StarterType: 1 = immediate
s(1)                     # RestartType: 1 = restart immediately
s(0)                     # TaskFinished
s(1); s(1)               # Time1, Time2
s(""); s(""); s("TICK.") # StartText, LookText, FinishText
s(3)                     # Where: all rooms
s(0); s(0)               # PauseTask, PauserCompleted
s(0); s("")              # PrefTime1, PrefText1
s(0); s(0)               # ResumeTask, ResumerCompleted
s(0); s("")              # PrefTime2, PrefText2
s(0); s(0); s(0); s(0); s(0); s(0)   # Obj2/Dest Obj3/Dest Obj1/Dest
s(0)                     # TaskAffected

# NPCS
s(0)

s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
args = [a for a in sys.argv[1:] if not a.startswith("--")]
out = args[0] if args else "p4AND.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
