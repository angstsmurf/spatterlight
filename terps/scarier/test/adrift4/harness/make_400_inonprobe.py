#!/usr/bin/env python3
"""ADRIFT 4.0 probe: how does an examine word an object that is BOTH a
surface and an open container?

Measured live on The_X-Files_A_New_Beginning.taf (Adrift_22_xfiles.txt line 9),
run400 answers `x desk` with the surface clause FIRST and the container clause
run onto the end of the same sentence:

    Your Desk is open.  Your Coffee Mug and The Memo are on Your Desk, and
    inside is Gun Holster, Your Cell Phone, Neatly Wrapped Gift and Your
    Badge.

The Runner does both halves in one helper, whatisinon (run400
Proc_19_26_46A950 @46A950), and the join is a flag, var_9E, set once the "on"
half has printed.  The "in" half tests it FIRST (loc_46A786) and appends
", and inside is " plus a plain list.  Its count-1 branch
("<a> is inside <cont>") and count-2 branch ("<a> and <b> are inside <cont>")
are both guarded on `var_9E = 0`, so on that reading a surface listing forces
the joined wording WHATEVER the in-count -- and the `If var_9E = 1` arms
nested inside those two branches (loc_46A4F1, loc_46A66C) are dead code.

That reading is what scarier implements (sclibrar.cpp lib_list_in_on_object),
but the xfiles line only measures in-count 4, which would take the joined
wording either way.  Two corpus goldens rest on the unmeasured counts --
ADRIFTMAS_Party (in-count 2) and yonastoundingcastle (in-count 1) -- so
measure them.

The cells, all examined in one room:

    desk1   on 1, in 1     the yonastoundingcastle shape
    desk2   on 1, in 2     the ADRIFTMAS_Party shape
    desk3   on 1, in 3     both halves in their >= 3 wording
    desk4   on 2, in 1     the on-half's own count-2 branch, joined
    desk5   on 0, in 1     surface flag set but nothing on it: var_9E stays 0,
                           so this must read "A cog is inside desk5."
    box     container only, in 1     the unjoined baseline
    tray    surface only,  on 1      the unjoined baseline
    crate   held, on 1, in 1, and starts CLOSED, so `open crate` is the
            control (the open handler refuses a container the player is not
            carrying, hence held rather than standing in the room):
            openclose() passes the containers-only mode (@475852), so it must
            print the in-half alone, unjoined, and `x crate` right afterwards
            must then print both.
    bag     held, on 1, in 1         the same, reached from `i` (inventory
                                     passes the both-halves mode too, @45C2C8)

MEASURED AND CLOSED 2026-08-25, run400 on p4INON.taf, Adrift_1_p4inon.txt,
all 10 commands echoed.  Scarier matches every row word for word:

    x desk1   Furniture.  A pin1 is on the desk1, and inside is a cog1.
    x desk2   ... A pin2 is on the desk2, and inside is a cog2a and a cog2b.
    x desk3   ... A pin3 is on the desk3, and inside is a cog3a, a cog3b and
              a cog3c.
    x desk4   ... A pin4a and a pin4b are on the desk4, and inside is a cog4.
    x desk5   Furniture.  A cog5 is inside the desk5.
    x box     Furniture.  A cogbox is inside the box.
    x tray    Furniture.  A pintray is on the tray.
    open crate  You open the crate.  A cogcrate is inside the crate.
    x crate   Furniture.  The crate is open.  A pincrate is on the crate, and
              inside is a cogcrate.
    i         You are carrying a crate and a bag.  A pincrate is on the crate,
              and inside is a cogcrate.  A pinbag is on the bag, and inside is
              a cogbag.

desk1 settles it: at in-count 1 with a surface listing already printed, the
Runner says ", and inside is a cog1." and NOT a second sentence, so the
`If var_9E = 1` arms nested inside the count-1 and count-2 branches really
are dead code and lib_list_in_object()'s guard order is right.  desk5 is the
other side of the same flag -- surface flag set, nothing on it, var_9E stays
0, and the unjoined "A cog5 is inside the desk5." comes back.  ADRIFTMAS_Party
(in-count 2) and yonastoundingcastle (in-count 1) now rest on measurement.

`open crate` confirms the containers-only mode prints the in-half alone and
unjoined, and `i` confirms inventory passes the both-halves mode.

FOOTGUN, cost half an hour: this generator used to write **Capacity 99**, and
that is invalid -- run400 hangs for ever at "Loading...", which measure.sh
reports as "first command never reached the game", indistinguishable from a
broken feed.  Capacity packs tens = object count, units = SIZE INDEX
(scobjcts.cpp:674-706), and size index 9 is out of range; 52 is what to write.
p4INON.taf had never once loaded before 2026-08-25 for this reason.  Bisect a
suspect .taf with loadtest.sh: the window title carries the game name only if
the file actually loaded.

Usage:
    python3 make_400_inonprobe.py p4INON.plain
    python3 taftool.py pack p4INON.plain <donor.taf> p4INON.taf
    (then, in ~/adrift-battle/runner/wine/)
    LOAD_SLEEP=22 sh measure.sh p4INON.taf cmdfile_inon.txt
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("In/on listing probe.")
s(0)                     # StartRoom: the one room (0-based)
ml("You have won.")

# GLOBAL
s("In-On Probe 400")
s("SCARE probe")
s("I don't understand.")
s(1)                     # Perspective: second person
s(0)                     # ShowExits
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

# ROOMS -- one, no exits.
s(1)
s("Probe Room")
s("A room with furniture in it.")
for _ in range(8):
    s(0)
s(0)                     # Alts
s(0)                     # HideOnMap

# OBJECTS
#
# All dynamic, so the schema is the plain one: no ROOM_LIST1 Where and no
# static Parent fixup.  InitialPosition 4 + room is "in room 0"; 1 with
# Parent 0 is "held by the player"; 2 is "in container #Parent" and 3 is "on
# surface #Parent", where #Parent counts only CONTAINERS (or only SURFACES)
# in object order -- an object that is both appears in both numberings.
def obj(short, container=0, surface=0, position=4, parent=0, openable=0):
    s("a")               # Prefix
    s(short)             # Short
    s(0)                 # V$Alias count
    s(0)                 # Static
    s("Furniture.")      # Description
    s(position)          # InitialPosition
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(container)         # Container
    s(surface)           # Surface
    s(52 if container else 0)      # Capacity: tens = object count, units =
                         # SIZE INDEX, and 9 is out of range -- a 99 here
                         # makes run400 hang for ever at "Loading..."
                         # (bisected 2026-08-25 with loadtest.sh; this
                         # file had never once loaded).  See
                         # scobjcts.cpp:674-706.
    s(0)                 # Wearable
    s(0)                 # SizeWeight
    s(parent)            # Parent
    s(openable)          # Openable: 0 = has no open/closed state, i.e. always
                         # open (OBJ_WONTCLOSE), so nothing prints an "is
                         # open." clause ahead of the listing and no #Key
                         # field follows.  6 = closed, and then a #Key does
                         # follow (0 = no key needed).
    if openable == 6:
        s(0)             # Key
    s(0)                 # SitLie
    s(0); s(0)           # Edible, Readable
    s(0)                 # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

# object index:      0      1      2      3      4      5     6      7      8
# containers:        0      1      2      3      4      5     -      6      7
# surfaces:          0      1      2      3      4      -     5      6      7
FURNITURE = [
    ("desk1", 1, 1, 0),  # 0
    ("desk2", 1, 1, 0),  # 1
    ("desk3", 1, 1, 0),  # 2
    ("desk4", 1, 1, 0),  # 3
    ("desk5", 1, 1, 0),  # 4
    ("box",   1, 0, 0),  # 5
    ("tray",  0, 1, 0),  # 6
    ("crate", 1, 1, 6),  # 7 -- closed, and held (see below)
]
# bag is object 8, held rather than in the room.

# (short, position, parent)
CONTENTS = [
    ("pin1",   3, 0),    # on desk1
    ("cog1",   2, 0),    # in desk1
    ("pin2",   3, 1),    # on desk2
    ("cog2a",  2, 1),    # in desk2
    ("cog2b",  2, 1),
    ("pin3",   3, 2),    # on desk3
    ("cog3a",  2, 2),    # in desk3
    ("cog3b",  2, 2),
    ("cog3c",  2, 2),
    ("pin4a",  3, 3),    # on desk4
    ("pin4b",  3, 3),
    ("cog4",   2, 3),    # in desk4
    ("cog5",   2, 4),    # in desk5, which has nothing on it
    ("cogbox", 2, 5),    # in box
    ("pintray",3, 5),    # on tray    (surface index 5)
    ("pincrate", 3, 6),  # on crate   (surface index 6)
    ("cogcrate", 2, 6),  # in crate   (container index 6)
    ("pinbag", 3, 7),    # on bag     (surface index 7)
    ("cogbag", 2, 7),    # in bag     (container index 7)
]

s(len(FURNITURE) + 1 + len(CONTENTS))
for short, container, surface, openable in FURNITURE:
    obj(short, container=container, surface=surface, openable=openable,
        position=1 if short == "crate" else 4)
obj("bag", container=1, surface=1, position=1, parent=0)   # 8, held
for short, position, parent in CONTENTS:
    obj(short, position=position, parent=parent)

# TASKS -- one, so the file is never a zero-task edge case.
s(1)
s(1); s("xyzzy")
s("Nothing happens.")    # CompleteText
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

s(0)                     # EVENTS
s(0)                     # NPCS
s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4INON.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
