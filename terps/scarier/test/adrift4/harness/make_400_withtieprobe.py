#!/usr/bin/env python3
"""ADRIFT 4.0 probe: an ambiguity INSIDE a " with " half.

The last unmeasured corner of the second-noun lead in
notes/WINE-TRANSCRIPTS-TODO.md.  p4LOCK could not raise one -- its stones
are Shorts "red stone" and "blue stone", and 463640 scores the whole Short,
so "stone" resolves to nothing rather than to a tie -- so this probe gives
two objects the SAME Short and tells them apart by Prefix, the way p4CO's
trees do, and a second pair that share only an ALIAS:

    dyn 0  "a knife"                      held
    dyn 1  "a coin"                       held, and the box's Key
    dyn 2  "a red stone"   Short "stone"  Alpha \\ the Short tie
    dyn 3  "a blue stone"  Short "stone"  Alpha /
    dyn 4  "a ruby"        alias "gems"   Alpha \\ the alias tie
    dyn 5  "an emerald"    alias "gems"   Alpha /
    static "the rope"                     Alpha
    static "the box"       container, LOCKED, Key = the coin

sswhore's `unlock drawer with key` is the corpus line this answers: Scarier
still invents its wording there, and an invented string is a deviation by
itself.

The feed (`cmdfile_wtie.txt`, run400) separates every cell with a neutral
`look`, so a prompt's pending question is always spent by something that
does its own work:

     cut stone with knife    a tie in the FIRST half, unhandled verb
     cut rope with stone     a tie in the SECOND half, unhandled verb
     unlock box with stone   the sswhore cell, box still locked
     open box with stone     openclose resolves the whole line
     lock box with stone     the box is locked already
     unlock box with gems    the same, tied by ALIAS only
     unlock box with coin    control: the real key, unlocks
     unlock box with stone   now unlocked: refusal or ambiguity first?
     open box with stone     the same for open
     x stone                 control: the full prompt
     take stone              control: takes()' own tie wording

A length-1 self-restarting event prints TICK. on every counted turn, so an
administrative line can be told from a turn.

The ten feeds driven, all run400, all 2026-09-20
(~/adrift-battle/runner/wine/cmdfile_wtie*.txt ->
pfx/drive_c/adrift/Adrift_wtie*.txt):

    wtie    the list above: the first with-halves, locked box
    wtie2   lock/unlock cells -- MIS-DESIGNED, its first cell unlocks the
            box and every later one is "The box is not locked!"; feed 3
            replaces it
    wtie3   the same matrix with `lock box with coin` after every cell, so
            each `unlock box with X` runs on a locked box: stone/gems/zzz
            all unlock (the keyless branch), knife/rope/ruby/box refuse
    wtie4   the keyless branch and the pick-up: the coin dropped before
            `unlock box with stone`, `... with zzz` and bare `unlock box`
    wtie5   open and close with a with-half
    wtie6   `x stone knife` and `cut stone knife` against their " with "
            twins, and `take stone with knife`
    wtie7   the single-noun controls (`cut/push/open/chop stone`) and
            `chop stone knife`
    wtie8   the repeats: a with-line twice, an answer rebuilding the line,
            and `chop zzz with stone`
    wtie9   the examine matrix, head and tail, knife/rope/box/zzz
    wtie10  the pick-up proved with `i` on both sides of it

Usage:
    python3 make_400_withtieprobe.py p4WTIE.plain
    python3 taftool.py pack p4WTIE.plain p4TAKE.taf p4WTIE.taf
Drive it with, from ~/adrift-battle/runner/wine:
    TRANSCRIPT=Adrift_wtie.txt sh fast.sh p4WTIE.taf cmdfile_wtie.txt run400
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("With-tie probe.")
s(0)                     # StartRoom
ml("You have won.")

# GLOBAL
s("With Tie Probe 400")
s("SCARE probe")
s("NO IDEA.")            # DontUnderstand -- unmistakable
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
s(0); s(0)               # Sound, Graphics
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS
def room(short, long_, exits):
    s(short); s(long_)
    for slot in range(8):
        dest = exits.get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0); s(0)
    s(0); s(0)           # Alts, HideOnMap

s(2)
room("Alpha", "The first room.", {0: 2})
room("Beta", "The second room.", {2: 1})

# OBJECTS -- the p4LOCK writer, plus aliases.
def obj(short, static=False, prefix="a", position=4, room=1, aliases=(),
        container=0, openable=0, key=-1, capacity=22):
    s(prefix); s(short)
    s(len(aliases))
    for a in aliases:
        s(a)
    s(1 if static else 0)
    s("A plain thing.")
    if static:
        s(0)
    else:
        s(position)      # InitialPosition: 1 held, 4 = room 0, 5 = room 1
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    if static:
        s(1); s(room)    # Where: one room, 1-based
    s(container); s(0); s(capacity if container else 0)
    if not static:
        s(0); s(0); s(0) # Wearable, SizeWeight, Parent
    s(openable)          # Openable: 0 none, 5 open, 6 closed, 7 locked
    if openable:
        s(key)           # Key: dynamic-object index, -1 none
    s(0)                 # SitLie
    if not static:
        s(0)             # Edible
    s(0)                 # Readable
    if not static:
        s(0)             # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(8)
obj("rope", static=True, prefix="the")
obj("knife", position=1)                                   # dyn 0
obj("coin", position=1)                                    # dyn 1
obj("box", static=True, prefix="the", container=1, openable=7, key=1)
obj("stone", position=4, prefix="a red")                   # dyn 2
obj("stone", position=4, prefix="a blue")                  # dyn 3
obj("ruby", position=4, aliases=("gems",))                 # dyn 4
obj("emerald", position=4, prefix="an", aliases=("gems",)) # dyn 5

# TASKS -- one, so that the probe can prove a task is not what answers.
def task(cmd, text):
    s(1); s(cmd)
    s(text)
    s(""); s(""); s("")  # Reverse/Repeat/Additional
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand
    s(3)                 # Where: anywhere
    s("")                # Question
    s(0)                 # Restrictions
    s(0)                 # Actions
    s("")                # RestrMask

s(1)
task("probe", "PROBE OK.")

# EVENTS -- length 1, restarts immediately.
s(1)
s("Ticker")
s(1)                     # StarterType: immediate
s(1)                     # RestartType: restart immediately
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

# NPCS -- none.
s(0)

s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WTIE.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
