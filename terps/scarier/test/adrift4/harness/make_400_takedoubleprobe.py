#!/usr/bin/env python3
"""ADRIFT 4.0 probe: does a textless take-family task run TWICE?

British Fox T318 `get grace`: the Runner prints the text of task 123 twice,
Scarier once.  Task 283 `get grace` is repeatable, has no text of its own and
executes task 123, whose CompleteText is the printed line.  Reading run400
get_piece (473A34): at 472DC8, on a line with no whole-word "all" or "and",
it pre-matches the typed line (453C50) and dispatches the task (44CCE0,
472DE7), but claims the line only when the pre-matcher answered 1 -- "the
matched direction has text".  A task without its own text answers 2, so
get_piece carries on, its later pre-match at 473241 exits silently, and the
dispatcher at 48A481 runs the typed line a second time.

The cells (n counts runs; `count` prints it):

    get bob     textless, repeatable, n += 1, executes "BOBTEXT."
    get cat     own text "CAT.", repeatable, n += 1
    poke bob    textless, repeatable, not take-family, executes "BOBTEXT."
    get dog     textless, NOT repeatable, n += 1, executes "BOBTEXT."
    get eel     textless, repeatable, only n += 1 -- no text anywhere

Bob is an NPC; cat, dog and eel name nothing in the game.

Measured (run400x, Adrift_282_p4tdbl.txt, 2026-09-24):

    get bob     "BOBTEXT.  BOBTEXT."   n += 2   (both runs)
    get cat     "CAT."                 n += 1   (text of its own: claimed)
    poke bob    "BOBTEXT."             n += 1   (not a take: dispatcher only)
    get dog     "BOBTEXT."             n += 1   (spent by the first run)
    get dog     "Take what?"           n += 0
    get eel     "I don't understand."  n += 2   (both runs, nothing printed)

A task the first run spends misses the 473241 pre-match, so get_piece goes on
to 47332B, whose "Take what?" only speaks into an empty buffer (FunHouse T2
`pick up money`, once-only and silent: "Take what?").  Ported in
run_all_commands() (scrunner.cpp, "get_piece dispatches a pre-matched task").

--objects (p4TDBO) asks the same with the noun naming a DYNAMIC object in
reach, which get_piece's resolver finds after the 472DE7 dispatch:

    get ball    ball on the floor; textless, repeatable, executes BOBTEXT
    get cup     cup on the floor; own text "CUPTEXT."
    get pen     pen on the floor; textless, NOT repeatable, executes BOBTEXT
    get rock    rock on the floor; textless, repeatable, n += 1 only
    get gem     gem HELD; textless, repeatable, executes BOBTEXT

`i` after each says whether the library take ran too.

Measured (run400x, Adrift_282_p4tdbo.txt, 2026-09-24) -- ONE run, then the
library take on the resolved object:

    get ball    "Player take the ball.  BOBTEXT."      n += 1, ball taken
    get cup     "CUPTEXT."                             n += 1, not taken
    get pen     "Player take the pen.  BOBTEXT."       n += 1, pen taken
    get pen     "Player is already carrying the pen."  n += 0 (spent)
    get rock    "Player take the rock."                n += 1
    get gem     "BOBTEXT.Player is already carrying the gem."   n += 1

A take puts its line in front of the task's text (47359A saves the buffer
and pspace()s it back); a refusal is appended with no separator at all.

Usage:
    python3 make_400_takedoubleprobe.py p4TDBL.plain
    python3 make_400_takedoubleprobe.py --objects p4TDBO.plain
    python3 taftool.py pack p4TDBL.plain <donor.taf> p4TDBL.taf
"""
import sys

OBJECTS = "--objects" in sys.argv[1:]

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Take double probe.")
s(0)
ml("You have won.")

# GLOBAL
s("Take Double Probe 400")
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
s("A bare room.")
for _ in range(8):
    s(0)
s(0)                     # Alts count
s(0)                     # HideOnMap

# OBJECTS.  InitialPosition 1 is held by the player, 4 + n room n
# (0-based); see make_400_takeprobe.py.
def obj(short, position):
    s("a"); s(short)
    s(0)                 # V$Alias count
    s(0)                 # static: no
    s("A plain thing.")
    s(position)          # InitialPosition
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(0); s(0); s(0)     # Container, Surface, Capacity
    s(0); s(0); s(0)     # Wearable, SizeWeight, Parent
    s(0)                 # Openable
    s(0)                 # SitLie
    s(0)                 # Edible
    s(0)                 # Readable
    s(0)                 # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

if OBJECTS:
    s(5)
    for name in ("ball", "cup", "pen", "rock"):
        obj(name, 4)
    obj("gem", 1)
else:
    s(0)

# TASKS
def task(cmd, text, actions=(), repeatable=1):
    s(1); s(cmd)
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(repeatable)        # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(3)                 # Where: all rooms
    s("")                # Question
    s(0)                 # Restrictions
    s(len(actions))
    for a in actions:
        for field in a:
            s(field)
    s("")                # RestrMask

INC_N = (3, 0, 1, 1, "", 0)      # variable 0 += 1
def run(task_index):             # execute task (0-based index)
    return (5, 0, task_index)

if OBJECTS:
    s(7)
    task("count", "N=%n%.")                                   # 0
    task("#bobtext", "BOBTEXT.")                              # 1
    task("get ball", "", actions=[INC_N, run(1)])             # 2
    task("get cup", "CUPTEXT.", actions=[INC_N])              # 3
    task("get pen", "", actions=[INC_N, run(1)], repeatable=0)  # 4
    task("get rock", "", actions=[INC_N])                     # 5
    task("get gem", "", actions=[INC_N, run(1)])              # 6
else:
    s(7)
    task("count", "N=%n%.")                                   # 0
    task("#bobtext", "BOBTEXT.")                              # 1
    task("get bob", "", actions=[INC_N, run(1)])              # 2
    task("get cat", "CAT.", actions=[INC_N])                  # 3
    task("poke bob", "", actions=[INC_N, run(1)])             # 4
    task("get dog", "", actions=[INC_N, run(1)], repeatable=0)  # 5
    task("get eel", "", actions=[INC_N])                      # 6

# EVENTS -- none.
s(0)

# NPCS -- Bob, here, no walks.
s(1)
s("Bob")                 # Name
s("")                    # Prefix
s(0)                     # V$Alias count
s("A test NPC.")         # Descr
s(1)                     # StartRoom (1-based)
s("")                    # AltText
s(0)                     # Task
s(0)                     # Topics
s(0)                     # Walks
s(0)                     # ShowEnterExit
s("Bob is standing here.")  # InRoomText
s(0)                     # Gender

s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES
s(1)
s("n"); s(0); s("0")

s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
args = [a for a in sys.argv[1:] if not a.startswith("--")]
out = args[0] if args else ("p4TDBO.plain" if OBJECTS else "p4TDBL.plain")
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
