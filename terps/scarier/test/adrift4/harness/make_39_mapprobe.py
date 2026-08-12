#!/usr/bin/env python3
"""ADRIFT 3.9 automap stress probe.

ADRIFT 3.9 / 4 have no authored map pages or coordinates -- run390/run400 derive
layout from the exit graph (Form29; Scarier: scmap.cpp).  This probe exercises
the cases that *do* exist, for use with scmap_dump:

    ./scmap_dump map39.taf [script.txt] [-o out.ppm] [-all] [-grid]

Rooms (0-based keys as drawn; exits use 1-based Dest):

    0 Hub       start; compass + Up/Down/In/Out badges + SE/SW
    1 North     duplex N of Hub
    2 East      one-way E from Hub (return via NE to Hub; NE is not drawn --
                rooms are not NE-aligned, and v4 invents no Source/Dest anchors)
    3 Cellar    S of Hub and Down badge from Hub; gated S to Door Room
    4 West      duplex W of Hub
    5 Attic     Up from Hub only (badge dest; never placed on the grid)
    6 Closet    In from Hub only (badge)
    7 Outside   Out from Hub only (badge)
    8 SW Corner eight-point SW of Hub (duplex)
    9 SE Corner eight-point SE of Hub
   10 Door Room S of Cellar; exit restricted until OPEN DOOR
   11 Hidden    HideOnMap; reached by task (nw / enter hidden), not a map exit.
                A compass exit into HideOnMap makes run390 draw a runaway stub
                (no grid cell to aim at). Corpus pattern: task teleport or
                U/D/In/Out badge -- see GATEWAY, PK Girl, Toxically Earth.

Tasks: OPEN DOOR (gates Cellar-South); NW / ENTER HIDDEN (teleport to Hidden).

Suggested scmap_dump views (no "too complex" case):

    start     (empty script)           Hub alone + stubs/badges to unseen
    reveal    map39_reveal.txt         walk the graph, leave door closed
    dooropen  map39_dooropen.txt       after OPEN DOOR, Cellar--Door Room dotted
    hidden    map39_hidden.txt         expect "player is in a room hidden..."
    allseen   -all                     full layout of non-hidden rooms

Usage: python3 make_39_mapprobe.py [out.taf]
"""
import hashlib
import sys
from pathlib import Path

L = []
def s(x): L.append(str(x))

# HEADER (MStartupText = lines then "**")
s("Map probe 3.9."); s("**")
s(0)                    # StartRoom (0-based Hub)
s("You have won."); s("**")

# GLOBAL
s("Map Probe 390")      # GameName
s("SCARE probe")        # GameAuthor
s("I don't understand.")
s(2)                    # Perspective
s(0)                    # ShowExits (keep transcripts quiet)
s(0)                    # WaitTurns
s(1)                    # DispFirstRoom
s(0)                    # BattleSystem
s(0)                    # MaxScore
s("Player")
s(0)                    # PromptName
s("A map-test subject.")
s(0)                    # Task (no AltDesc)
s(0)                    # Position
s(0)                    # ParentObject
s(0)                    # PlayerGender
s(100)                  # MaxSize
s(100)                  # MaxWt
s(1)                    # EightPointCompass -- enables NE/SE/SW/NW
s(0)                    # bNoDebug
s(0)                    # NoScoreNotify
s(0)                    # NoMap
s(0)                    # bNoAutoComplete
s(0)                    # bNoControlPanel
s(0)                    # bNoMouse
s(0)                    # Sound
s(0)                    # Graphics
s(0)                    # iUnk1 SizeMultiple
s(0)                    # iUnk2 WeightMultiple

# Direction slots with eight-point on:
# 0 N  1 E  2 S  3 W  4 Up  5 Down  6 In  7 Out  8 NE  9 SE  10 SW  11 NW
N_EXITS = 12

def exit_slot(dest, var1=0, var2=0):
    """dest is 1-based room number; var1>0 marks a movement restriction."""
    return (dest, var1, var2)

def room(short, long_, exits=None, hide=0):
    s(short)
    s(long_)
    s("")                   # LastDesc
    slots = dict(exits or {})
    for d in range(N_EXITS):
        if d not in slots:
            s(0)
        else:
            dest, v1, v2 = slots[d]
            s(dest); s(v1); s(v2)
    s("")                   # AddDesc1
    s(0)                    # Task1
    s("")                   # AddDesc2
    s(0)                    # Task2
    s(0)                    # Obj
    s("")                   # AltDesc
    s(0)                    # TypeHideObjects
    s(hide)                 # HideOnMap

# Task index (0-based) referenced by exit Var1 (= index+1):
OPEN_DOOR = 0               # exit Var1 = OPEN_DOOR+1 == 1

# $Long is one TAF line; use <br> for visible breaks (LOOK reprints this).
HUB_LONG = (
    "Walk the graph to mark rooms seen.<br>"
    "OPEN DOOR gates Cellar-South (dotted once both rooms are seen).<br>"
    "Up/Down/In/Out are badges. East is one-way (NE back to Hub).<br>"
    "ENTER HIDDEN (or NW) teleports to a HideOnMap room -- no compass exit "
    "(run390 draws a runaway stub for those).<br>"
    "Walkthrough (ends on Hub; skips Hidden and Door Room):<br>"
    "n / s / e / ne / w / e / s / n / u / d / in / out / out / in / sw / ne / se / nw<br>"
    "Then in Cellar: s / open door / s / n / n  (unlock, visit Door Room, return to Hub)"
)

s(12)
room("Hub",
     HUB_LONG,
     {0: exit_slot(2),          # N -> North
      1: exit_slot(3),          # E -> East (one-way)
      2: exit_slot(4),          # S -> Cellar
      3: exit_slot(5),          # W -> West
      4: exit_slot(6),          # Up -> Attic (badge)
      5: exit_slot(4),          # Down -> Cellar (badge to same as S)
      6: exit_slot(7),          # In -> Closet (badge)
      7: exit_slot(8),          # Out -> Outside (badge)
      9: exit_slot(10),         # SE -> SE Corner
      10: exit_slot(9)})        # SW -> SW Corner
      # no NW: Hidden is task-only (see HUB_LONG)
room("North",
     "Duplex south to Hub.",
     {2: exit_slot(1)})
room("East",
     "One-way from Hub. No west exit -- Northeast returns to Hub.",
     {8: exit_slot(1)})         # NE -> Hub
room("Cellar",
     "North to Hub. South to Door Room (restricted until OPEN DOOR). "
     "Up badge also reaches here from Hub. Placed south of Hub so Door Room "
     "does not collide with SE Corner.",
     {0: exit_slot(1),          # N -> Hub
      2: exit_slot(11, OPEN_DOOR + 1, 0),  # S gated: open when task done
      4: exit_slot(1)})         # Up -> Hub
room("West",
     "Duplex east to Hub.",
     {1: exit_slot(1)})
room("Attic",
     "Badge-only destination (Up from Hub). Down to Hub. Never grid-placed.",
     {5: exit_slot(1)})
room("Closet",
     "Badge-only In from Hub. Out to Hub.",
     {7: exit_slot(1)})
room("Outside",
     "Badge-only Out from Hub. In to Hub.",
     {6: exit_slot(1)})
room("SW Corner",
     "Eight-point duplex. Northeast to Hub.",
     {8: exit_slot(1)})         # NE -> Hub
room("SE Corner",
     "Eight-point. Northwest to Hub.",
     {11: exit_slot(1)})
room("Door Room",
     "North to Cellar. Arrive only after OPEN DOOR.",
     {0: exit_slot(4)})
room("Hidden",
     "HideOnMap. Out returns to Hub. Map is blank while you stand here.",
     {7: exit_slot(1)},         # Out -> Hub (badge; Spirit's Flight storeroom pattern)
     hide=1)

# OBJECTS
s(0)

# TASKS
# Type 1 action: move player (Var1=0) to room (Var2=0) Var3=0-based room.
HIDDEN_ROOM = 11

def task(cmds, text, actions=None, where_type=3, where_room=None, show_room=0):
    if isinstance(cmds, str):
        cmds = [cmds]
    s(len(cmds) - 1)
    for c in cmds:
        s(c)
    s(text)                 # CompleteText
    s("")                   # ReverseMessage
    s("")                   # RepeatText
    s("")                   # AdditionalMessage
    s(show_room)            # ShowRoomDesc
    s(1)                    # Repeatable
    s(0)                    # Reversible
    s(0); s("")             # ReverseCommand
    s(where_type)           # Where
    if where_type == 1:
        s(where_room)       # single room, 0-based
    s("")                   # Question
    s(0)                    # Restrictions
    if not actions:
        s(0)
    else:
        s(len(actions))
        for a in actions:
            for f in a:
                s(f)

s(2)
task("open door",
     "Door unlocked. Cellar south should draw dotted once both rooms are seen.")
# No compass exit into HideOnMap (run390 runaway stub). Task teleport instead
# (GATEWAY / PK Girl pattern). Also answer plain "nw" from Hub.
task(["nw", "northwest", "enter hidden"],
     "You slip into a room the map refuses to draw.",
     actions=[(1, 0, 0, HIDDEN_ROOM)],  # move player to Hidden
     where_type=1, where_room=0,        # Hub only
     show_room=HIDDEN_ROOM + 1)         # ShowRoomDesc is 1-based room index

# EVENTS / NPCS
s(0)
s(0)

# tail
s(0)                    # RoomGroups
s(0)                    # Synonyms
s(0)                    # Variables
s(0)                    # ALRs
s(0)                    # CustomFont
s("2026")               # CompileDate
s("    Wild    ")       # sPassword

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
# Same V3.90 signature + VB PRNG stream as make_39_probe.py / make_39_whereprobe.py.
# run390 decrypts the 14-byte header to "Version 3.90\r\n" and requires
# Mid(sPassword,5,4)=="Wild".  run400 uses a different signature and will reject
# this file with the same "not an adventure file" error.
SIG = bytes([0x3c, 0x42, 0x3f, 0xc9, 0x6a, 0x87, 0xc2, 0xcf,
             0x94, 0x45, 0x37, 0x61, 0x39, 0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14):
    draw()
obf = bytes(b ^ draw() for b in body)
taf = SIG + obf

here = Path(__file__).resolve().parent
out = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "map39.taf"
out.write_bytes(taf)
out.with_suffix(out.suffix + ".plain").write_bytes(body)
print("wrote %s (%d bytes, sha256 %s)" % (out, len(taf), hashlib.sha256(taf).hexdigest()))

