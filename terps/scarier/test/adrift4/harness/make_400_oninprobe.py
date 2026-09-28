#!/usr/bin/env python3
"""ADRIFT 4.0 probe: how %onin_X% and %state_X% pick their object, and what
%onin_X% prints for something with objects both on and in it.

Top-level review items 2.4 and 2.7 (the review file was removed after
0d337a103; see that commit).  Before the fix, scvars.cpp resolved %in_X%
and %on_X% by the lowest-indexed object whose Short is X
(var_marker_object_by_short, measured on escape_to_new_york), but %onin_X%
and %state_X% through uip_match(), which only offers objects the player
can see and keeps the LAST match.  run400's
substitute_percent_tags (47A3DC) runs the same index-order Short loop for
all four markers -- %onin_ at 479BD5 with lister mode 2, %state_ at 47A0B2
over objects whose CurrentState is not 0 -- so this probe puts the
low-indexed namesake out of sight:

    obj  short  where  what
    0    box    Alpha  container + surface: apple on, coin and gem in
    1    box    Bravo  container + surface: cup on, pen in
    5    lever  Alpha  state 1 of "Up High|Down Low"
    6    lever  Bravo  state 2 of "Left Side|Right Side"
    7    knob   Alpha  state 1 of "Dull|Shiny"
    8    knob   Bravo  no states at all

(2-4 are the crate, rack and tin below, all in Bravo)

and reads, from Bravo (the start room) and then from Alpha:

    OI=[%onin_box%]  IN=[%in_box%]  ON=[%on_box%]
    ST=[%state_lever%]  KN=[%state_knob%]

Lowest index means box 0 ("apple ... coin and gem"), "up high" and "dull"
from both rooms; the parser's choice would be box 1, "right side" and a
failure for the stateless knob, from Bravo.

The crate, rack and tin in Bravo answer 2.7, the shape of the nested
", and ... inside" clause, for each count of the two lists:

    crate  plate and bowl on, key and ring in    (2 on, 2 in)
    rack   fork and spoon on, nail in            (2 on, 1 in)
    tin    nut and bolt in, nothing on            (the unnested in clause)

with box 0 and box 1 giving (1 on, 2 in) and (1 on, 1 in).

The chest and jar, also in Bravo and both CLOSED (Openable 6), ask whether
%onin_X%'s in-half checks openness the way %in_X%'s does (whatisinon's
shared in-branch, 46A421: container, global_52 < 6, obhere):

    chest  hat on, sock in, closed    CH=[%onin_chest%]
    jar    bean in, closed            JA=[%onin_jar%]

cmdfile_onin2.txt opens both and looks again.

Usage:
    python3 make_400_oninprobe.py p4ONIN.plain
    python3 taftool.py pack p4ONIN.plain <any 4.0 donor .taf> p4ONIN.taf

Drive it with, from ~/adrift-battle/runner/wine:
    ./par.sh jobs_onin.txt 1     (row: onin|p4ONIN.taf|cmdfile_onin.txt|run400.exe)
with cmdfile_onin.txt = look, w, look, e, look.  The chest/jar rows:
    ./par.sh jobs_onin2.txt 1    (row: onin2|p4ONIN.taf|cmdfile_onin2.txt|run400.exe)
with cmdfile_onin2.txt = look, open chest, look, open jar, look.

Measured (Adrift_305_onin.txt): box 0, "up high" and "dull" from both rooms,
but from Bravo OI/IN/ON are empty -- whatisinon lists nothing for an object
not here.  Nested clause: "A plate and a bowl are on the crate, and inside is
a key and a ring." (singular "is" whatever the count).
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Onin and state resolution probe.")
s(1)                     # StartRoom: Bravo (0-based)
ml("You have won.")

# GLOBAL
s("Onin Probe 400")
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

READS = ("OI=[%onin_box%] IN=[%in_box%] ON=[%on_box%]"
         " ST=[%state_lever%] KN=[%state_knob%]")
s(2)
room("Alpha", "Alpha.  " + READS, {1: 2})
room("Bravo", "Bravo.  " + READS + "  CR=[%onin_crate%] RA=[%onin_rack%]"
     " TI=[%onin_tin%] CH=[%onin_chest%] JA=[%onin_jar%]", {3: 1})

# OBJECTS.  Statics first, as the status probe lays them out; then the
# stateful levers and knobs; then the contents, whose Parent is the index
# among Container (or Surface) objects in object order.
OBJS = []

def static(short, room, container, surface, openable=0):
    OBJS.append(("static", short, room, container, surface, openable))

def dynamic(short, pos, parent=0, states=None):
    OBJS.append(("dynamic", short, pos, parent, states))

static("box",   0, 1, 1)     # 0  container 0, surface 0
static("box",   1, 1, 1)     # 1  container 1, surface 1
static("crate", 1, 1, 1)     # 2  container 2, surface 2
static("rack",  1, 1, 1)     # 3  container 3, surface 3
static("tin",   1, 1, 0)     # 4  container 4
dynamic("lever", 4 + 0, states="Up High|Down Low")        # 5
dynamic("lever", 4 + 1, states="Left Side|Right Side")    # 6
dynamic("knob",  4 + 0, states="Dull|Shiny")              # 7
dynamic("knob",  4 + 1)                                   # 8
IN, ON = 2, 3
dynamic("apple", ON, 0)
dynamic("coin",  IN, 0)
dynamic("gem",   IN, 0)
dynamic("cup",   ON, 1)
dynamic("pen",   IN, 1)
dynamic("plate", ON, 2)
dynamic("bowl",  ON, 2)
dynamic("key",   IN, 2)
dynamic("ring",  IN, 2)
dynamic("fork",  ON, 3)
dynamic("spoon", ON, 3)
dynamic("nail",  IN, 3)
dynamic("nut",   IN, 4)
dynamic("bolt",  IN, 4)
# The closed chest and jar come last so every index above stays put: the
# chest is container 5 and surface 4, the jar container 6.
static("chest", 1, 1, 1, 6)
static("jar",   1, 1, 0, 6)
dynamic("hat",   ON, 4)
dynamic("sock",  IN, 5)
dynamic("bean",  IN, 6)

s(len(OBJS))
for o in OBJS:
    if o[0] == "static":
        _, short, room_, container, surface, openable = o
        s("a"); s(short)
        s(0)             # V$Alias count
        s(1)             # Static
        s("A plain thing.")
        s(0)             # InitialPosition -- unused for a static object
        s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
        s(1); s(room_ + 1)   # Where: one room, 1-based
        s(container); s(surface); s(52)   # Container, Surface, Capacity
        s(openable)      # Openable: 0 none, 6 closed
        if openable:
            s(0)         # Key
        s(0)             # SitLie
        s(0)             # Readable
        s(0)             # CurrentState
        s(0)             # ListFlag
        s(""); s(0)      # InRoomDesc, OnlyWhenNotMoved
    else:
        _, short, pos, parent, states = o
        s("a"); s(short)
        s(0)             # V$Alias count
        s(0)             # Static
        s("A test object.")
        s(pos)           # InitialPosition: 2 in, 3 on, 4 + room
        s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
        s(0); s(0); s(0)     # Container, Surface, Capacity
        s(0); s(0); s(parent)    # Wearable, SizeWeight, Parent
        s(0)             # Openable
        s(0); s(0); s(0); s(0)   # SitLie, Edible, Readable, Weapon
        if states:
            s(1 if "Up" in states or "Dull" in states else 2)  # CurrentState
            s(states)
            s(0)         # StateListed
        else:
            s(0)         # CurrentState: none
        s(0)             # ListFlag
        s(""); s(0)      # InRoomDesc, OnlyWhenNotMoved

# TASKS, EVENTS, NPCS -- none.
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
out = sys.argv[1] if len(sys.argv) > 1 else "p4ONIN.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
