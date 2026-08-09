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
    pWKI             like D (Times=1 looping two-stop walk) but with
                     ShowEnterExit OFF, so the Runner uses its own random
                     arrival/departure messages instead of authored ones.
                     run390 prints no authored ExitText on a leave turn (seen
                     in wkE_390 and again in pWKH39); this asks whether it
                     announces departures AT ALL, or only skips the authored
                     form -- 3.9 corpus games like "Melbourne Beach" do show
                     "David leaves to the north.", which is the default form.
    pWKJ             like D, but the two rooms are joined north/south.  The
                     leave announcement names a direction only when a room
                     exit leads to where the NPC went, and in the exit-less
                     probes run390 printed no leave line at all -- so this
                     separates "run390 never prints authored ExitText" from
                     "run390 skips a leave it cannot give a direction for".
    pWKH             multi-turn stay: Times = 3 in the player's room, 2 away,
                     no wildcard.  The 4.0 twin of this (make_400_walkprobe.py
                     H) showed run400 running the CharTask on the arrival turn
                     only; this asks the same of run390, whose walk dispatch
                     is otherwise a different code path.
    pWKL             like H (fixed stops, Times = 3 / 2) but the rooms are
                     joined north/south (as in J): does the PLAYER moving
                     into the room of a mid-stay walker fire the CharTask?
                     run400 fires it (probe L, live 2026-08-02), at every
                     stop of the walk.  Session: z n s z z z n s z.
    pWKK             FOLLOW-PLAYER stop: like H (Times = 3 / 2) but stop 0 is
                     "follow player" (Rooms value 1) and the rooms are joined
                     north/south (as in J) so the player can move mid-stay.
                     Distinguishes: CharTask every co-located tick (Scarier
                     today), counter-arrival only (the fixed-room rule), or
                     on genuine movement (a follow catch-up also fires).
                     Session: z z z z z z n s z.

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

# ROOMS -- two rooms.  Normally with no exits (the NPC walk teleports by stop
# list); variant J wires north/south between them, because npc_announce names a
# direction only when a room exit leads to where the NPC came from / went to.
def room(short, exits=()):
    s(short)             # Short
    s("LONG.")           # Long
    s("")                # LastDesc
    # Exits (4-point compass -> 8 slots: N E S W up down in out).  A slot is a
    # single 0 for "no exit", else Dest (1-based room) Var1 Var2 -- ZVar3 is
    # not stored in 3.9 files.
    for slot in range(8):
        dest = dict(exits).get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0)
    s("")                # AddDesc1
    s(0)                 # Task1
    s("")                # AddDesc2
    s(0)                 # Task2
    s(0)                 # Obj
    s("")                # AltDesc
    s(0)                 # TypeHideObjects
    s(0)                 # HideOnMap

s(2)
if variant in ("J", "K", "L"):
    room("Probe Room", {0: 2})   # north -> Far Room
    room("Far Room", {2: 1})     # south -> Probe Room
else:
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
else:                            # C/D/H/I/J/K/L: no wildcard -- walk wiring only
    s(1)
    task("#met", "CHARTASK FIRED.")
    chartask = 1

# D/E/F: the walk is visible (enter/exit + in-room texts) and loops between
# the two rooms.  A 1-stop non-looping walk (A/B/C) never runs in the real
# 3.9 Runner, so the dispatch variants use the looped shape.
looped = variant in ("D", "E", "F", "G", "H", "I", "J", "K", "L")

# I: the walk is looping and visible, but ShowEnterExit is off.
show_enterexit = variant != "I"

# How many turns the walker stays at each stop -- only H/K stay longer than one.
times = (3, 2) if variant in ("H", "K", "L") else (1, 1)

# K: stop 0 is "follow player" (walk Rooms value 1) instead of a fixed room.
first_stop = 1 if variant == "K" else 2

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
s(first_stop)            # Rooms[0]: 0=hidden 1=follow n+2=room n -> room 0
s(times[0])              # Times[0]
if looped:
    s(3)                 # Rooms[1]: room 1 (Far Room)
    s(times[1])          # Times[1]
if looped and show_enterexit:
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
