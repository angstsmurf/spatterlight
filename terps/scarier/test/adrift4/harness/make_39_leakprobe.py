#!/usr/bin/env python3
"""ADRIFT 3.9 probe: does run390 keep run400's LEAKY carried size total?

Scarier maintains one running size/weight pair for every game that has those
axes -- 3.9 and 4.0 alike -- and the rule it maintains them by was transcribed
from run400's Proc_21_54 and confirmed command by command in run400 only (see
the note above gs_carried_track() in scgamest.cpp).  Applying it to 3.9 was an
inference from "3.9 has the same two axes".  The pre-3.9 half of the same
question turned out NOT to follow the inference -- 3.7/3.8 recompute -- so the
3.9 half is worth measuring rather than assuming.

The 4.0 rule leaks in two distinct places, and this probe reads both directly:
`count` prints the totals, so no refusal arithmetic is needed.

  contents leak.  Dropping a carried container refunds only the container's
  OWN size; whatever is inside keeps its size on the total forever.  Weight
  refunds correctly, because the weight arm recurses and the size arm does not.

  wear double-debit.  Wearing subtracts the size once ("newly worn") and
  dropping the worn object subtracts it again ("no longer possessed"), with no
  credit in between, so a wear/drop cycle drives the total DOWN past zero.

Sizes and weights are indices into the base-3 scale (SizeWeight = size*10 +
weight), chosen so every leak shows up as a value that names its own culprit:

    bag   SizeWeight 21 -> size  9, weight 3, container, open, capacity 45
    rock  SizeWeight 31 -> size 27, weight 3
    cape  SizeWeight 22 -> size  9, weight 9, wearable

MaxSize = MaxWt = 102 -> a limit of 90 each, comfortably above the 45/15 the
probe ever holds, so nothing is ever refused and the readout stays clean.

Predicted `count` size, leaky (run400's rule) vs recomputed:

    command             leaky        recomputed
    take bag            9            9
    take rock           36           36
    put rock in bag     36           36
    drop bag            27  <-- the rock's own 27, orphaned      0
    take cape           36           9
    wear cape           27           9 (or 0 if worn is free)
    drop cape           18  <-- 9 BELOW the pre-cape 27          0

Weight should return to 0 after each drop under either model; it is printed
alongside as a control that the totals are moving at all.

A third mode, `wt`, leaves the size axis alone and squeezes the WEIGHT one
instead, to read back the two refusal wordings rather than the totals:

    brick  SizeWeight 03 -> weight 27; two of them exceed a MaxWt of 30 while
                            either alone fits ("portable" in SCARE's terms)
    lump   SizeWeight 04 -> weight 81, over the limit on its own

Usage: python3 make_39_leakprobe.py [leak|lim|wt] [out.taf]
Session: sh runner_savetranscript.sh p39leak.taf cmdfile_p39leak.txt run390.exe
"""
import sys

WHICH = sys.argv[1] if len(sys.argv) > 1 else "leak"
if WHICH not in ("leak", "lim", "wt"):
    sys.exit("usage: make_39_leakprobe.py [leak|lim|wt] [out.taf]")

# p39leak reads the totals straight off `count`, so nothing must ever be
# refused: MaxSize 102 -> 10 * 3^2 = 90, far above the 45 it ever holds.
# p39lim asks the same two questions of the CAPACITY CHECK instead of the
# report, in case the Runner prints one number and checks another, so its
# limit is tuned to sit between the leaky and recomputed totals:
# MaxSize 101 -> 10 * 3^1 = 30.
# p39wt asks nothing of the size axis -- MaxSize 104 -> 10 * 3^4 = 810 -- and
# pins MaxWt at 101 -> 30 instead, so the weight refusal is the one that
# speaks.
MAXSIZE = {"leak": 102, "lim": 101, "wt": 104}[WHICH]
MAXWT = 101 if WHICH == "wt" else 102

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 carried-size leak probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39LEAK")        # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective (second person)
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem (0 -> no <BATTLE> block below)
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test subject.")     # PlayerDesc
s(0)                     # Task (0 -> no AltDesc)
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(MAXSIZE)               # MaxSize
s(MAXWT)                 # MaxWt    -> 10 * 3^2 = 90
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
# IntroRes/WinRes contribute no lines with sound and graphics off;
# FStatusBox/EStatusBoxText are absent in 3.9.
s(3)                     # SizeMultiple   -- what the editor always writes.
s(3)                     # WeightMultiple    A 0 here would flatten the scale.

# ROOMS
s(1)
s("Test Arena"); s("A bare arena."); s("")
for _ in range(8): s(0)  # exits (compass off -> 8)
s(""); s(0); s(""); s(0); s(0); s(""); s(0)
s(0)                     # HideOnMap (NoMap == 0)

# OBJECTS
s({"leak": 4, "lim": 5, "wt": 4}[WHICH])

def obj(prefix, short, desc, sizeweight, container=0, capacity=0, openable=0,
        wearable=0):
    s(prefix); s(short); s("")      # Prefix Short [1]Alias
    s(0)                            # Static
    s(desc)
    s(4)                            # InitialPosition: 4 + room 0 = in the arena
    s(0); s(0); s("")               # Task TaskNotDone AltDesc
    s(container); s(0); s(capacity) # Container Surface Capacity
    s(wearable); s(sizeweight); s(0)# Wearable SizeWeight Parent
    s(openable)                     # Openable (on-disk 6 <-> internal 5 = OPEN)
    s(0); s(0); s(0); s(0)          # SitLie Edible Readable Weapon
    # ZCurrentState/FListFlag/Res1/Res2/EInRoomDesc/ZOnlyWhenNotMoved: no lines,
    # and BattleSystem is off, so no OBJ_BATTLE triple either.

# Object 0 is a decoy that is never picked up, and it is here to keep the
# WEIGHT axis readable.  Every in-room object writes Parent 0 -- that is what
# the editor emits and what real games carry -- and 3.9/4.0 seed the Runner's
# [2E] parent field from it verbatim.  run400's weigh routine matches children
# on that raw field with no container or position test, so with a real object
# at index 0 the whole probe set counts as its children and taking it weighs
# everything at once (the Goldilocks phantom, see obj_weigh()).  Parking a
# never-touched pebble there makes every phantom child a child of something we
# never weigh, and leaves the three objects under test with no children but
# the ones the session genuinely puts inside them.
obj("a", "pebble", "A small pebble.", 0)

# Capacity 52 -> 5 * 3^2 = 45, which must exceed the rock's size of 27.  The
# bag needs a nonzero size of its own as well: run390 refuses "put X in Y" for
# a size-0 container (see make_39_heldprobe.py).
if WHICH == "leak":
    obj("a", "bag",  "A canvas bag.", 21, container=1, capacity=52, openable=6)
    obj("a", "rock", "A heavy rock.", 31)
    obj("a", "cape", "A red cape.",   22, wearable=1)
elif WHICH == "lim":
    obj("a", "bag",  "A canvas bag.", 21, container=1, capacity=52, openable=6)
    obj("a", "rock", "A heavy rock.", 21)
    obj("a", "big",  "A big crate.",  31)
    obj("a", "cape", "A red cape.",   21, wearable=1)
else:
    obj("a", "brick", "A clay brick.",   3)
    obj("a", "block", "Another brick.",  3)
    obj("a", "lump",  "A lump of lead.", 4)

# TASKS / EVENTS / NPCS
s(0); s(0); s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate
s("    Wild    ")        # sPassword (the passwordless form)

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[2] if len(sys.argv) > 2 else "p39%s.taf" % WHICH
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
