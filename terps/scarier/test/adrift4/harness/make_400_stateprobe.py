#!/usr/bin/env python3
"""ADRIFT 4.0 probe: HOW MUCH of an object state name does run400 lower-case?

The X-Files replay (`Adrift_22_xfiles.txt`, 2026-08-23) showed run400 printing
an object's state in lower case where scarier prints the `States` pipe-list
verbatim: "switched off", "switch in the on position".  But every state that
transcript reaches is already lower-case after its first letter, so the
evidence only pins the FIRST CHARACTER.  `LCase()` over the whole string and
"lower the first letter" cannot be told apart from it, and the corpus has
states where the difference is destructive:

    in the UP position     TheADRIFTProject
    facing South           The_Hunter
    Sur la gauche          Les Feux de l'enfer
    R1 .. R7               Oh_Human
    Locked off             baroo

so scarier still prints all of them verbatim (`obj_state_name()` in
`scobjcts.cpp`; its three callers -- `lib_list_object_state`, `%obstate%` and
`%state_X%` -- are all print sites, and nothing in the engine compares a state
name, so whatever this probe says, that one function is the single place to
change).  run400's game-logic literals live in a runtime table that neither
`run400.p32dasm.txt` nor `run400-analysed/Form1.frm` resolves, so the P-code
cannot answer it either.  Measure it.

Six stateful objects in one room, each carrying a state whose SECOND word
would survive a first-letter-only rule and die under a whole-string `LCase`:

    lever    In the UP position   |  In the DOWN Position
    compass  Facing South         |  Facing North
    hatch    Locked Off           |  Open Wide
    panel    R1                   |  R2
    sign     Sur la gauche        |  A droite
    switch   switched off         |  switched on      <- the xfiles control

`switch` is the control: it is already all lower case, so it must come back
unchanged under either rule, and a difference there would mean the probe
itself is wrong.  `panel` is the sharpest cell -- "R1" has nothing but the
capital, so whole-string `LCase` gives "r1" and first-letter-only gives "r1"
as well.  (Keep it anyway: if run400 answers "R1" it lower-cases NEITHER, and
the whole reading of the xfiles line is wrong.)

Each object is read three ways, because the three callers are three separate
sites in run400 too and need not agree:

    x <obj>     the examine lister  (BStateListed is on for all six)
    ob <obj>    a task whose CompleteText is  OB=[%obstate%]
    st<n>       a task whose CompleteText is  ST=[%state_<obj>%]

and `flip` then moves the lever to its second state so the same three reads
can be repeated on a state the game switched to rather than started in.

MEASURED AND CLOSED 2026-08-25, Adrift_1_p4state.txt, all 29 commands echoed.
The answer is that the three callers do NOT agree, and only one of them folds:

    States entry           x <obj>              %obstate%            %state_X%
    In the UP position     In the UP position   In the UP position   in the up position
    Facing South           Facing South         Facing South         facing south
    Locked Off             Locked Off           Locked Off           locked off
    R1                     R1                   R1                   r1
    Sur la gauche          Sur la gauche        Sur la gauche        sur la gauche
    switched off           switched off         switched off         switched off
    In the DOWN Position   In the DOWN Position In the DOWN Position in the down position

`mid <obj>` answers the same as `st <obj>` on every row, so it is not a
line-opening rule; and "UP" and "R1" both lose their capitals, so it is not a
first-letter rule either.  %state_<obj>% is LCase()d whole and nothing else
is.  So obj_state_name() in scobjcts.cpp was the WRONG place to change --
the fold went into the state_ branch of scvars.cpp, its only caller that
wants it.  where_are_my_keys_solution.txt re-blessed, three lines, all three
independently visible in Adrift_23_where_are_my_keys.txt.

Usage:
    python3 make_400_stateprobe.py p4STATE.plain
    python3 taftool.py pack p4STATE.plain <donor.taf> p4STATE.taf

Drive it with, from ~/adrift-battle/runner/wine:
    LOAD_SLEEP=22 sh measure.sh p4STATE.taf cmdfile_state.txt
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("State case probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("State Case Probe 400")
s("SCARE probe")
s("I don't understand.")
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

# ROOMS -- one, no exits.
s(1)
s("Alpha"); s("The only room.")
for _ in range(8):
    s(0)
s(0)                     # Alts
s(0)                     # HideOnMap

# OBJECTS -- six dynamic, non-openable, stateful trinkets, all in Alpha and
# all with BStateListed on so the examine lister prints the state.
def obj(short, states):
    s("a")               # Prefix
    s(short)             # Short
    s(0)                 # V$Alias count
    s(0)                 # Static
    s("A test object.")  # Description
    s(4)                 # InitialPosition: 4 + room -> Alpha
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(0); s(0); s(0)     # Container, Surface, Capacity
    s(0); s(0); s(0)     # Wearable, SizeWeight, Parent
    s(0)                 # Openable -- 0, so no Key and so state = Var2 + 1
    s(0); s(0); s(0); s(0)   # SitLie, Edible, Readable, Weapon
    s(1)                 # CurrentState: state 1, the first of the pipe list
    s(states)            # States
    s(1)                 # StateListed
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

OBJECTS = [
    ("lever",   "In the UP position|In the DOWN Position"),
    ("compass", "Facing South|Facing North"),
    ("hatch",   "Locked Off|Open Wide"),
    ("panel",   "R1|R2"),
    ("sign",    "Sur la gauche|A droite"),
    ("switch",  "switched off|switched on"),
]

s(len(OBJECTS))
for short, states in OBJECTS:
    obj(short, states)

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

# Action type 2 = change object status: Var1 = the index among STATEFUL
# objects (the lever is 0), Var2 = the state, which for a non-openable object
# task_run_change_object_status() turns into state Var2 + 1.
FLIP_LEVER = (2, 0, 1)

s(2 + 2 * len(OBJECTS))
task("ob %object%", "OB=[%obstate%]")
task("flip", "Flipped.", actions=[FLIP_LEVER])
for short, _ in OBJECTS:
    task("st %s" % short, "ST=[%%state_%s%%]" % short)
# A second reading of each, this time with the state name embedded in a
# sentence, in case run400 lower-cases only when the name opens the line.
for short, _ in OBJECTS:
    task("mid %s" % short, "MID: the %s reads %%state_%s%% today." % (short, short))

# EVENTS, NPCS
s(0)
s(0)

s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES
s(0)

s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4STATE.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
