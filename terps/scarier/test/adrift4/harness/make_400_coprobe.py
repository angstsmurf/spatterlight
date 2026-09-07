#!/usr/bin/env python3
"""ADRIFT 4.0 probe: does 4.0 raise the co() object-ambiguity prompt, and from
where?

`lca_solution.txt` turn 91 is the one line of that 261-command replay where
run400 and scarier disagree:

    > chop tree
    run400   Which tree.  The tree or the tree?
    scarier  Whatever you're trying to do, you can't. ...   (the game's own
                                                             DontUnderstand)

`chop` appears in no task in Lights_Camera_Action.taf, so run400 raised that
prompt on a line NOTHING claimed -- and the next command (`n`) was not eaten
as an answer, it simply moved the player north.

scarier has the rule already (`lib_co_ambiguity_prompt()`), ported from
run380's generaltasks() scan @441D5D + end-of-turn replacement @4431B0, but
gated `< 3.90`: the note there says run390 kept co() only for characters()
and sitstand(), and that 4.0 "never raises this prompt from the dispatcher".
lca says that last clause is wrong.  What is not yet known is WHICH lines 4.0
raises it on, so this probe walks the handlers one at a time.

Two rooms.  Alpha holds seven objects, Bravo one:

    object 0  "a red tree"      Alpha    \\ two present namesakes: ambiguous
    object 1  "a blue tree"     Alpha    /  by Short, distinct noun phrases
    object 2  "a rock"          Alpha       unique, the control
    object 3  "a mustang key"   Alpha    \\ ambiguous by ALIAS only, both
       alias "keys"                      /  Shorts being different
    object 4  "a truck key"     Alpha    /
       alias "keys"
    object 5  "a hut"           Alpha    \\ ambiguous by one Short and one
       alias "shed"                      /  alias: the mixed case
    object 6  "a shed"          Alpha    /
    object 7  "the tree"        Bravo       a THIRD tree, never present with
                                            the other two

and one task, `poke %object%`, answering "POKE."  DontUnderstand is set to
the unmistakable "NO IDEA." so a refusal can never be confused with a prompt.

The eleven commands and what each one asks:

     1  look            baseline; Alpha lists five objects
     2  chop tree       UNKNOWN verb, ambiguous by Short.  This is the lca
                        cell.  Prompt = 4.0 runs the scan from the dispatcher
                        like 3.8; "NO IDEA." = it does not and lca's prompt
                        came from somewhere else.
     3  x tree          LIBRARY examine, ambiguous.  4.0's examine goes
                        through the up-front resolver Proc_21_58_463640,
                        whose tie answer is "see no such thing" -- so a
                        prompt here would mean the co() scan outranks it.
     4  poke tree       a TASK whose pattern is `%object%`, ambiguous.  In
                        3.8 a task claiming the line suppresses the prompt.
     5  poke rock       control: the task on an unambiguous noun.
     6  chop rock       control: unknown verb, unambiguous noun -> "NO IDEA."
     7  chop keys       unknown verb, ambiguous by ALIAS.  Says whether the
                        4.0 scan reads aliases (the 3.8 one does).
     8  x keys          examine, ambiguous by alias.
     9  e               to Bravo.
    10  chop tree       unknown verb, ONE tree present and two absent.  Says
                        whether the scan is presence-filtered.
    11  look            baseline for Bravo.

Command 3 doubles as the "is the next line eaten" control: it follows the
prompt line directly, so if 4.0 swallows an answer the way a disambiguation
question would, `x tree` never gets its own `> ` echo.

Six feeds were run against this one .taf; each later cmdfile isolates its
cells with a neutral `look` and answers the question it opened:

    cmdfile_co.txt   the eleven commands above          Adrift_924
    cmdfile_co2.txt  which handlers prompt, and on
                     Short vs alias ties                Adrift_925/926
    cmdfile_co3.txt  what the pending answer slot
                     accepts (`red`, `zzz`, `mustang`)  Adrift_927
    cmdfile_co4.txt  whole-word names ("key", "mustang"
                     name nothing) and a bare noun      Adrift_928
    cmdfile_co5.txt  two nouns in one line: what is
                     listed, and which term is used     Adrift_929
    cmdfile_co6.txt  answering with a listed object's
                     own name; `rock` vs `x rock`       Adrift_930

The measurements and the rule they add up to are written up in
notes/WINE-TRANSCRIPTS-TODO.md and in the lib_co_400_*() block comment in
sclibrar.cpp; they were ported 2026-09-07.

Usage:
    python3 make_400_coprobe.py p4CO.plain
    python3 taftool.py pack p4CO.plain <donor.taf> p4CO.taf

Drive it with, from ~/adrift-battle/runner/wine:
    sh fast.sh p4CO.taf cmdfile_co.txt run400
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Object-ambiguity probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("CO Probe 400")
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

s(8)
obj("tree", 0, prefix="a red")                  # 0 -> Alpha, Short ambiguity
obj("tree", 0, prefix="a blue")                 # 1 -> Alpha, but distinct NPs
obj("rock", 0)                                  # 2 -> Alpha, unique
obj("mustang key", 0, aliases=("keys",))        # 3 -> Alpha, alias only
obj("truck key",   0, aliases=("keys",))        # 4 -> Alpha, alias only
obj("hut",  0, aliases=("shed",))               # 5 -> Alpha, alias "shed"
obj("shed", 0)                                  # 6 -> Alpha, Short "shed"
obj("tree", 1, prefix="the")                    # 7 -> Bravo, never co-present

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

s(1)
task("poke %object%", "POKE.")

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
out = sys.argv[1] if len(sys.argv) > 1 else "p4CO.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
