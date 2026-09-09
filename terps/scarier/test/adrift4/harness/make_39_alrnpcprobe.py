#!/usr/bin/env python3
"""ADRIFT 3.90 twin of make_400_alrnpcprobe.py: are ALRs applied before or
after characters()?

The 4.0 probe's world, in the 3.90 layout, so the pre-4.0 arm of the same
rewrite gets measured too.  All four Runners compose "<person> cannot see
<Name> from here." out of characters()' examine branch, and all four gate it on
the message the turn has produced so far (run370 438F2F, run380 440E42,
run390 45A07C, run400 4801AD) -- but the pre-4.0 guard tests

    InStr(msg, "<You> can't see that") > 0  Or  msg = "Nothing special."

where 4.0 tests `msg = "<You> see no such thing."` and the seen byte on top.
So the ALR question is the same question on both sides, and the sentence being
matched is different: 3.9's is "Nothing special.", which is what the Runner
prints for `x <unknown noun>` (measured 2026-08-25, p39EXAM).

Two ALRs, the same shape as the 4.0 probe's:

    "Nothing special."  ->  "Nothing special, or else it is unimportant."
    "cannot see"        ->  "cannot spot"

Three rooms; the player never reaches the third:

    Test Room   east -> Bravo      <- start, and Dave's room
    Bravo       west -> Test Room
    Gamma       no exits           <- Erin's room, unreachable

    Dave  StartRoom Test Room.  Shares a room with the player on turn 1.
    Erin  StartRoom Gamma.  Alive, never met.  3.9 has NO seen gate, so unlike
          the 4.0 probe this one must ALSO be named by the rewrite -- which is
          the other half of the version split, measured from this side.

Usage:
    python3 make_39_alrnpcprobe.py p39ALRNPC.taf
Session:
    ./fast.sh p39ALRNPC.taf cmdfile_p39alrnpc.txt run390.exe
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 ALR-vs-characters() probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39ALRNPC")      # GameName
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

# NPCS -- no walks at all; each one sits where it starts.
def npc(name, descr, startroom, inroom):
    s(name)              # Name
    s("")                # Prefix
    s("")                # [1]$Alias
    s(descr)             # Descr
    s(startroom)         # StartRoom (1-based; 0 = nowhere)
    s("")                # AltText
    s(0)                 # Task
    s(0)                 # Topics
    s(0)                 # Walks
    s(1)                 # ShowEnterExit
    s("wanders in")      # EnterText
    s("wanders off")     # ExitText
    s(inroom)            # InRoomText
    s(0)                 # Gender

s(2)
npc("Dave", "A quiet man.",   1, "Dave is here.")
npc("Erin", "A quiet woman.", 3, "Erin is here.")

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables

ALRS = [("Nothing special.", "Nothing special, or else it is unimportant."),
        ("cannot see", "cannot spot")]
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

out = sys.argv[1] if len(sys.argv) > 1 else "p39ALRNPC.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
