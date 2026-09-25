#!/usr/bin/env python3
"""ADRIFT 4.0 probe: does an object WORN by an NPC phantom-weigh object
#(Parent-1) the way an object HELD by an NPC measurably does?  And which
moves rewrite that stale link?

Background (scgamest.cpp gs_create, memory adrift4-npc-held-phantom-weight-seed):
run400's loader keeps the NPC index in the object's [46] container field for
an NPC-held object (490749: [26]=-200, [46]=Parent-1), and the recursive
weigh (447680) counts every object whose [46] equals the weighed object's
INDEX as a child, with no position test.  So an object HELD by NPC n weighs
into object n -- wonderland's ethereal knife (94 > 90).  The listing gives the
NPC-WORN branch the same rewrite (490767/490797: [26]=-300, [46]=Parent-1).

MEASURED against run400 (seed 1234, 2026-09-25), all 16 cells now identical
in scarier:
  - WORN by an NPC phantoms exactly like HELD (p4WORNNPC: coin refused;
    p4WORNNPC0: coin refused), whether the NPC starts nowhere (H), the cloak
    is not wearable (N), or a built-in `give`/`wear` ran first (G/W).
  - A TASK move rewrites the moved object's [46] in every arm (Proc_19_10):
    &HFF for room/hidden/roomgroup/player-held/player-worn/same-room, the
    container/surface for into/onto, the NPC index for NPC-held/worn.  So
    S/R/P (cloak to hidden, cloak to Alpha, feather to hidden) take the
    child out of the coin's weight, and K (cloak handed to NPC 0) moves its
    phantom from the coin (object 1) to the pebble (object 0).  Moving the
    PARENT by task (F/M/T) or by built-in drop (G) changes nothing.
  - `count` never re-weighs: after S/R/P it still says 83.  Read the effect
    off `drop coin` (debits the fresh weigh) and `take coin` instead.
goldilocks' crown therefore DOES weigh into the package; the earlier "does
not" reading was the crown's missing 9 offset by the bottle's stale 9 that
scarier kept after task 54 `water bean` moved it to the garden.

Two shapes, packed as 4.00 (taftool.py pack against a 4.00 template):

  p4WORNNPC   rooms Alpha (start) -> east -> Bravo (three NPCs, never visited)
              objects  0 pebble   in Alpha, decoy: every in-room object writes
                                  Parent 0, so they are all its children
                       1 coin     in Alpha, wt 1      <- worn-chain target
                       2 ring     in Alpha, wt 1      <- held-chain target
                       3 feather  in Alpha, wt 1      <- no chain, control
                       4 cloak    WORN by NPC 1 (Bob),  Parent 2, wt 81
                       5 anvil    HELD by NPC 2 (Carl), Parent 3, wt 81
              NPCs     0 Alice (nothing), 1 Bob, 2 Carl, all in Bravo
              MaxWt 101 -> 10 * 3^1 = 30.
                take feather   taken (1 <= 30)                 control
                take ring      "too heavy ... at the moment"   HELD, 1+81 > 30
                take coin      refused: worn phantoms too.

  p4WORNNPC0  goldilocks' exact shape: the WORN chain points at object 0.
              objects  0 coin     in Alpha, wt 1      <- worn-chain target
                       1 feather  in Alpha, wt 1, Parent 0 -> child of coin
                       2 cloak    WORN by NPC 0 (Alice), Parent 1, wt 81
              NPCs     0 Alice in Bravo
                take feather   taken;  take coin  refused (1+1+81 > 30).

The third argument picks a task-action variant (see ACTS below); the fourth
overrides MaxWt (103 -> 270 lets the coin be taken so `count`/`drop` read the
weigh back).  `count` reads the running totals where 4.0 prints them.

Usage:
    python3 make_400_wornnpcprobe.py p4WORNNPC.plain  [0 [VARIANT [MAXWT]]]
    python3 taftool.py pack p4WORNNPC.plain p4TAKE.taf p4WORNNPC.taf
"""
import sys

SEP = "\xbd\xd0"
ZERO = len(sys.argv) > 2 and sys.argv[2] == "0"
ACTVAR = sys.argv[3] if len(sys.argv) > 3 else ""
MAXWT = int(sys.argv[4]) if len(sys.argv) > 4 else 101   # 103 = 270: lets the coin be taken despite the phantom

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("A synthetic 4.0 NPC-worn phantom-weight probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s(("Probe 4WORNNPC0" if ZERO else "Probe 4WORNNPC") + ACTVAR)
s("SCARE probe")
s("NO IDEA.")            # DontUnderstand
s(1)                     # Perspective: second person
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player"); s(0); s("A test subject.")
s(0)                     # Task
s(0); s(0); s(0)         # Position, ParentObject, PlayerGender
s(104); s(MAXWT)         # MaxSize 10*3^4 = 810, MaxWt 10*3^(MAXWT-100)
s(0)                     # EightPointCompass
s(0); s(0); s(0)         # NoDebug, NoScoreNotify, NoMap
s(0); s(0); s(0)         # NoAutoComplete, NoControlPanel, NoMouse
s(0); s(0)               # Sound, Graphics
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0)                 # Alts
    s(0)                 # HideOnMap

ROOMS = 2
s(ROOMS)
room("Alpha", "The first room.",  {1: 2})
room("Bravo", "The second room.", {3: 1})

POS_HELD = 1
POS_WORN = 4 + ROOMS
def POS_ROOM(r): return 4 + r

def obj(prefix, short, desc, position, parent=0, sizeweight=10, wearable=0):
    s(prefix); s(short); s(0)       # Prefix Short V$Alias count
    s(0)                            # Static
    s(desc)
    s(position)                     # InitialPosition
    s(0); s(0); s("")               # Task TaskNotDone AltDesc
    s(0); s(0); s(0)                # Container Surface Capacity
    s(wearable); s(sizeweight); s(parent)   # Wearable SizeWeight Parent
    s(0)                            # Openable
    s(0)                            # SitLie
    s(0); s(0)                      # Edible Readable
    s(0)                            # Weapon
    s(0)                            # CurrentState
    s(0)                            # ListFlag
    s(""); s(0)                     # InRoomDesc OnlyWhenNotMoved

if ZERO:
    s(3)
    obj("a", "coin",    "A copper coin.",  POS_ROOM(0))
    obj("a", "feather", "A white feather.", POS_ROOM(0))
    obj("a", "cloak",   "A leaden cloak.", POS_WORN, parent=1, sizeweight=4, wearable=1)
else:
    s(6)
    obj("a", "pebble",  "A small pebble.", POS_ROOM(0))
    obj("a", "coin",    "A copper coin.",  POS_ROOM(0))
    obj("a", "ring",    "A brass ring.",   POS_ROOM(0))
    obj("a", "feather", "A white feather.", POS_ROOM(0))
    obj("a", "cloak",   "A leaden cloak.", POS_WORN, parent=2, sizeweight=4, wearable=1)
    obj("an", "anvil",  "An iron anvil.",  POS_HELD, parent=3, sizeweight=4)

# TASKS -- one control, plus (ACTVAR) one task whose actions are the ones
# under test.  Raw action tuples: (0, Var1, Var2, Var3) moves an object --
# Var1 0 = all held, 1 = all worn, d+2 = dynamic object d; Var2 0 = to room,
# Var3 = 1-based room, 0 = hidden.  (1, 0, 5, 0) = the player lies on the
# floor (goldilocks' beanstalk task).  (4, n) = score change.
ACTS = {
    "A": [("boom",    [(0, 0, 0, 2)])],                       # all held -> Bravo
    "B": [("hide",    [(0, 3, 0, 0)])],                       # feather -> hidden
    "D": [("lie",     [(1, 0, 5, 0)])],                       # lie on the floor
    "E": [("boomall", [(0, 0, 0, 2), (1, 0, 5, 0), (4, 3)])], # goldilocks' trio
    # Parent-held variants (run with MAXWT 103): the coin is held first.
    "F": [("boom",    [(0, 0, 0, 1)])],                       # all held -> Alpha
    "M": [("movecoin",[(0, 3, 0, 1)])],                       # coin -> Alpha
    "T": [("boomall", [(0, 0, 0, 1), (1, 0, 5, 0), (4, 3)])], # trio, room Alpha
    # Child-moved variants (ZERO shape, MAXWT 103): the phantom object itself
    # is moved by a task action.  Var1 = dynamic index + 3.
    "S": [("vanish",  [(0, 5, 0, 0)])],                       # cloak -> hidden
    "R": [("shed",    [(0, 5, 0, 1)])],                       # cloak -> Alpha
    "P": [("pluck",   [(0, 4, 0, 0)])],                       # feather -> hidden
    # Non-ZERO shape: cloak (obj 4, worn by Bob) -> held by Alice (NPC 0).
    "K": [("hand",    [(0, 7, 4, 2)])],
}.get(ACTVAR, [])

def task(cmd, text, actions):
    s(1); s(cmd)
    s(text)                  # CompleteText
    s(""); s(""); s("")      # ReverseMessage RepeatText AdditionalMessage
    s(0)                     # ShowRoomDesc
    s(1)                     # Repeatable
    s(0)                     # Reversible
    s(0)                     # V$ReverseCommand count
    s(3)                     # Where: type 3 = all rooms
    s("")                    # Question
    s(0)                     # Restrictions
    s(len(actions))          # Actions
    for a in actions:
        for f in a: s(f)
    s("")                    # RestrMask

s(1 + len(ACTS))
task("probe", "PROBE OK.", [])
for cmd, acts in ACTS:
    task(cmd, "%s FIRED." % cmd.upper(), acts)

# EVENTS -- none.
s(0)

# NPCS -- no walks; each one sits in Bravo.
def npc(name, descr, startroom, inroom):
    s(name); s(""); s(0)     # Name Prefix V$Alias count
    s(descr)
    s(startroom)             # StartRoom (1-based)
    s("")                    # AltText
    s(0)                     # Task
    s(0)                     # Topics
    s(0)                     # Walks
    s(1)                     # ShowEnterExit
    s("wanders in"); s("wanders off")
    s(inroom)                # InRoomText
    s(0)                     # Gender

if ZERO:
    s(1)
    npc("Alice", "A quiet woman.", 2, "Alice is here.")
else:
    s(3)
    npc("Alice", "A quiet woman.", 2, "Alice is here.")
    npc("Bob",   "A quiet man.",   2, "Bob is here.")
    npc("Carl",  "A quiet man.",   2, "Carl is here.")

s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WORNNPC.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
