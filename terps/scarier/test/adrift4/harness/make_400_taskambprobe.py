#!/usr/bin/env python3
r"""ADRIFT 4.0 probe: is a line a TASK answered, and that names an object term
two or more present objects share, a turn?

cyber2 (4.00) T15 `give electric uniform to lightning` runs task 6 and makes
no draw in run400x -- no event restart, no battle tick -- while the player
holds the Electric Uniform and wears the Fire Uniform (both Short "Uniform",
alias "Suit").  The character twin is measured and ported
(lib_npc_400_line_names_namesakes, Sun Empire); the object twin was not.

`turns` is administrative and prints the Runner's counter, so each cell is
`turns` / <cell> / ... and the counter says which cells were turns.

Objects (static, room Alpha unless noted):
    0 a red box      \ shared Short "box"
    1 a blue box     /
    2 a green ball   \ shared alias "toy" only
    3 a yellow cube  /
    4 a rock           unique control
    5 a crate          Alpha  \ shared Short, one namesake absent
    6 a crate          Bravo  /
Tasks (repeatable, all rooms, no restrictions):
    poke red box -> POKE RED.   poke rock -> POKE ROCK.   poke toy -> POKE TOY.
    poke crate -> POKE CRATE.   wave * -> WAVE.   rub %object% -> RUB.
Usage:
    python3 make_400_taskambprobe.py p4TAMB.plain
    python3 taftool.py pack p4TAMB.plain p4CO.taf p4TAMB.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Task ambiguity probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("TAMB Probe 400")
s("SCARE probe")
s("NO IDEA.")            # DontUnderstand -- deliberately unmistakable
s(1)                     # Perspective: second person
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
s(0); s(0)               # Sound, Graphics -- both off, so no resource fields
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS -- Alpha east to Bravo, Bravo west to Alpha.
def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0)                 # Alts
    s(0)                 # HideOnMap

s(2)
room("Alpha", "The first room.", {1: 2})
room("Bravo", "The second room.", {3: 1})

# OBJECTS -- all static, so they stay put and the room lists them; the
# question here is name resolution, not carrying.
def obj(short, room, aliases=(), prefix="a"):
    s(prefix)            # Prefix
    s(short)             # Short
    s(len(aliases))      # V$Alias count
    for a in aliases:
        s(a)
    s(1)                 # Static
    s("A plain thing.")  # Description
    s(0)                 # InitialPosition -- unused for a static object
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(1); s(room + 1)    # Where: ROOM_LIST1 Type 1 = one room, Room 1-based
    s(0); s(0); s(0)     # Container, Surface, Capacity
    s(0)                 # Openable: not openable, so no Key
    s(0)                 # SitLie
    s(0)                 # Readable
    s(0)                 # CurrentState: 0, so no States/StateListed
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(7)
obj("box", 0, prefix="a red")                   # 0
obj("box", 0, prefix="a blue")                  # 1
obj("green ball", 0, aliases=("toy",))          # 2
obj("yellow cube", 0, aliases=("toy",))         # 3
obj("rock", 0)                                  # 4
obj("crate", 0)                                 # 5
obj("crate", 1)                                 # 6

# TASKS -- one, `poke %object%`.
def task(cmd, text):
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
    s(0)                 # Actions
    s("")                # RestrMask

s(6)
task("poke red box", "POKE RED.")
task("poke rock", "POKE ROCK.")
task("poke toy", "POKE TOY.")
task("poke crate", "POKE CRATE.")
task("wave *", "WAVE.")
task("rub %object%", "RUB.")

# EVENTS -- one that prints "TICK." over and over, so that a turn whose
# output the prompt REPLACES (3.8 threw the whole turn away, run380 @4431B0)
# can be told from a turn the prompt is merely appended to.  Field order is
# the v4 EVENT schema in sctafpar.cpp:144.
s(1)
s("ticker")              # Short
s(1)                     # StarterType: 1 = immediate
s(2)                     # RestartType: 2 = restart after Time1/Time2 turns
s(0)                     # TaskFinished
s(1); s(1)               # Time1, Time2 -- one turn, every turn
s("")                    # StartText
s("")                    # LookText
s("<br>TICK.")           # FinishText
s(3)                     # Where: ROOM_LIST0 Type 3 = all rooms
s(0)                     # PauseTask
s(0)                     # PauserCompleted
s(0); s("")              # PrefTime1, PrefText1
s(0)                     # ResumeTask
s(0)                     # ResumerCompleted
s(0); s("")              # PrefTime2, PrefText2
s(0); s(0)               # Obj2, Obj2Dest
s(0); s(0)               # Obj3, Obj3Dest
s(0); s(0)               # Obj1, Obj1Dest
s(0)                     # TaskAffected

# NPCS -- none.
s(0)

s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES -- none.
s(0)

s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4TAMB.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
