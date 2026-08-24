#!/usr/bin/env python3
"""ADRIFT 4.0 probe: is a walk announcement joined to the turn's text the way
the ALR list can see across the join?

sophie.taf (4.00) stays silent about Grumble's arrival on eight of its first
fifty turns.  make_400_walkannounceprobe.py exonerated every structural
difference between the silent tasks and the noisy ones -- ShowRoomDesc,
AdditionalMessage, action order, an executed task that is room-restricted,
non-repeatable and prints a paragraph of its own -- because the probe announced
in all twenty cells.  Bisecting sa.taf itself in Wine found the real cause:
sophie's own ALR list, 488 entries hand-written by the author, deletes the
arrival sentences at named spots, e.g.

    'quiet.  Grumble complaining of beer deprivation staggers in from the west.'
        -> 'quiet.'

Deleting all 488 ALRs made the silent turn announce.  For that Original to
match, run400 must have joined the announcement onto the preceding text with
pspace's TWO SPACES, in the same buffer the ALR walk later runs over.  Scarier
instead emits a newline before the sentence and a newline after it, so no such
ALR can ever fire and every one of the eight lines survives.

The cells, one NPC on sophie's walk (one stop, Rooms = 1 "follow the player",
Times = 1, ShowEnterExit on) and four ALRs:

    e       the library's own go handler.  Bravo prints "The second room." and
            the exits line, and the ALR
                'You can only move west.  Bob wanders in from the west.'
            -> 'ALRKILL.'  fires only if the announcement is joined to the
            exits line with two spaces inside one filtered buffer.
    j16     a task whose CompleteText is the bare "J16." with no <br> and no
            ShowRoomDesc, so the announcement joins straight onto task text:
                'J16.  Bob wanders in from the west.' -> 'ALRJOIN.'
    (both)  a single-space twin of the first Original, 'ONESPACE.', which must
            never fire -- it pins the separator to two spaces rather than "some
            whitespace".
    w       going back re-arms the next cell, and 'Bob wanders off to the
            east.' -> 'DEPARTKILL.' asks whether the departure half of the
            walk tick is in the same buffer.

Usage:
    python3 make_400_walkalrprobe.py p4WALKALR.plain
    python3 taftool.py pack p4WALKALR.plain <donor.taf> p4WALKALR.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Walk ALR probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Walk ALR Probe 400")
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
s(0); s(0)               # Sound, Graphics
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

s(0)                     # OBJECTS -- none

# TASKS
def task(cmd, text, srd=0, actions=None):
    s(1); s(cmd)
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(srd)               # ShowRoomDesc
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

# Action type 1 = move: Var1 = 0 ("move player"), Var2 = 0 ("to room"),
# Var3 = the 0-based room.
TO_BRAVO = (1, 0, 0, 1)

s(1)
task("j16", "J16.", srd=0, actions=[TO_BRAVO])

s(0)                     # EVENTS -- none

# NPCS -- one, on sophie's walk.
s(1)
s("Bob")                 # Name
s("")                    # Prefix
s(0)                     # V$Alias count
s("A patient walker.")   # Descr
s(1)                     # StartRoom (1-based): Alpha
s("")                    # AltText
s(0)                     # Task
s(0)                     # Topics
s(1)                     # Walks
s(1)                     # NumStops
s(1)                     # Loop
s(0)                     # StartTask
s(0)                     # CharTask
s(0)                     # MeetObject
s(0)                     # ObjectTask
s(0)                     # StoppingTask
s(0)                     # MeetChar
s("")                    # ChangedDesc
s(1); s(1)               # Rooms[0] = 1 (follow the player), Times[0] = 1
s(1)                     # ShowEnterExit
s("wanders in")          # EnterText
s("wanders off")         # ExitText
s("Bob is here.")        # InRoomText
s(0)                     # Gender

s(0); s(0)               # RoomGroups, Synonyms

s(0)                     # VARIABLES

ALRS = [
    ("You can only move west.  Bob wanders in from the west.", "ALRKILL."),
    ("J16.  Bob wanders in from the west.", "ALRJOIN."),
    ("You can only move west. Bob wanders in from the west.", "ONESPACE."),
    ("Bob wanders off to the east.", "DEPARTKILL."),
]
s(len(ALRS))
for orig, repl in ALRS:
    s(orig); s(repl)

s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WALKALR.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
