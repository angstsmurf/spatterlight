#!/usr/bin/env python3
"""The put-ON probe world, in all four ADRIFT file versions.

Every rule the put row has measured so far -- the take-from handler's own
answers (2026-09-10), the put-IN handler's own answers (2026-09-12) and the
3.80/3.70 halves of those (2026-09-12) -- was driven with `put X in Y`.
`put X on Y` went to a different pair of handlers in every Runner
(run390 `insides()` vs `surfaces()`, run400's own split) and not one cell of
it has ever been measured, so scarier's whole put-ON path is unversioned:
`lib_put_on_is_valid()`, `lib_put_on_backend()` and `lib_cmd_put_all_on()`
answer the same way for a 3.70 file as for a 4.00 one.  This builds the
world that settles it.

The world, identical in all four files and deliberately LIT -- the darkness
of `make_39_darkprobe.py` is not wanted here, because the seen flag it gates
would confound every reach cell:

    Lit Room  --n-->  Cave  (--s--> back)

    1 torch   "an", dynamic, in the Lit Room   -- the raw-Prefix all-list cell
    2 lamp    "a",  dynamic, HELD from turn 1  -- a held non-surface
    3 stone   "a",  dynamic, in the Cave       -- a loose non-surface
    4 pebble  "a",  dynamic, in the Cave       -- the implicit-take cell
    5 table   "a",  dynamic SURFACE, in the Cave, capacity 5
    6 coin    "a",  dynamic, ON the table      -- the "already on" cell
    7 box     "a",  dynamic CONTAINER, OPEN, in the Cave, capacity 5
    8 nut     "a",  dynamic, INSIDE the box    -- the reach-into-a-carried-
                                                  container cell
    9 bench   "a",  STATIC SURFACE, in the Cave -- the floor-supporter cell

plus one repeatable, unrestricted `probe` task printing "PROBE OK.", which
must answer at both ends of every feed or the transcript is worthless.

The four schemas, and what differs (the detail is in
`make_38_darkprobe.py` and `make_37_darkprobe.py`, whose divergence lists
this file merges):

  3.90  the baseline written here: BContainer/BSurface as a pair, a literal
        capacity (tens = object count, units = size index), InitialPosition
        4 + room index, and Parent counted 0-based WITHIN the parent's own
        type -- so the coin's Parent 0 is "the first surface" and the nut's
        Parent 0 is "the first container".  On-disk Openable 6 is OPEN.
  3.80  no Variables/ALRs/CustomFont tail, a seven-field GLOBAL, ROOM
        without HideOnMap, one #SurfaceContainer integer (1 container,
        2 surface) in place of the pair, a capacity that the loader
        multiplies by ten and adds two to, a 0..4 burden class for
        SizeWeight, the pre-restriction TASK shape, InitialPosition one
        lower (3 + room index), and a Parent counted over containers AND
        surfaces together -- so the nut's Parent is 1, not 0.
  3.70  the 3.80 shape plus: WinTask in the header, TASK movements of
        6 x 2 ints with no Var3, no trailing BWinGame, and a fixed
        17-string COMMAND block where 3.80 has a synonyms count.
  4.00  the \xbd\xd0 multi-line marker, a V$Alias count, an explicit Key
        after an openable, a $RestrMask on the task, and on-disk Openable 5
        is OPEN.  Positions and per-type Parents are 3.90's.

A hand-authored file is right when `harness/scare` parses it to exact EOF:
`parse_game` reports "unexpected trailing data" the moment the schema and
the bytes disagree.  The .taf files stay untracked, like p39DARK.taf and
p4TFROM.taf; this generator is the artefact.

Usage:
    python3 make_surfprobe.py 390            -> p39SURF.taf
    python3 make_surfprobe.py 380            -> p38SURF.taf
    python3 make_surfprobe.py 370            -> p37SURF.taf
    python3 make_surfprobe.py 400            -> p4SURF.taf.plain, then
        python3 taftool.py pack p4SURF.taf.plain p4TAKE.taf p4SURF.taf
    python3 make_surfprobe.py all            -> all four, 4.00 packed too

Session (from ~/adrift-battle/runner/wine):
    sh fast.sh p39SURF.taf cmdfile_surf1.txt run390.exe
"""
import subprocess
import sys

SEP = "\xbd\xd0"

# The world, once.  Room numbers are 1-based here and translated per version.
LIT, CAVE = 1, 2

# name, prefix, description, where, kind, capacity, openable, static
#   openable: 0 none | 1 open | 2 closed
#   where: ("room", n) | ("held",) | ("in", name) | ("on", name)
#   kind:  "" | "container" | "surface"
OBJECTS = [
    ("torch",  "an", "An unlit torch.",  ("room", LIT),   "",          0, 0, 0),
    ("lamp",   "a",  "A brass lamp.",    ("held",),       "",          0, 0, 0),
    ("stone",  "a",  "A grey stone.",    ("room", CAVE),  "",          0, 0, 0),
    ("pebble", "a",  "",                 ("room", CAVE),  "",          0, 0, 0),
    ("table",  "a",  "A low table.",     ("room", CAVE),  "surface",   5, 0, 0),
    ("coin",   "a",  "A gold coin.",     ("on", "table"), "",          0, 0, 0),
    ("box",    "a",  "A wooden box.",    ("room", CAVE),  "container", 5, 1, 0),
    ("nut",    "a",  "A brass nut.",     ("in", "box"),   "",          0, 0, 0),
    ("bench",  "a",  "A stone bench.",   ("room", CAVE),  "surface",   5, 0, 1),
]
NAMES = [o[0] for o in OBJECTS]
# SitLie per object name (bit 0 sit/stand on, bit 1 lie on); others 0.
SITLIE = {}
# Object names that are wearable (dynamic objects only).
WEARABLE = set()
# Object names dobattle will accept as a weapon (dynamic objects only).
WEAPONS = set()
# Tasks: (command, CompleteText) pairs, all repeatable, unrestricted, Where =
# all rooms.  The first must stay `probe` -> "PROBE OK.".
TASKS = [("probe", "PROBE OK.")]
# NPCs: (name, prefix, alias, description, room, in-room text, topics,
# gender), topics a list of (subject, reply).  No walks.  3.7/3.8 store no
# gender; 3.9 does.
NPCS = []

# ---------------------------------------------------------------- the battle
# The Battle System is 3.90 and 4.00 only -- 3.7/3.8 carry no such flag and
# no battle blocks at all, so BATTLE is refused for them.  Set it and the
# GLOBAL grows a <BATTLE> block after MaxWt, every OBJECT an <OBJ_BATTLE>
# and every NPC an <NPC_BATTLE>.  3.9 stores one value per attribute and
# knows only Stamina, Strength and Defence; 4.0 stores each as a [lo, hi]
# pair rolled at load (written here with lo = hi, so the roll is a constant)
# and adds the Accuracy and Agility pairs, a Recovery and a StaminaTask.
BATTLE = 0
# The player's attributes.
BATTLE_PLAYER = dict(stamina=9999, strength=1, defence=0, accuracy=1,
                     agility=1, recovery=0)
# Per NPC name; anything left out takes BATTLE_NPC.  Attitude 0 is neutral
# (it never strikes back), 1 ally, 2 enemy.
BATTLE_NPC = dict(attitude=0, stamina=9999, strength=1, defence=0,
                  accuracy=1, agility=1, speed=1, killed_task=0, recovery=0,
                  stamina_task=0)
BATTLE_NPCS = {}
# Per object name; anything left out takes BATTLE_OBJECT.  `hit` is the
# weapon's hit value and `protection` a worn object's armour.
BATTLE_OBJECT = dict(protection=0, hit=0, method=0, accuracy=0)
BATTLE_OBJECTS = {}


def battle_npc(name):
    merged = dict(BATTLE_NPC)
    merged.update(BATTLE_NPCS.get(name, {}))
    return merged


def battle_object(name):
    merged = dict(BATTLE_OBJECT)
    merged.update(BATTLE_OBJECTS.get(name, {}))
    return merged


def type_index(name, kinds):
    """0-based index of `name` among the objects whose kind is in `kinds`."""
    count = 0
    for o in OBJECTS:
        if o[0] == name:
            return count
        if o[4] in kinds:
            count += 1
    raise KeyError(name)


def position_parent(where, version):
    """(InitialPosition, Parent) for one object, in `version`'s numbering."""
    kind, *rest = where
    if kind == "held":
        return 1, 0
    if kind == "room":
        base = 3 if version < 390 else 4
        return base + rest[0] - 1, 0
    # In or on.  3.7/3.8 use one position for both and index the parent over
    # containers and surfaces together; 3.9/4.0 split the position and index
    # within the parent's own type.
    if version < 390:
        return 2, type_index(rest[0], ("container", "surface"))
    if kind == "in":
        return 2, type_index(rest[0], ("container",))
    return 3, type_index(rest[0], ("surface",))


def build(version):
    if BATTLE and version < 390:
        raise SystemExit("the Battle System does not exist before 3.90")
    L = []

    def s(x):
        L.append(str(x))

    def ml(x):
        if version >= 400:
            L.append(x)
            L.append(SEP)
        else:
            L.append(x)
            L.append("**")

    # ---------------------------------------------------------------- HEADER
    ml("A synthetic put-ON probe.")
    s(0)                              # StartRoom
    ml("You have won.")
    if version == 370:
        s(-1)                         # WinTask: -1 is "no task wins this game"

    # ---------------------------------------------------------------- GLOBAL
    s("Probe %dSURF" % version)
    s("SCARE probe")
    if version < 390:
        s(10)                         # MaxCarried (also the burden limit)
    s("I don't understand.")          # DontUnderstand
    s(1 if version >= 400 else 2)     # Perspective (second person)
    s(1)                              # ShowExits
    s(0)                              # WaitTurns
    if version >= 390:
        s(1)                          # DispFirstRoom
        s(1 if BATTLE else 0)         # BattleSystem
        s(0)                          # MaxScore
        s("Player"); s(0); s("A test subject.")
        s(0)                          # Task (0 -> no AltDesc)
        s(0); s(0); s(0)              # Position ParentObject PlayerGender
        s(102); s(102)                # MaxSize MaxWt
        if BATTLE:
            p = BATTLE_PLAYER
            if version >= 400:
                for lo in ("stamina", "strength", "accuracy", "defence",
                           "agility"):
                    s(p[lo]); s(p[lo])    # each attribute is a [lo, hi] pair
                s(p["recovery"])
            else:
                s(p["stamina"]); s(p["strength"]); s(p["defence"])
        s(0)                          # EightPointCompass
        s(0); s(0); s(0)              # NoDebug NoScoreNotify NoMap
        s(0); s(0); s(0)              # NoAutoComplete NoControlPanel NoMouse
        s(0); s(0)                    # Sound Graphics
        if version >= 400:
            s(0); s("")               # StatusBox StatusBoxText
        s(3); s(3)                    # SizeMultiple WeightMultiple
        if version >= 400:
            s(0)                      # Embedded

    # ----------------------------------------------------------------- ROOMS
    def room(short, long_, exits):
        s(short); s(long_)
        if version < 400:
            s("")                     # LastDesc
        for slot in range(8):         # N E S W up down in out
            dest = dict(exits).get(slot)
            if dest is None:
                s(0)
            else:
                s(dest); s(0); s(0)
                if version >= 400:
                    s(0)              # Var3 -- 4.0's exit record is four ints
        if version < 400:
            s(""); s(0); s(""); s(0)  # AddDesc1 Task1 AddDesc2 Task2
            s(0); s(""); s(0)         # Obj AltDesc TypeHideObjects
        else:
            s(0)                      # Alts
        if version >= 390:
            s(0)                      # HideOnMap
    s(2)
    room("Lit Room", "A bright room.", {0: CAVE})
    room("Cave", "A cave with rough walls.", {2: LIT})

    # --------------------------------------------------------------- OBJECTS
    s(len(OBJECTS))
    for name, prefix, desc, where, kind, capacity, openable, static in OBJECTS:
        position, parent = position_parent(where, version)
        s(prefix); s(name)
        if version >= 400:
            s(0)                      # V$Alias count
        else:
            s("")                     # [1]$Alias
        s(1 if static else 0)         # Static
        s(desc)
        s(0 if static else position)  # InitialPosition (unused for statics)
        s(0); s(0); s("")             # Task TaskNotDone AltDesc
        if static:
            s(1); s(where[1])         # Where: ONE_ROOM, 1-based room number
        if version < 390:
            s({"container": 1, "surface": 2}.get(kind, 0))
            s(capacity)               # loader does capacity * 10 + 2
        else:
            s(1 if kind == "container" else 0)
            s(1 if kind == "surface" else 0)
            s(capacity * 10 + 2 if capacity else 0)
        if not static:
            s(1 if name in WEARABLE else 0)   # Wearable
            s(0)                      # SizeWeight (3.8/3.7: burden class)
            s(parent)
        # openable: 0 none, 1 open, 2 closed
        if version >= 400:
            s((0, 5, 6)[openable])    # Openable: 4.0 stores OPEN as 5
        else:
            s((0, 6, 5)[openable])    # ... and 3.9 and earlier as 6
        if openable and version >= 400:
            s(0)                      # Key
        s(SITLIE.get(name, 0))        # SitLie
        if not static:
            s(0)                      # Edible
        s(0)                          # Readable
        if not static:
            s(1 if name in WEAPONS else 0)    # Weapon
        if version >= 400:
            s(0)                      # CurrentState
            s(0)                      # ListFlag
            if BATTLE:
                b = battle_object(name)
                s(b["protection"]); s(b["hit"]); s(b["method"])
                s(b["accuracy"])
            s(""); s(0)               # InRoomDesc OnlyWhenNotMoved
        elif BATTLE:
            # 3.9 reads OBJ_BATTLE straight after Weapon: no Accuracy.
            b = battle_object(name)
            s(b["protection"]); s(b["hit"]); s(b["method"])

    # ----------------------------------------------------------------- TASKS
    s(len(TASKS))
    for command, text in TASKS:
        if version >= 400:
            s(1); s(command)              # V$Command count, then the command
        else:
            s(0); s(command)              # W$Command: count 0 -> one command
        s(text)                           # CompleteText
        s("")                             # ReverseMessage
        s("")                             # RepeatText (must stay empty)
        s("")                             # AdditionalMessage
        s(0)                              # ShowRoomDesc
        s(1)                              # Repeatable
        if version >= 390:
            s(0)                          # Reversible
            if version >= 400:
                s(0)                      # V$ReverseCommand: count 0, no strings
            else:
                s(0); s("")               # W$ReverseCommand: count 0 -> one
            s(3)                          # Where: all rooms
            s("")                         # Question
            s(0)                          # Restrictions
            s(0)                          # Actions
            if version >= 400:
                s("")                     # RestrMask
        else:
            s(0)                          # Score
            s(0)                          # SingleScore
            for _ in range(6):            # [6]<TASK_MOVE>Movements
                s(0); s(0)
                if version >= 380:
                    s(0)                  # Var3 -- 3.7 has none
            s(0)                          # Reversible
            s(0); s("")                   # ReverseCommand
            s(0); s(0)                    # WearObj1 WearObj2
            s(0); s(0); s(0)              # HoldObj1 HoldObj2 HoldObj3
            s(0)                          # Obj1
            s(0); s(0)                    # Task TaskNotDone
            s(""); s(""); s(""); s("")    # TaskMsg HoldMsg WearMsg CompanyMsg
            s(0)                          # NotInSameRoom
            s(0)                          # NPC
            s("")                         # Obj1Msg
            s(0)                          # Obj1Room
            s(3)                          # Where: all rooms
            s(0)                          # KillsPlayer
            s(0)                          # HoldingSameRoom
            s("")                         # Question
            s(0)                          # Obj2
            if version >= 380:
                s(0)                      # BWinGame -- 3.7 keeps it in the header

    # ---------------------------------------------------------------- EVENTS
    s(0)                              # Events
    s(len(NPCS))                      # NPCs
    for name, prefix, alias, desc, room_, inroom, topics, gender in NPCS:
        s(name); s(prefix)
        if version >= 400:
            # V$Alias: a count, then the strings (make_400_askprobe.py).
            if alias:
                s(1); s(alias)
            else:
                s(0)
        else:
            s(alias)                  # [1]$Alias
        s(desc)
        s(room_)                      # StartRoom (1-based; 0 = nowhere)
        s("")                         # AltText
        s(0)                          # Task
        s(len(topics))                # Topics
        for subject, reply in topics:
            s(subject); s(reply); s(0); s("")
        s(0)                          # Walks
        s(1); s("wanders in"); s("wanders off")
        s(inroom)                     # InRoomText
        if version >= 390:
            s(gender)                 # Gender (0 male, 1 female)
        if BATTLE:
            b = battle_npc(name)
            s(b["attitude"])
            if version >= 400:
                for lo in ("stamina", "strength", "accuracy", "defence",
                           "agility"):
                    s(b[lo]); s(b[lo])
                s(b["speed"]); s(b["killed_task"])
                s(b["recovery"]); s(b["stamina_task"])
            else:
                s(b["stamina"]); s(b["strength"]); s(b["defence"])
                s(b["speed"]); s(b["killed_task"])
    s(0)                              # RoomGroups
    if version == 370:
        for w in ("north", "east", "south", "west", "up", "down", "in", "out",
                  "look", "inventory", "examine", "pick up", "put down",
                  "wear", "remove", "goto", "help"):
            s(w)                      # [17]<COMMAND>Commands
    else:
        s(0)                          # Synonyms
    if version >= 390:
        s(0)                          # Variables
        s(0)                          # ALRs
        s(0)                          # CustomFont
    s("2026")                         # CompileDate
    if version < 400:
        s("    Wild    ")             # sPassword (4.0 keeps it in the trailer)

    return ("\r\n".join(L) + "\r\n").encode("latin-1")


SIGNATURES = {
    390: bytes([0x3c, 0x42, 0x3f, 0xc9, 0x6a, 0x87, 0xc2, 0xcf,
                0x94, 0x45, 0x37, 0x61, 0x39, 0xfa]),
    380: bytes([0x3c, 0x42, 0x3f, 0xc9, 0x6a, 0x87, 0xc2, 0xcf,
                0x94, 0x45, 0x36, 0x61, 0x39, 0xfa]),
    370: bytes([0x3c, 0x42, 0x3f, 0xc9, 0x6a, 0x87, 0xc2, 0xcf,
                0x94, 0x45, 0x39, 0x61, 0x39, 0xfa]),
}
OUT = {370: "p37SURF.taf", 380: "p38SURF.taf", 390: "p39SURF.taf",
       400: "p4SURF.taf"}


def obfuscate(body):
    state = 0x00a09e86

    def draw():
        nonlocal state
        state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
        return (255 * state) // 0x1000000

    for _ in range(14):
        draw()
    return bytes(b ^ draw() for b in body)


def emit(version):
    body = build(version)
    out = OUT[version]
    if version >= 400:
        open(out + ".plain", "wb").write(body)
        subprocess.check_call([sys.executable, "taftool.py", "pack",
                               out + ".plain", "p4TAKE.taf", out])
        print("wrote %s (%d plain bytes)" % (out, len(body)))
    else:
        open(out, "wb").write(SIGNATURES[version] + obfuscate(body))
        open(out + ".plain", "wb").write(body)
        print("wrote %s (%d bytes)" % (out, 14 + len(body)))


if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        emit(v)
