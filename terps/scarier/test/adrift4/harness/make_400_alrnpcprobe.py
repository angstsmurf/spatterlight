#!/usr/bin/env python3
"""ADRIFT 4.0 probe: are ALRs applied before or after characters()?

The open question left by "Ported 2026-09-08: `x <character who is elsewhere>`
names the character" (notes/WINE-TRANSCRIPTS-TODO.md, at the foot).  run400's
rewrite -- "<person> cannot see <Name> from here." -- fires out of characters()'
examine branch only if the message the turn has produced so far IS the examine
tail, and 4.0 tests that tail by EQUALITY (4801AD-48021F):

    ... Or msg = "<You> see no such thing."   And   var_140(26) = 1

`thepkgirl` ALRs `You see no such thing.` into `You see no such thing, or else
it is unimportant.`, so on that game the two readings differ:

  * ALRs applied to the pending message BEFORE characters() runs -- the
    equality misses and the ALR'd tail stands;
  * ALRs applied at print time, AFTER characters() -- the rewrite wins and the
    player is told the character's name.

Scarier assumes the latter (pf_replace_alrs() runs in the output filter), but
nothing measured it: run400's ALR array (MemVar_49411C, count MemVar_494120)
is read only by the loader in the decompile, so the application site was never
found, and the one corpus row that is this rule (`thepkgirl` t~3067, `x katryn`)
had diverged long before in the Runner's own transcript -- Katryn is never seen
in that run, so its guard fails on the seen byte and it says nothing.

This is the probe that settles it.  Three rooms; the player never reaches the
third:

    Alpha   "The first room."    east  -> Bravo      <- start, and Dave's room
    Bravo   "The second room."   west  -> Alpha
    Gamma   "The third room."    no exits            <- Erin's room, unreachable

    Dave    StartRoom Alpha.  The player shares a room with him on turn 1, so
            4.0's seen byte (var_140(26), run390 4591CC) is set.
    Erin    StartRoom Gamma.  Alive, never met, so the seen byte is clear --
            the control for the OTHER half of the guard.

and two ALRs, both of which the rewrite has to survive to be seen at all:

    "You see no such thing."  ->  "You see no such thing, or else it is
                                   unimportant."       thepkgirl's own ALR
    "cannot see"              ->  "cannot spot"        does the REWRITE's own
                                                       text get ALR'd?

The commands and what each one asks:

     1  probe        control task, "PROBE OK.", proves the file is wired
     2  x dave       Dave present: his Descr.  Sets/confirms the seen byte.
     3  x zzzz       unknown noun in Alpha.  Must come back ALR'd, or the ALR
                     list is not wired and nothing below means anything.
     4  x erin       named character, alive, NEVER SEEN.  The guard fails on
                     the seen byte whichever reading is right, so this must be
                     the ALR'd tail -- it proves the ALR reaches the message
                     on the character-examine path specifically.
     5  e            to Bravo.
     6  x dave       THE CELL.  Dave is seen and elsewhere.
                       "You cannot spot Dave from here."   -> ALRs at PRINT
                          time, after characters(): Scarier's assumption is
                          right and the port needs nothing.
                       "You cannot see Dave from here."    -> the rewrite wins
                          but the ALR pass never touches it (would contradict
                          cell 3; would mean two ALR sites).
                       "You see no such thing, or else it is unimportant."
                          -> ALRs BEFORE characters(): the equality misses, and
                          lib_cmd_examine_other() needs an "an ALR would touch
                          this message" guard.
     7  x erin       the unseen control again, from the other room.
     8  x zzzz       the ALR control again, from the other room.
     9  look         closes the transcript on a known line.

Usage:
    python3 make_400_alrnpcprobe.py p4ALRNPC.plain
    python3 taftool.py pack p4ALRNPC.plain p4TAKE.taf p4ALRNPC.taf
Session:
    sh measure.sh p4ALRNPC.taf cmdfile_p4alrnpc.txt run400.exe
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("A synthetic 4.0 ALR-vs-characters() probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Probe 4ALRNPC")
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

# NPCS -- no walks at all; each one sits where it starts.
def npc(name, descr, startroom, inroom):
    s(name)              # Name
    s("")                # Prefix
    s(0)                 # V$Alias count
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

s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES -- none.
s(0)

# ALRS
ALRS = [("You see no such thing.",
         "You see no such thing, or else it is unimportant."),
        ("cannot see", "cannot spot")]
s(len(ALRS))
for orig, repl in ALRS:
    s(orig)              # Original
    s(repl)              # Replacement

s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4ALRNPC.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
