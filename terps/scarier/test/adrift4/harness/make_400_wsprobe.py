#!/usr/bin/env python3
"""ADRIFT 4.0 probe: is the whitespace between two adjacent [] / {} groups in a
task command pattern required, forbidden, or optional?

RUNNER_TESTS_TODO.md §7.  Scarier used to demand a space at every group
boundary (NODE_WHITESPACE), which made ImagiDroids' `{go/walk/move}[n/escape/
out]{orth/out}` exits and The Forum's `{the}{wooden}[clog]{s}` unmatchable; it
now interposes a NODE_JOIN that eats a space if it is there and never fails.
That was decided on internal evidence only.  The cell nobody has measured is
whether *explicit* pattern whitespace is equally tolerant -- if it is,
uip_match_whitespace() should collapse into uip_match_join().

One room, no objects, four tasks:

    [al]{pha}     -> "JOIN FIRED."      adjacent choice + optional, no space
    [be] {ta}     -> "SPACE FIRED."     the same shape with an explicit space
    [ga][mma]     -> "CHOICE FIRED."    two adjacent choices
    ping          -> "PING FIRED."      control, proves the probe is wired

Session (all typed in room 1):

    ping / alpha / al pha / al / beta / be ta / be / gamma / ga mma

Read-out:

    alpha  fires  -> adjacency does not require a space   (Scarier: fires)
    al pha fires  -> adjacency allows a space             (Scarier: fires)
    al     fires  -> the optional group may be omitted    (Scarier: fires)
    beta   fires  -> explicit whitespace is optional too  (Scarier: does NOT)
    be ta  fires  -> explicit whitespace works            (Scarier: fires)
    be     fires  -> trailing optional after a space      (Scarier: fires)
    gamma  fires  -> choice/choice adjacency, no space    (Scarier: fires)
    ga mma fires  -> choice/choice adjacency, with space  (Scarier: fires)

Output is the PLAIN body only; produce a Runner-valid .taf with:

    python3 make_400_wsprobe.py p4WS.plain
    python3 taftool.py pack p4WS.plain <any valid 4.0 donor.taf> p4WS.taf

(taftool.py sits next to this script; the donor supplies the "Wild" password
trailer run400 validates.)
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Whitespace probe.")
s(0)                     # StartRoom (0-based)
ml("You have won.")

# GLOBAL
s("Whitespace Probe 400")
s("SCARE probe")
s("I don't understand.")
s(2)                     # Perspective
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player")
s(0)                     # PromptName
s("A test subject.")
s(0); s(0); s(0); s(0)   # Task, Position, ParentObject, PlayerGender
s(100); s(100)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
s(0)                     # StatusBox
s("")                    # StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS -- one.
s(1)
s("Probe Room")
s("LONG.")
for _ in range(8):
    s(0)
s(0)                     # Alts count
s(0)                     # HideOnMap

# OBJECTS
s(0)

# TASKS
def task(cmd, text):
    s(1); s(cmd)         # V$Command
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

s(4)
task("[al]{pha}", "JOIN FIRED.")
task("[be] {ta}", "SPACE FIRED.")
task("[ga][mma]", "CHOICE FIRED.")
task("ping",      "PING FIRED.")

# EVENTS, NPCS
s(0)
s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WS.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
