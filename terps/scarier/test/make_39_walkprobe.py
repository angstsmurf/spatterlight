#!/usr/bin/env python3
"""ADRIFT 3.9 probe: is NPC-walk CharTask dispatch wildcard-interceptable?

Companion to make_39_wildprobe.py, which settled the *event* TaskAffected
dispatch (run390 submits the task's command text through the normal task
matcher, so an earlier runnable `*` task steals the execution).  The open
question from RUNNER_TESTS_TODO.md section 2 is whether a walk's CharTask /
ObjectTask dispatch takes the same path.  Statically it should: the P-code at
0005AAD5 (CharTask) and 0005AB88 (ObjectTask) in Form1.characters is
instruction-for-instruction the checkevent dispatch at 00048D83 -- copy
tasks[n-1].command[0] into the input global, call Form1.tasks(1).

Two rooms; the player and a rock start in room 1, NPC Bob starts in room 2
with a single StartTask=0 walk to room 1 (Times=1), CharTask = the "#met"
task, meeting the player on arrival.

    pWKA (default)   task 1 = `*` "WILDCARD FIRED." (repeatable, all rooms,
                     unrestricted), task 2 = `#met` "CHARTASK FIRED.",
                     walk CharTask=2.  If walk dispatch goes through the
                     matcher, the wildcard steals: the arrival turn prints
                     WILDCARD FIRED twice and never CHARTASK FIRED.  If the
                     dispatch is direct, CHARTASK FIRED appears.
    pWKB (control)   same but task order swapped (CharTask=1): #met is first
                     in list order, so it fires under either model -- proves
                     the walk itself is wired correctly.

Session for both: z / z / z (the wildcard eats each typed command; the walk
arrives on its own schedule).

Usage: python3 make_39_walkprobe.py [A|B] [out.taf]
"""
import sys

variant = (sys.argv[1] if len(sys.argv) > 1 else "A").upper()

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("Walk probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Walk Probe 39")       # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test subject.")     # PlayerDesc
s(0)                     # Task (0 -> no AltDesc)
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(100)                   # MaxSize
s(100)                   # MaxWt
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
s(0)                     # iUnk1
s(0)                     # iUnk2

# ROOMS -- two rooms, no exits (the NPC walk teleports by stop list).
def room(short):
    s(short)             # Short
    s("LONG.")           # Long
    s("")                # LastDesc
    for _ in range(8): s(0)  # exits (4-point compass -> 8 slots)
    s("")                # AddDesc1
    s(0)                 # Task1
    s("")                # AddDesc2
    s(0)                 # Task2
    s(0)                 # Obj
    s("")                # AltDesc
    s(0)                 # TypeHideObjects
    s(0)                 # HideOnMap

s(2)
room("Probe Room")
room("Far Room")

# OBJECTS -- none.
s(0)

# TASKS -- wildcard and the walk-met task, order per variant.
def task(cmd, text, restr=None):
    s(0); s(cmd)         # W$Command: count, then count+1 alternatives
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: ROOM_LIST0 Type 3 = all rooms
    s("")                # Question (no hints follow)
    if restr is None:
        s(0)             # Restrictions
    else:
        gate, failmsg = restr
        s(1)             # Restrictions
        s(2)             # type 2: task state
        s(gate - 1)      # Var1: 1-based task, stored -1 (V390_TASK_RESTR ++)
        s(0)             # Var2: 0 = task must be done (gate never is -> FAIL)
        s(failmsg)       # FailMessage
    s(0)                 # Actions

if variant in ("A", "E"):
    s(2)
    task("*", "WILDCARD FIRED.")
    task("#met", "CHARTASK FIRED.")
    chartask = 2
elif variant in ("B", "F"):
    s(2)
    task("#met", "CHARTASK FIRED.")
    task("*", "WILDCARD FIRED.")
    chartask = 1
elif variant == "G":             # G: restricted #met -- silent skip or loud?
    s(2)
    task("#met", "CHARTASK FIRED.", restr=(2, "METFAIL."))
    task("xyzzygate", "GATE DONE.")   # never typed, never completed
    chartask = 1
else:                            # C/D: no wildcard at all -- walk wiring only
    s(1)
    task("#met", "CHARTASK FIRED.")
    chartask = 1

# D/E/F: the walk is visible (enter/exit + in-room texts) and loops between
# the two rooms.  A 1-stop non-looping walk (A/B/C) never runs in the real
# 3.9 Runner, so the dispatch variants use the looped shape.
looped = variant in ("D", "E", "F", "G")

# EVENTS
s(0)

# NPCS -- Bob, starting in room 2 (1-based), one walk to room 1.
s(1)
s("Bob")                 # Name
s("a")                   # Prefix
s("")                    # [1]$Alias
s("A test NPC.")         # Descr
s(2)                     # StartRoom (1-based; room index 1)
s("")                    # AltText
s(0)                     # Task
s(0)                     # Topics
s(1)                     # Walks
s(2 if looped else 1)    # NumStops
s(1 if looped else 0)    # Loop
s(0)                     # StartTask (0 = start at game start)
s(chartask)              # CharTask (1-based task index of #met)
s(0)                     # MeetObject
s(0)                     # ObjectTask
s(0)                     # StoppingTask
s("")                    # ChangedDesc
s(2)                     # Rooms[0]: 0=hidden 1=follow n+2=room n -> room 0
s(1)                     # Times[0]
if looped:
    s(3)                 # Rooms[1]: room 1 (Far Room)
    s(1)                 # Times[1]
if looped:
    s(1)                 # ShowEnterExit
    s("BOB ENTERS.")     # EnterText
    s("BOB LEAVES.")     # ExitText
else:
    s(0)                 # ShowEnterExit
s("Bob is standing here." if looped else "")  # InRoomText
s(0)                     # Gender

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate
s("    Wild    ")        # sPassword: pw[0:4]+"Wild"+pw[4:8]

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[2] if len(sys.argv) > 2 else "pWK%s.taf" % variant
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
