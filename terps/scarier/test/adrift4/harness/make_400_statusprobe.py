#!/usr/bin/env python3
"""ADRIFT 4.0 probe: which object does %status_<name>% resolve to?

briefcase_solution.txt turn 10 (`nw`, into the Study) is the one line in that
replay where run400 and scarier disagree:

    run400   ... A door (closed) leads out to the south-east.
    scarier  ... A door (open)   leads out to the south-east.

The Study's Long carries `A door (%status_door%)`, and briefcase has TWO
openable objects whose Short is "door": obj17, in the Music Room, and obj23,
in the End of Corridor.  The walkthrough opens obj23 one turn earlier (TASK
30, `open*door`, `ACT type=2 v1=1 v2=0`) and then walks north-west, so obj23
is open and obj17 has never been touched.  scarier answers "open", so it is
resolving the name against obj23; run400 answers "closed", so it is not.

scarier resolves the name with the parser: `scvars.cpp`'s `status_` branch
calls `uip_match ("%object%", name + 7, game)`, which only ever offers objects
the player can see.  That is one of at least three rules that fit the single
briefcase cell:

    local        the object of that name the player can see        -> obj23
    global-first the lowest-indexed object of that name, anywhere  -> obj17
    referenced   whatever the last command referred to             -> obj23

briefcase separates `local` from `global-first` and nothing else, and one cell
is not a rule.  Three rooms and two doors separate all three:

    Alpha   holds door A  (object 0, the LOW index)
    Bravo   holds door B  (object 1, the HIGH index)
    Charlie holds no door at all

and every room's Long is `<name>.  ST=[%status_door%]`.  The session opens and
closes the two doors so that each cell has a different answer under each rule:

    turn  where  A       B       local       global-first  referenced
    look  Alpha  closed  closed  closed      closed        closed
    e     Bravo  closed  closed  closed      closed        closed
    open door -- opens B (the only door the player can see)
    look  Bravo  closed  OPEN    open        closed        open
    w     Alpha  closed  open    closed      closed        open      <- CELL 1
    open door -- opens A
    e     Bravo  open    open    open        open          open
    close door -- closes B
    look  Bravo  open    CLOSED  closed      open          closed    <- CELL 2
    e     Charlie                (none)      open          closed    <- CELL 3

CELL 1 splits `referenced` off from the other two; CELL 2 splits
`global-first` off; CELL 3 is the three-way one -- "open" means global-first,
"closed" means referenced, and anything else (an error string, an empty
parenthesis) means the lookup really is scoped to what the player can see and
simply fails when there is nothing to see.

Both doors are static, as a real ADRIFT door is, and both start
`Openable = 6` (closed) with no key.

Usage:
    python3 make_400_statusprobe.py p4STATUS.plain
    python3 taftool.py pack p4STATUS.plain <donor.taf> p4STATUS.taf

Drive it with, from ~/adrift-battle/runner/wine:
    sh fast.sh p4STATUS.taf cmdfile_status.txt run400
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Status resolution probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Status Probe 400")
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

# ROOMS -- Alpha east to Bravo, Bravo west to Alpha and east to Charlie,
# Charlie west to Bravo.  Exit order is north, east, south, west, up, down,
# in, out; a zero means no exit, a real one is Dest (1-based) Var1 Var2 Var3.
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
room("Alpha",   "Alpha.  ST=[%status_door%]",   {1: 2})
room("Bravo",   "Bravo.  ST=[%status_door%]",   {1: 3, 3: 1})
room("Charlie", "Charlie.  ST=[%status_door%] AL=[%status_gate%]"
                " AP=[%status_a gate%] HA=[%status_hatch%]"
                " PF=[%status_the grate%] PB=[%status_grate%]", {3: 2})

# OBJECTS -- five static ones.  Object 0 is in Alpha and object 1 in Bravo,
# so object 0 is the low index AND the far one whenever the player stands in
# Bravo, which is what makes the three door cells decisive.  Static because
# the library open command refuses a dynamic object the player is not
# carrying ("You are not carrying the door!"), and because a real ADRIFT door
# is static anyway.
#
# The last three sit in Charlie and answer two more questions that the door
# cells cannot, both read off Charlie's Long:
#
#   AL=[%status_gate%]   "gate" is object 2's ALIAS, its Short being "portal",
#                        so an answer at all means aliases are searched.
#   HA=[%status_hatch%]  object 3 is a "hatch" that is NOT openable and object
#                        4 is a "hatch" that is; "open" means the search skips
#                        past an unopenable namesake, anything else means it
#                        takes the first name match and then fails.
def obj(short, room, openable, aliases=(), prefix="a"):
    s(prefix)            # Prefix
    s(short)             # Short
    s(len(aliases))      # V$Alias count
    for a in aliases:
        s(a)
    s(1)                 # Static
    s("A plain thing.")  # Description
    s(0)                 # InitialPosition -- unused for a static object
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(1); s(room + 1)    # Where: ROOM_LIST1 Type 1 = one room, Room is 1-based
    s(0); s(0); s(0)     # Container, Surface, Capacity
    s(openable)          # Openable: 0 = not, 5 = open, 6 = closed
    if openable:
        s(0)             # Key: 0 = none
    s(0)                 # SitLie
    s(0)                 # Readable
    s(0)                 # CurrentState: 0, so no States/StateListed
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(6)
obj("door",   0, 6)                      # object 0 -> Alpha, closed
obj("door",   1, 6)                      # object 1 -> Bravo, closed
obj("portal", 2, 5, aliases=("gate",))   # object 2 -> Charlie, open, alias
obj("hatch",  2, 0)                      # object 3 -> Charlie, NOT openable
obj("hatch",  2, 5)                      # object 4 -> Charlie, open
obj("grate",  2, 5, prefix="the")        # object 5 -> Charlie, open, prefixed

# TASKS, EVENTS, NPCS -- none; open and close are library commands.
s(0)
s(0)
s(0)

s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES -- none.
s(0)

s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4STATUS.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
