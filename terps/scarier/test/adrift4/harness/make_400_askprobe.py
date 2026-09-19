#!/usr/bin/env python3
"""ADRIFT 4.00 twin of make_39_askprobe.py: how the character handler's ask
branch picks a topic (run400 47F8F7-47FB1E).

Same NPCs, topics and command file as the 3.90 probe:

    Dave  topics: [0] "red key"  [1] "key"  [2] "*"  [3] "coin"
                  [4] "coin" (Task 1 = probe, AltReply empty)
    Erin  topics: [0] "key"      (no "*")

Usage:
    python3 make_400_askprobe.py p4ASK.plain
    python3 taftool.py pack p4ASK.plain p4TAKE.taf p4ASK.taf
Session:
    ./fast.sh p4ASK.taf cmdfile_p39ask.txt run400x.exe
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("A synthetic 4.0 ask-topic probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Probe 4ASK")
s("SCARE probe")
s("NO IDEA.")            # DontUnderstand, unmistakable
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

# ROOMS -- exit order north, east, south, west, up, down, in, out; a real
# exit is Dest (1-based) Var1 Var2 Var3.
def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0)                 # Alts
    s(0)                 # HideOnMap

s(3)
room("Alpha", "The first room.",  {1: 2})
room("Bravo", "The second room.", {3: 1})
room("Gamma", "The third room.",  {})

# OBJECTS -- one, so the room listing has something in it and `x zzzz` is
# refused by a resolver that has a table to fail against.
s(1)
s("a")                   # Prefix
s("stone")               # Short
s(0)                     # V$Alias count
s(0)                     # Static
s("A grey stone.")       # Description
s(4)                     # InitialPosition: 4 = in room 1 (Alpha)
s(0); s(0); s("")        # Task, TaskNotDone, AltDesc
s(0); s(0); s(0)         # Container, Surface, Capacity
s(0); s(0); s(0)         # Wearable, SizeWeight, Parent
s(0)                     # Openable
s(0)                     # SitLie
s(0); s(0)               # Edible, Readable
s(0)                     # Weapon
s(0)                     # CurrentState
s(0)                     # ListFlag
s(""); s(0)              # InRoomDesc, OnlyWhenNotMoved

# TASKS -- one control.
s(1)
s(1); s("probe")
s("PROBE OK.")           # CompleteText
s("")                    # ReverseMessage
s("")                    # RepeatText
s("")                    # AdditionalMessage
s(0)                     # ShowRoomDesc
s(1)                     # Repeatable
s(0)                     # Reversible
s(0)                     # V$ReverseCommand count
s(3)                     # Where: all rooms
s("")                    # Question
s(0)                     # Restrictions
s(0)                     # Actions
s("")                    # RestrMask

# EVENTS -- none.
s(0)

# NPCS -- no walks; both share the player's room.
def npc(name, descr, startroom, inroom, topics):
    s(name)              # Name
    s("")                # Prefix
    s(0)                 # V$Alias count
    s(descr)             # Descr
    s(startroom)         # StartRoom (1-based; 0 = nowhere)
    s("")                # AltText
    s(0)                 # Task
    s(len(topics))       # Topics
    for subject, reply, task, altreply in topics:
        s(subject); s(reply); s(task); s(altreply)
    s(0)                 # Walks
    s(1)                 # ShowEnterExit
    s("wanders in")      # EnterText
    s("wanders off")     # ExitText
    s(inroom)            # InRoomText
    s(0)                 # Gender

s(2)
npc("Dave", "A quiet man.", 1, "Dave is here.", [
    ("red key", "DAVE RED KEY.", 0, ""),
    ("key",     "DAVE KEY.",     0, ""),
    ("*",       "DAVE STAR.",    0, ""),
    ("coin",    "DAVE COIN ONE.", 0, ""),
    ("coin",    "DAVE COIN TWO.", 1, ""),
])
npc("Erin", "A quiet woman.", 1, "Erin is here.", [
    ("key",     "ERIN KEY.",     0, ""),
])

s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES -- none.
s(0)

# ALRS
ALRS = []
s(len(ALRS))
for orig, repl in ALRS:
    s(orig)              # Original
    s(repl)              # Replacement

s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4ASK.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
