#!/usr/bin/env python3
"""Container size/capacity matrix probe, in BOTH ADRIFT 3.9 and 4.0 flavours.

The question this settles: what does an object's `Capacity` field mean?

    Scarier (sclibrar.cpp lib_put_in_backend) reads it as
        max object size = 3 ^ (Capacity % 10)
        max object COUNT = Capacity / 10
    and refuses a `put` once that many objects are already inside.

    run400's mdlSpreadTheLoad.Sub_20_46 (00045630) instead computes
        remaining = obj(container).[28] - sum of size of direct contents
    and the put gate at 00066293 refuses when object.size > remaining --
    i.e. Capacity is a VOLUME BUDGET, not an object count.

One room, everything on the floor, MaxSize/MaxWt cranked so the player's own
carry limits never interfere.  Four containers, none openable (so open/close
state can never be the reason for a refusal):

    sack   Capacity 12   1 x medium
    chest  Capacity 52   5 x medium
    tin    Capacity 10   1 x tiny
    pot    Capacity 12   1 x medium

and loose objects at four size indices:

    pebble bead coin seed nut   SizeWeight 0   -> size 3^0 =  1
    cup                         SizeWeight 10  -> size 3^1 =  3
    book brick plank            SizeWeight 20  -> size 3^2 =  9
    log                         SizeWeight 30  -> size 3^3 = 27

Discriminating session (see sizeprobe.txt written alongside):

    put pebble in sack      both models accept
    put bead in sack        COUNT refuses (1 already in), VOLUME accepts (1+1 <= 9)
    put coin in sack        VOLUME accepts
    put seed in sack        VOLUME accepts
    x sack
    put cup in tin          expect "too big": size 3 > max object size 3^0 = 1
    put nut in tin          accepted by both
    put book in chest       accepted by both (9 <= 45, 9 <= 9)
    put log in chest        expect "too big": 27 > 9
    put brick in pot        accepted by both
    put plank in pot        refused by both -- but the MESSAGE differs:
                            COUNT/Scarier says nothing about fit, VOLUME/run400
                            says "... can't fit inside ... at the moment."
    i

The two Runner refusal strings are themselves the diagnostic:
    "X is too big to fit inside Y."          per-object max-size gate (0006620E)
    "X can't fit inside Y at the moment."    capacity/volume gate    (00066293)

Usage:
    python3 make_sizeprobe.py 39  psize39.taf      # self-obfuscating, run390-ready
    python3 make_sizeprobe.py 400 psize400.plain   # then taftool.py pack ...
"""
import sys

version = sys.argv[1] if len(sys.argv) > 1 else "400"
if version not in ("39", "390", "400"):
    sys.exit("usage: make_sizeprobe.py [39|400] [out] [maxsize] [maxwt]")
v39 = version in ("39", "390")

# Player carry limits.  The packed form is count*10 + size-index, so 102 is the
# ADRIFT editor default ("10 medium objects") and 994 is the maximum the editor
# will emit.  Overridable because the Runner's decode of these is itself under
# investigation -- see RUNNER_TESTS_TODO.md.
MAXSIZE = int(sys.argv[3]) if len(sys.argv) > 3 else 994
MAXWT = int(sys.argv[4]) if len(sys.argv) > 4 else MAXSIZE

PARENT = -1                 # ADRIFTMaze and every other editor-made
                            # file writes -1 for "no parent object"

# The two globals sctafpar still calls iUnk1/iUnk2 are the SIZE and WEIGHT
# scale bases: every decoded dimension is base ** index, and the player limits
# are tens(value) * base ** units(value).  Editor-made files always write 3.
# Writing 0 here (as this generator used to) makes every index-0 object decode
# to 0**0 = 1 and everything else to 0, which is what made the earlier carry
# sweeps nonsense -- run400's debugger showed MaxSize 102 decoding to 0.
# Proved live: iUnk1=2, iUnk2=5, MaxSize=MaxWt=102 -> Runner shows Max 40 / 250,
# and taking one SizeWeight-22 plus one SizeWeight-12 object gives 6 / 50.
SIZE_MULTIPLE = int(sys.argv[7]) if len(sys.argv) > 7 else 3
WEIGHT_MULTIPLE = int(sys.argv[8]) if len(sys.argv) > 8 else SIZE_MULTIPLE

SEP = "\xbd\xd0"            # v4.0 multiline separator (sctafpar V400_SEPARATOR)

L = []
def s(x):  L.append(str(x))
def ml(x):
    """M-multiline field: 3.9 terminates with '**', 4.0 with the separator."""
    L.append(x)
    L.append("**" if v39 else SEP)

# ---------------------------------------------------------------- HEADER
ml("Container size/capacity matrix probe.")
s(0)                          # StartRoom (0-based)
ml("You have won.")

# ---------------------------------------------------------------- GLOBAL
s("Size Probe %s" % ("390" if v39 else "400"))
s("SCARE probe")
s("I don't understand.")      # DontUnderstand
s(2)                          # Perspective
s(0)                          # ShowExits
s(0)                          # WaitTurns
s(1)                          # DispFirstRoom
s(0)                          # BattleSystem (off -> no BATTLE block)
s(0)                          # MaxScore
s("Player")                   # PlayerName
s(0)                          # PromptName
s("A test subject.")          # PlayerDesc
s(0)                          # Task (0 -> no AltDesc)
s(0)                          # Position
s(0)                          # ParentObject
s(0)                          # PlayerGender
s(MAXSIZE)                    # MaxSize
s(MAXWT)                      # MaxWt
s(0)                          # EightPointCompass
s(0)                          # bNoDebug
s(0)                          # NoScoreNotify
s(0)                          # NoMap
s(0)                          # bNoAutoComplete
s(0)                          # bNoControlPanel
s(0)                          # bNoMouse
s(0)                          # Sound
s(0)                          # Graphics
# IntroRes, WinRes: nothing (sound+graphics off)
if v39:
    # FStatusBox / EStatusBoxText consume no lines in 3.9
    s(SIZE_MULTIPLE)          # iUnk1 = size scale base
    s(WEIGHT_MULTIPLE)        # iUnk2 = weight scale base
    # FEmbedded: no line
else:
    s(0)                      # StatusBox
    s("")                     # StatusBoxText
    s(SIZE_MULTIPLE)          # iUnk1 = size scale base
    s(WEIGHT_MULTIPLE)        # iUnk2 = weight scale base
    s(1)                      # Embedded

# ---------------------------------------------------------------- ROOMS
s(1)
s("Test Room")                # Short
s("A bare room.")             # Long
if v39:
    s("")                     # LastDesc
for _ in range(8):
    s(0)                      # 8 exits, all absent (compass off)
if v39:
    s("")                     # AddDesc1
    s(0)                      # Task1
    s("")                     # AddDesc2
    s(0)                      # Task2
    s(0)                      # Obj
    s("")                     # AltDesc
    s(0)                      # TypeHideObjects
    # Res/LastRes/Task1Res/Task2Res/AltRes: nothing
    s(0)                      # HideOnMap
else:
    # RESOURCE Res: nothing
    s(0)                      # Alts count
    s(0)                      # HideOnMap

# ---------------------------------------------------------------- OBJECTS
# (name, is_container, Capacity, SizeWeight)
#
# Mode "diag" is the calibration ladder: one SizeWeight per object, with an
# identical container first and last so a first-object effect shows up as the
# two behaving differently.
DIAG = [
    ("box",    True,  12, 22),
    ("ant",    False,  0,  0),
    ("bat",    False,  0,  1),
    ("cat",    False,  0,  2),
    ("dog",    False,  0, 10),
    ("eel",    False,  0, 11),
    ("fox",    False,  0, 20),
    ("gnu",    False,  0, 22),
    ("hen",    False,  0, 30),
    ("imp",    False,  0, 44),
    ("crate",  True,  12, 22),
]

# Mode "cal" walks each index digit from 0 to 9 with the other digit pinned at
# the editor default 2, so the pass/fail boundary of each carry gate is read
# straight off the transcript.  Object 0 is a plain SizeWeight-22 object, the
# control for the "object index 0 misbehaves" effect the DIAG ladder showed
# (its container `box` at index 0 was refused, the identical `crate` at index
# 10 was taken).
CAL = ([("aaa", False, 0, 22)]
       + [("s%d" % i, False, 0, i * 10 + 2) for i in range(10)]
       + [("w%d" % i, False, 0, 20 + i) for i in range(10)])

OBJECTS = [
    ("sack",   True,  12, 22),   # budget 1 x medium
    ("chest",  True,  52, 22),   # budget 5 x medium
    ("tin",    True,  10, 22),   # budget 1 x tiny
    ("pot",    True,  12, 22),   # budget 1 x medium
    ("pebble", False,  0,  0),   # size 1
    ("bead",   False,  0,  0),
    ("coin",   False,  0,  0),
    ("seed",   False,  0,  0),
    ("nut",    False,  0,  0),
    ("cup",    False,  0, 10),   # size 3
    ("book",   False,  0, 20),   # size 9
    ("brick",  False,  0, 20),
    ("plank",  False,  0, 20),
    ("log",    False,  0, 30),   # size 27
]

# Mode "cont" measures the container gate the same way: two identical Capacity
# containers, one filled with size-index-1 objects and one with size-index-3
# objects, so the count that fits in each gives the ratio between the decoded
# capacity and the decoded object size.  Object 0 is a never-touched dummy --
# run400 charges every parentless object's weight to object index 0 (see the
# `aaa`/`box` results in RUNNER_TESTS_TODO.md), so index 0 is unusable.
CONT = ([("dummy", False, 0, 22), ("baga", True, 0, 22), ("bagb", True, 0, 22)]
        + [("a%d" % i, False, 0, 11) for i in range(1, 10)]
        + [("b%d" % i, False, 0, 31) for i in range(1, 10)])

# Mode "cap" is the capacity sweep: one container per Capacity value, each with
# its own private pool of twelve identical objects, so a session is just 12
# `put` commands per container and then one `x <container>` per container --
# the contents line names exactly how many fit, and the whole sweep is read off
# a single screenshot.  Pool `e` is size index 4 rather than 2, so k52 measures
# the object-size scale while the rest measure the capacity scale.
CAPCONT = [("k10", 10, 22), ("k11", 11, 22), ("k12", 12, 22),
           ("k22", 22, 22), ("k52", 52, 42), ("k102", 102, 22)]
CAP = ([("dummy", False, 0, 22)]
       + [(name, True, cap, 22) for name, cap, _ in CAPCONT]
       + [("z1", False, 0, 2)]        # size index 0: the "too big" reproducer
       + [("%s%d" % (chr(ord("a") + n), i), False, 0, sw)
          for n, (_, _, sw) in enumerate(CAPCONT) for i in range(1, 13)])

# Mode "carry" is the player-limit sweep, run at the corpus-default player
# MaxSize/MaxWt of 102 ("10 normal-sized objects").  Twenty objects at each of
# the four non-zero size indices, so a `take` series shows exactly where the
# limit bites, plus three objects at size index 0 and three at weight index 0
# (the two values the Runner treats as unliftable), and two containers at the
# capacities the earlier sweep did not reach.
CARRY = ([("dummy", False, 0, 22), ("k0", True, 0, 22), ("k2", True, 2, 22)]
         + [("t%d" % i, False, 0, 2) for i in range(1, 4)]
         + [("u%d" % i, False, 0, 20) for i in range(1, 4)]
         + [("p%d" % i, False, 0, 12) for i in range(1, 21)]
         + [("q%d" % i, False, 0, 22) for i in range(1, 21)]
         + [("r%d" % i, False, 0, 32) for i in range(1, 21)]
         + [("h%d" % i, False, 0, 42) for i in range(1, 21)])

# Mode "cap2" is the sweep that actually discriminates the two capacity models,
# now that the scale bases above make the probe's arithmetic real.  Scarier
# reads Capacity as (count = tens, max object size = 3 ** units) and refuses the
# count+1'th object whatever its size; run400's mdlSpreadTheLoad.Sub_20_46
# instead keeps a VOLUME budget of tens * 3 ** units and subtracts each direct
# child's size.  The two agree whenever the contents are all "normal sized"
# (size 9), which is why every earlier sweep came out a tie -- so each container
# here is fed a pool whose member size differs from the container's own units
# digit:
#
#   container  Capacity  count model      volume model
#   c12t       12        1 x a (size 1)   9 x a
#   c52t       52        5 x b (size 1)   45 x b  (pool of 12 -> all 12)
#   c13t       13        1 x c (size 1)   27 x c  (pool of 12 -> all 12)
#   c22x       22        2 x f (size 3)   6 x f
#   c12m       12        1 x d (size 9)   1 x d           <- the tie, a control
#   c12b       12        e1 (size 27) refused by both, but with different text:
#                        "too big to fit inside" (per-object max size) vs
#                        "can't fit inside ... at the moment" (budget)
#
# VERDICT (run400, 2026-08-03): the volume model, in every row.  c12t took
# exactly nine, c52t and c13t took all twelve, c22x took exactly six, c12m took
# its one.  c12b answered "too big to fit inside"; a follow-up put of the same
# size-27 object into c52t (total 45, 21 spent) answered "can't fit inside ...
# at the moment", which pins the "too big" gate to the container's TOTAL
# volume and not to the units digit -- 27 is far more than c52t's 3^2 = 9 yet
# was never called too big.
CAP2CONT = [("c12t", 12, "a", 2), ("c52t", 52, "b", 2), ("c13t", 13, "c", 2),
            ("c22x", 22, "f", 12), ("c12m", 12, "d", 22), ("c12b", 12, "e", 32)]
CAP2 = ([("dummy", False, 0, 22)]
        + [(name, True, cap, 22) for name, cap, _, _ in CAP2CONT]
        + [("%s%d" % (pool, i), False, 0, sw)
           for _, _, pool, sw in CAP2CONT for i in range(1, 13)])

# "cap3" -- the two questions cap2 left open.
#
#   z02   Capacity 2   tens 0, so a volume of 0 x 3^2 = 0.  Fifteen containers
#                      in the walkthrough corpus have a tens digit of 0, and
#                      the two models disagree about what they say: a volume of
#                      0 makes every object "too big to fit inside", whereas an
#                      object count of 0 makes them all "can't fit ... at the
#                      moment".
#   z20   Capacity 20  volume 2 x 3^0 = 2; takes two of a size-1 pool and
#                      refuses the third.  A control for a units digit of 0.
#   n22   Capacity 22  volume 18, and the nesting question.  m12 (a size-9
#                      container of volume 9) is filled with nine size-1
#                      objects and then put inside n22, leaving 18 - 9 = 9 if
#                      the outer container is charged only for m12's own size,
#                      or 0 if it is charged for m12's contents too.  A size-9
#                      object put in afterwards therefore either goes in or is
#                      refused, and that is the whole answer.  (Weight is
#                      summed recursively, so the expectation is that size is
#                      not -- but expectation is not evidence.)
CAP3 = ([("dummy", False, 0, 22),
         ("z02", True, 2, 22), ("z20", True, 20, 22),
         ("n22", True, 22, 22), ("m12", True, 12, 22),
         ("v1", False, 0, 22)]
        + [("z%d" % i, False, 0, 2) for i in range(1, 4)]
        + [("y%d" % i, False, 0, 2) for i in range(1, 4)]
        + [("w%d" % i, False, 0, 2) for i in range(1, 10)])

if len(sys.argv) > 5:
    if sys.argv[5] == "cont":
        cap = int(sys.argv[6]) if len(sys.argv) > 6 else 52
        CONT[1] = ("baga", True, cap, 22)
        CONT[2] = ("bagb", True, cap, 22)
    OBJECTS = {"diag": DIAG, "cal": CAL, "cont": CONT, "cap": CAP,
               "cap2": CAP2, "cap3": CAP3,
               "carry": CARRY}.get(sys.argv[5], OBJECTS)

s(len(OBJECTS))
for name, container, capacity, sizeweight in OBJECTS:
    s("a")                    # Prefix
    s(name)                   # Short
    if v39:
        s("")                 # [1] Alias
    else:
        s(0)                  # V$Alias count
    s(0)                      # Static (all dynamic)
    s("A probe %s." % name)   # Description
    s(4)                      # InitialPosition: 4 + room -> room 0
    s(0)                      # Task
    s(0)                      # TaskNotDone
    s("")                     # AltDesc
    s(1 if container else 0)  # Container
    s(0)                      # Surface
    s(capacity)               # Capacity
    s(0)                      # Wearable
    s(sizeweight)             # SizeWeight
    s(PARENT)                 # Parent (-1 = none, as the editor writes it; 0
                              # names object 0 and makes the Runner charge it
                              # the whole game's weight -- see RUNNER_TESTS)
    s(0)                      # Openable (0 = not openable, always accessible)
    s(0)                      # SitLie
    s(0)                      # Edible
    s(0)                      # Readable
    s(0)                      # Weapon
    if not v39:
        s(0)                  # CurrentState
        s(0)                  # ListFlag
        # Res1, Res2: nothing
        s("")                 # InRoomDesc
        s(0)                  # OnlyWhenNotMoved
    # 3.9: ZCurrentState FListFlag EInRoomDesc ZOnlyWhenNotMoved read no lines,
    # and battle is off so no OBJ_BATTLE block.

# ---------------------------------------------------------------- rest
s(0)                          # Tasks
s(0)                          # Events
s(0)                          # NPCs
s(0)                          # RoomGroups
s(0)                          # Synonyms
s(0)                          # Variables
s(0)                          # ALRs
s(0)                          # CustomFont
s("2026")                     # CompileDate
if v39:
    s("    Wild    ")         # sPassword: Runner validates Mid(pw, 5, 4)

body = ("\r\n".join(L) + "\r\n").encode("latin-1")

out = sys.argv[2] if len(sys.argv) > 2 else ("psize39.taf" if v39
                                             else "psize400.plain")
if v39:
    # 3.9 .taf = 14-byte signature + body XORed with the VB PRNG stream.
    SIG = bytes([0x3c, 0x42, 0x3f, 0xc9, 0x6a, 0x87,
                 0xc2, 0xcf, 0x94, 0x45, 0x37, 0x61, 0x39, 0xfa])
    state = 0x00a09e86
    def draw():
        global state
        state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
        return (255 * state) // 0x1000000
    for _ in range(14):
        draw()
    open(out, "wb").write(SIG + bytes(b ^ draw() for b in body))
    open(out + ".plain", "wb").write(body)
else:
    # 4.0 body must be packed with a donor for the "Wild" password trailer:
    #   python3 taftool.py pack out.plain <donor.taf> out.taf
    open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
