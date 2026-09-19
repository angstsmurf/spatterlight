#!/usr/bin/env python3
"""ADRIFT 3.90 absent-NPC probe: characters()' per-verb answers for an NPC
that is not in the player's room.

run390 characters() (45ACD8), the name block at 4592B8: take/get/pick up
"<Name> is not here!" (4596E1, buffer empty, no seen test); talk to /
speak to the format hint (4597C0, no room test); ask/talk to "<Name> isn't
here!" (459C2A); give "You cannot see <Name> from here." (45A240); the
catch-all "is not here!" / "Who?" by the seen byte (45ACA8).

    Test Room: Dave (seen), the stone
    Bravo:     Erin, Gina ("a" "girl") -- seen after east/west
    Gamma:     Fred -- never seen

Usage:
    python3 make_39_absnpcprobe.py p39ABSNPC.taf
Session:
    ./fast.sh p39ABSNPC.taf cmdfile_p39absnpc.txt run390x.exe
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 ask-topic probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39ABSNPC")      # GameName
s("SCARE probe")         # GameAuthor
s("NO IDEA.")            # DontUnderstand
s(2)                     # Perspective (second person)
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test subject.")     # PlayerDesc
s(0)                     # Task
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(102); s(102)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0); s(0); s(0)         # bNoDebug, NoScoreNotify, NoMap
s(0); s(0); s(0)         # bNoAutoComplete, bNoControlPanel, bNoMouse
s(0); s(0)               # Sound, Graphics
s(3); s(3)               # SizeMultiple, WeightMultiple

# ROOMS
def room(short, long_, exits=()):
    s(short); s(long_); s("")
    for slot in range(8):            # N E S W up down in out
        dest = dict(exits).get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0)
    s(""); s(0); s(""); s(0); s(0); s(""); s(0)
    s(0)                             # HideOnMap

s(3)
room("Test Room", "The first room.",  {1: 2})    # east -> Bravo
room("Bravo",     "The second room.", {3: 1})    # west -> Test Room
room("Gamma",     "The third room.")

# OBJECTS -- one loose object, so the room listing has something in it.
s(1)
s("a"); s("stone"); s("")            # Prefix Short [1]Alias
s(0)                                 # Static
s("A grey stone.")                   # Description
s(4)                                 # InitialPosition: 4 + room index 0
s(0); s(0); s("")                    # Task TaskNotDone AltDesc
s(0); s(0); s(0)                     # Container Surface Capacity
s(0); s(0); s(0)                     # Wearable SizeWeight Parent
s(0)                                 # Openable
s(0); s(0); s(0); s(0)               # SitLie Edible Readable Weapon

# TASKS -- one control.
s(1)
s(0)                     # W$Command: count 0 -> 1 command
s("probe")
s("PROBE OK.")           # CompleteText
s("")                    # ReverseMessage
s("")                    # RepeatText
s("")                    # AdditionalMessage
s(0)                     # ShowRoomDesc
s(1)                     # Repeatable
s(0)                     # Reversible
s(0); s("")              # W$ReverseCommand
s(3)                     # Where: all rooms
s("")                    # Question
s(0)                     # Restrictions
s(0)                     # Actions

# EVENTS
s(0)

# NPCS -- no walks.
def npc(name, prefix, alias, descr, startroom, inroom, topics):
    s(name)              # Name
    s(prefix)            # Prefix
    s(alias)             # [1]$Alias
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

s(4)
npc("Dave", "", "", "A quiet man.", 1, "Dave is here.", [("key", "DAVE KEY.", 0, "")])
npc("Erin", "", "", "A quiet woman.", 2, "Erin is here.", [("key", "ERIN KEY.", 0, "")])
npc("Fred", "", "", "A loud man.", 3, "Fred is here.", [("key", "FRED KEY.", 0, "")])
npc("Gina", "a", "girl", "A small girl.", 2, "A girl is here.", [])

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables

ALRS = []
s(len(ALRS))
for orig, repl in ALRS:
    s(orig); s(repl)

s(0)                     # CustomFont
s("2026")                # CompileDate
s("    Wild    ")        # sPassword

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p39ABSNPC.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
