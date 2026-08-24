#!/usr/bin/env python3
"""ADRIFT 4.0 probe: which of a task's restrictions refused, and whose message?

The_X-Files_A_New_Beginning.taf (4.00) has task 24, `Burn %object%`, restr
mask `#A#`:

    RESTR type=0 v1=1 v2=3 v3=0 fail=[There's nothing here to burn!]
    RESTR type=3 v1=0 v2=2 v3=-1 fail=[]

i.e. "any object is visible to the player" AND "the player is alone".  Scarier
passes both and prints the CompleteText; run400 answers

    I don't understand what you want me to do with The Memo.

which is generaltasks' end-of-turn fallback -- so run400 refused the task and
printed NOTHING.  Restriction 1's message is not empty, restriction 2's is, so
the natural reading is "restriction 2 failed and its silence fell through".
But that reading rests on an undecoded detail: run400's fail path does a
non-empty-message test before printing, and whether it takes the FIRST failing
restriction that has a message or simply the LAST failing one is not readable
from the listing.  If it is the latter, restriction 1 could have been the
failing one all along and its message was never a candidate.

So measure both halves at once.  Two rooms, one nowhere NPC, one trinket each
side, and a cell per shape:

    pa   no restrictions at all -- the baseline that the command matches
    burn no restrictions, on the verb xfiles uses -- is `burn` intercepted?
    pb   restriction 1 alone            ("any object visible to the player")
    pc   restriction 2 alone            ("the player is alone")
    pd   both, xfiles' EXACT shape: message on 1, none on 2
    pe   both, a message on each
    pf   a sure-failing 1 ("NO object is visible"), message
    pg   a sure-failing 2 ("the player is NOT alone"), message
    ph   sure-fail 1 with a message AND a passing 2 with a message
    pi   a passing 1 with a message AND sure-fail 2 with a message
    pj   sure-fail 1 with NO message, sure-fail 2 with one
    pk   sure-fail 1 with a message, sure-fail 2 with NONE
    pl   a single sure-failing restriction with no message at all

pb and pc say which of xfiles' two restrictions run400 disagrees with.  ph/pi
say which failing restriction's message the Runner picks when only one has
failed; pj/pk say whether it skips an empty message to reach a later one or
just prints nothing; pl is the pure silent refusal, and should produce the
"I don't understand what you want me to do with the coin." fallback verbatim.

The one NPC starts NOWHERE (StartRoom 0), exactly as xfiles' NPC 12 does, and
the command file runs the alone cells in the START room, then again after a
round trip east and back.  That is deliberate: the .taf stores the header's
StartRoom 0-based but every exit Dest 1-based, so if run400 fails to normalise
the header the player's room number is 0 until the first move -- which would
collide with the nowhere NPC's 0 and make "alone" false in the start room only.
xfiles burns the memo in its start room.  If pc fails before the round trip and
passes after it, that is the whole bug.

Usage:
    python3 make_400_burnprobe.py p4BURN.plain
    python3 taftool.py pack p4BURN.plain <donor.taf> p4BURN.taf
    LOAD_SLEEP=22 sh measure.sh p4BURN.taf cmdfile_burn.txt
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Burn restriction probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Burn Restriction Probe 400")
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
s(0); s(0)               # Sound, Graphics -- both off, so no resource fields
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

# OBJECTS -- one trinket each side, so "any object is visible to the player"
# has something to see in either room and every command has a real referent.
def obj(short, pos):
    s("a")               # Prefix
    s(short)             # Short
    s(0)                 # V$Alias count
    s(0)                 # Static
    s("A trinket.")      # Description
    s(pos)               # InitialPosition: 4 + the 0-based room
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(0); s(0); s(0)     # Container, Surface, Capacity
    s(0); s(0); s(0)     # Wearable, SizeWeight, Parent
    s(0); s(0); s(0); s(0); s(0)   # Openable, SitLie, Edible, Readable, Weapon
    s(0); s(0)           # CurrentState, ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(2)
obj("coin", 4)           # Alpha
obj("ring", 5)           # Bravo

# RESTRICTIONS -- (Type, Var1, Var2, Var3, FailMessage).  Types 0 and 3 both
# serialise as #Type #Var1 #Var2 #Var3 $FailMessage.
#
# Type 0 Var1 = 1 is "any object", Var2 = 3 is "is visible to", Var3 = 0 is the
# player: xfiles' restriction 1.  Var1 = 0 is the negated form, "no object is
# visible to the player", which is false whenever anything is in the room --
# the sure-failing twin.
def VIS(msg):   return (0, 1, 3, 0, msg)
def NOVIS(msg): return (0, 0, 3, 0, msg)
# Type 3 Var1 = 0 is the player, Var2 = 2 is "is alone", Var2 = 3 is "is not
# alone", Var3 = -1 (no target): xfiles' restriction 2 and its twin.  With the
# game's only NPC nowhere, "alone" is true and "not alone" is the sure-failer.
def ALONE(msg):    return (3, 0, 2, -1, msg)
def NOTALONE(msg): return (3, 0, 3, -1, msg)

# TASKS
def task(cmd, text, restrs=(), mask="", repeat=1):
    s(1); s(cmd)
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(repeat)            # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(3)                 # Where: all rooms
    s("")                # Question
    s(len(restrs))
    for r in restrs:
        for field in r:
            s(field)
    s(0)                 # Actions
    s(mask)              # RestrMask

s(13)
task("pa %object%",   "PA PASS.")
task("burn %object%", "BURN PASS.")
task("pb %object%",   "PB PASS.", [VIS("PB FAIL.")],   "#")
task("pc %object%",   "PC PASS.", [ALONE("PC FAIL.")], "#")
# xfiles task 24's exact shape: a message on the visibility test, none on the
# alone test.
task("pd %object%",   "PD PASS.", [VIS("PD FAIL A."), ALONE("")],
     "#A#")
task("pe %object%",   "PE PASS.", [VIS("PE FAIL A."), ALONE("PE FAIL B.")],
     "#A#")
task("pf %object%",   "PF PASS.", [NOVIS("PF FAIL.")],    "#")
task("pg %object%",   "PG PASS.", [NOTALONE("PG FAIL.")], "#")
task("ph %object%",   "PH PASS.", [NOVIS("PH FAIL A."), ALONE("PH FAIL B.")],
     "#A#")
task("pi %object%",   "PI PASS.", [VIS("PI FAIL A."), NOTALONE("PI FAIL B.")],
     "#A#")
task("pj %object%",   "PJ PASS.", [NOVIS(""), NOTALONE("PJ FAIL B.")],
     "#A#")
task("pk %object%",   "PK PASS.", [NOVIS("PK FAIL A."), NOTALONE("")],
     "#A#")
task("pl %object%",   "PL PASS.", [NOVIS("")], "#")

# EVENTS -- none.
s(0)

# NPCS -- one, and it starts NOWHERE, as xfiles' NPC 12 does.
s(1)
s("Ghost")               # Name
s("")                    # Prefix
s(0)                     # V$Alias count
s("Not in play.")        # Descr
s(0)                     # StartRoom: nowhere
s("")                    # AltText
s(0)                     # Task
s(0)                     # Topics
s(0)                     # Walks
s(0)                     # ShowEnterExit
s("")                    # InRoomText
s(0)                     # Gender
# [4] Res: nothing, sound and graphics both being off.

s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4BURN.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
