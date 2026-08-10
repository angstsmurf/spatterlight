#!/usr/bin/env python3
"""ADRIFT 3.9 probe: what does run390 answer for a task command typed OUTSIDE
the task's Where rooms?

This is the 3.9 twin of make_400_whereprobe.py.  run400 answers the game's own
DontUnderstand text for BOTH a Where-type-0 task and an out-of-room type-1 task
(measured live, RUNNER_TESTS_TODO.md section 5); run390 was seen to answer
"You can't do that here!" on *The Hangover*'s two type-0 tasks, so the two
Runners look like they differ.  Same three tasks, same session, one version
down, settles it.

Two rooms joined north/south, no objects/NPCs, eight tasks (the first six typed
in room 1):

    alpha   Where type 0 (No rooms)     -> "ALPHA FIRED."
    beta    Where type 3 (All rooms)    -> "BETA FIRED."   (control: wiring)
    gamma   Where type 1 (room 2 only)  -> "GAMMA FIRED."  (out of room)
    delta   Where type 2 (some rooms:   -> "DELTA FIRED."  (out of room, and
            room 2 only)                                    a multi-word command)
    epsilon Where type 3, NOT repeatable, empty RepeatText -- type it twice: is
            the second, refused-because-done attempt also "can't do that here"?
    zeta    Where type 3, one restriction that FAILS with an empty FailMessage:
            is the refusal about the room only, or about any refused task?

The last two say how wide the flag is; the first four say whether the message
exists at all.

Two more tasks probe the *other* pre-4.0 refusal that turned up while measuring
this one -- "You have already done that." for a completed non-repeatable task
typed again (` have already done that.` is a UTF-16 literal in run370, run380
and run390, and absent from run400, the same version pattern):

    eta     Where type 3, NOT repeatable, RepeatText "ETA REPEAT." -- does an
            authored RepeatText displace the refusal?
    theta   Where type 1 (room 1 only), NOT repeatable -- complete it in room 1,
            walk north, and type it again: with the task both done AND out of
            its room, which of the two refusals wins?

The rooms are joined north/south for theta's sake; ShowExits is off, so no
transcript line moves because of it.

Perspective is settable because the refusal is built as `arr(0) & " can't do
that here!"` in the P-code, and `arr(0)` looked like the perspective pronoun.
It is: run390 answers "I can't do that here!" for Perspective 0 and "You can't
do that here!" for 1, 2 and 3 -- pre-4.0 has only two perspectives (its
inventory says "You are carrying nothing." for 2 as well, where SCARE, reading
2 as third person, said "Player is carrying nothing.").

That third finding is now a regression of its own: the Makefile builds this
probe three times, with Perspective 1, 0 and 2, and the script types `i` so the
inventory line records which person the library renders.  The Perspective-2
build must read exactly like the Perspective-1 build -- 2 is third person in
4.0 only, and lib_get_perspective() clamps it below that.

Variant "e" adds a one-turn always-restarting event printing "TICK." so the
transcript shows whether the refusal consumes a turn: the P-code sets
`handled = 1` alongside the message, which would make it tick where a plain
DontUnderstand does not.

    python3 make_39_whereprobe.py p39where.taf [perspective] [variant]
"""
import sys

VARIANT = sys.argv[3] if len(sys.argv) > 3 else ""

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText
s("A 3.9 Where probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Where Probe 390")    # GameName
s("SCARE probe")        # GameAuthor
s("I don't understand.")# DontUnderstand
s(int(sys.argv[2]) if len(sys.argv) > 2 else 2)   # Perspective
s(0)                    # ShowExits
s(0)                    # WaitTurns
s(1)                    # DispFirstRoom
s(0)                    # BattleSystem
s(0)                    # MaxScore
s("Player")             # PlayerName
s(0)                    # PromptName
s("A test subject.")    # PlayerDesc
s(0)                    # Task (0 -> no AltDesc)
s(0)                    # Position
s(0)                    # ParentObject
s(0)                    # PlayerGender
s(100)                  # MaxSize
s(100)                  # MaxWt
s(0)                    # EightPointCompass
s(0)                    # bNoDebug
s(0)                    # NoScoreNotify
s(0)                    # NoMap
s(0)                    # bNoAutoComplete
s(0)                    # bNoControlPanel
s(0)                    # bNoMouse
s(0)                    # Sound
s(0)                    # Graphics
s(0)                    # iUnk1 (SizeMultiple)
s(0)                    # iUnk2 (WeightMultiple)

# ROOMS -- two, joined north/south so theta can be completed in one room and
# retyped in the other.
def room(short, exits=()):
    s(short)            # Short
    s("LONG.")          # Long
    s("")               # LastDesc
    # Exits (N E S W up down in out).  A slot is a single 0 for "no exit", else
    # Dest (1-based room) Var1 Var2 -- ZVar3 is not stored in 3.9 files.
    for slot in range(8):
        dest = dict(exits).get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0)
    s("")               # AddDesc1
    s(0)                # Task1
    s("")               # AddDesc2
    s(0)                # Task2
    s(0)                # Obj
    s("")               # AltDesc
    s(0)                # TypeHideObjects
    s(0)                # HideOnMap

s(2)
room("Probe Room", {0: 2})   # north -> Far Room
room("Far Room", {2: 1})     # south -> Probe Room

# OBJECTS
s(0)

# TASKS
def task(cmd, text, where_type, rooms=None, repeatable=1, restr=None,
         repeattext=""):
    s(0); s(cmd)        # W$Command: count 0 -> 1 command
    s(text)             # CompleteText
    s("")               # ReverseMessage
    s(repeattext)       # RepeatText
    s("")               # AdditionalMessage
    s(0)                # ShowRoomDesc
    s(repeatable)       # Repeatable
    s(0)                # Reversible
    s(0); s("")         # W$ReverseCommand: count 0 -> 1 string
    s(where_type)       # Where: ROOM_LIST0 Type
    if where_type == 1:
        s(rooms[0])     # ... single room (0-based)
    elif where_type == 2:
        for r in range(2):
            s(1 if r in rooms else 0)
    s("")               # Question
    if restr is None:
        s(0)            # Restrictions
    else:
        s(1)
        s(restr[0])                     # Type
        for v in restr[1]: s(v)         # Vars
        s(restr[2])                     # FailMessage
    s(0)                # Actions
    # RestrMask is a parse fixup in 3.9; Res: nothing

s(8)
task("alpha",      "ALPHA FIRED.",   0)
task("beta",       "BETA FIRED.",    3)
task("gamma",      "GAMMA FIRED.",   1, [1])
task("delta echo", "DELTA FIRED.",   2, [1])
task("epsilon",    "EPSILON FIRED.", 3, repeatable=0)
# Task-state restriction "all tasks not done" -- fails from turn 1, silently.
task("zeta",       "ZETA FIRED.",    3, restr=(2, (0, 1), ""))
task("eta",        "ETA FIRED.",     3, repeatable=0, repeattext="ETA REPEAT.")
task("theta",      "THETA FIRED.",   1, [0], repeatable=0)

# EVENTS -- variant "e" only: a one-turn event that restarts immediately, so
# every turn that really is a turn prints "TICK.".
if VARIANT == "e":
    s(1)
    s("Ticker")
    s(1)                 # StarterType: immediate
    s(1)                 # RestartType: immediately
    s(0)                 # TaskFinished
    s(1); s(1)           # Time1 Time2
    s(""); s(""); s("TICK.")
    s(3)                 # Where: all rooms
    s(0); s(0)           # PauseTask PauserCompleted
    s(0); s("")          # PrefTime1 PrefText1
    s(0); s(0)           # ResumeTask ResumerCompleted
    s(0); s("")          # PrefTime2 PrefText2
    s(0); s(0); s(0); s(0); s(0); s(0)  # Obj2/Dest Obj3/Dest Obj1/Dest
    s(0)                 # TaskAffected
else:
    s(0)

# NPCS
s(0)

# tail
s(0)                    # RoomGroups
s(0)                    # Synonyms
s(0)                    # Variables
s(0)                    # ALRs
s(0)                    # CustomFont
s("2026")               # CompileDate
s("    Wild    ")       # sPassword

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p39where.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
