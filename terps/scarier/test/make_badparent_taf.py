#!/usr/bin/env python3
"""
Generate a synthetic ADRIFT 4.0 TAF for the invalid-object-parent regression
test (test/badparent_test.c).  No copyrighted game data required.

Some published games (e.g. The Timmy Reid Adventure, Blood Relatives) ship
dynamic objects whose initial location names a nonexistent NPC or container --
"held by NPC -1", "in container -1", and the like.  SCARE used to store the
resulting out-of-range parent index verbatim and then assert-crash on the very
first turn-update (gs_npc_location / gs_object_openness).  gs_create() now
validates the parent and hides such objects instead, matching the way it
already handles an object placed in an out-of-bounds room.

This game has one room, one NPC, and three dynamic objects:
  * "amulet" -- InitialPosition "held", Parent 99  -> held by nonexistent NPC
  * "coin"   -- InitialPosition "in container", Parent -1 -> bad container
  * "ring"   -- InitialPosition "held", Parent 1  -> held by the real NPC (sane)

The test loads the game (which runs the first turn-update) and checks it does
not crash, that the two bad objects are hidden, and that the good one is not.

Run:  python3 make_badparent_taf.py badparent_test.taf
"""
import sys, zlib

SEP = "\xbd\xd0"
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,
             0x93,0x45,0x3e,0x61,0x39,0xfa])
HEADER_EXTRA = bytes(8)

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# ---- HEADER ----
ml("A synthetic room for testing invalid object parents.")
s(0)                                        # StartRoom
ml("You have finished the bad-parent test.")

# ---- GLOBAL ----
s("Bad Parent Test"); s("SCARE regression"); s("I don't understand.")
s(2)                                        # Perspective
s(1)                                        # ShowExits
s(0)                                        # WaitTurns
s(1)                                        # DispFirstRoom
s(0)                                        # BattleSystem OFF
s(0)                                        # MaxScore
s("Player"); s(0); s("A tester.")            # PlayerName, PromptName, PlayerDesc
s(0)                                        # Task (==0 -> no AltDesc)
s(0); s(0); s(0)                            # Position, ParentObject, PlayerGender
s(100); s(100)                             # MaxSize, MaxWt
s(0)                                        # EightPointCompass (-> 8 exits)
s(0); s(0); s(0); s(0); s(0); s(0)          # NoDebug,NoScoreNotify,NoMap,NoAuto,NoCtl,NoMouse
s(0); s(0)                                  # Sound, Graphics (off)
s(0); s("")                                 # StatusBox, StatusBoxText
s(0); s(0); s(0)                            # iUnk1, iUnk2, Embedded

# ---- ROOMS: count = 1 ----
s(1)
s("Test Room")
s("A bare room for testing object placement.")
for _ in range(8):                          # 8 exits, all absent
    s(0)
s(0)                                        # Alts count
s(0)                                        # bHideOnMap

# ---- OBJECTS: count = 3 ----
s(3)
def obj(short, initial_position, parent):
    s("a")                                  # Prefix
    s(short)                                # Short
    s(0)                                    # V$Alias count
    s(0)                                    # BStatic = 0 (dynamic)
    s("")                                   # Description
    s(initial_position)                     # InitialPosition
    s(0)                                    # Task
    s(0)                                    # BTaskNotDone
    s("")                                   # AltDesc
    # not static -> no Where room list
    s(0)                                    # BContainer
    s(0)                                    # BSurface
    s(0)                                    # Capacity
    s(0)                                    # BWearable          (?!BStatic)
    s(0)                                    # SizeWeight
    s(parent)                               # Parent             (?!BStatic)
    s(0)                                    # Openable (==0 -> no Key)
    s(0)                                    # SitLie
    s(0)                                    # BEdible            (?!BStatic)
    s(0)                                    # BReadable (==0 -> no ReadText)
    s(0)                                    # BWeapon            (?!BStatic)
    s(0)                                    # CurrentState (==0 -> no States)
    s(0)                                    # BListFlag
    # Res1, Res2: RESOURCE empty (sound/graphics off)
    s("")                                   # InRoomDesc
    s(0)                                    # OnlyWhenNotMoved
obj("amulet", 1, 99)                         # held by nonexistent NPC (99-1=98)
obj("coin",   2, -1)                         # in nonexistent container (-1)
obj("ring",   1, 1)                          # held by the one real NPC (1-1=0)

# ---- TASKS: count = 0 ----
s(0)

# ---- EVENTS: count = 0 ----
s(0)

# ---- NPCS: count = 1 ----
s(1)
s("guard")                                  # Name
s("a")                                      # Prefix
s(0)                                        # V$Alias count
s("A test guard.")                           # Descr
s(1)                                        # StartRoom (1 = room 0)
s("")                                       # AltText
s(0)                                        # Task
s(0)                                        # V<TOPIC> count
s(0)                                        # V<WALK> count
s(0)                                        # BShowEnterExit (-> no Enter/Exit)
s("A guard stands here.")                    # InRoomText
s(0)                                        # Gender
# [4]<RESOURCE>Res: four RESOURCE blocks, all empty (sound/graphics off)
# BattleSystem off -> no NPC_BATTLE block

# ---- tail ----
s(0)                                        # RoomGroups count
s(0)                                        # Synonyms count
s(0)                                        # Variables count
s(0)                                        # ALRs count
s(0)                                        # CustomFont
s("2026")                                   # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = SIG + HEADER_EXTRA + zlib.compress(body, 9)
path = sys.argv[1] if len(sys.argv) > 1 else "badparent_test.taf"
open(path, "wb").write(out)
print(f"wrote {path}: {len(out)} bytes ({len(L)} lines)")
