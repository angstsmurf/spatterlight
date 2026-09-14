"""ADRIFT 3.8 / 3.7 probe: the p39EXAM examine-refusal world, one schema down.

`make_39_examprobe.py` measured the whole examine / read / open / close
refusal family under run390 (and `make_400_examprobe.py` under run400); the
3.80 and 3.70 halves stayed census-derived because run380 and run370 cannot
load a 3.90 file.  This authors the same world -- same rooms, same objects in
the same order, same `probe` task -- to the 3.8 schema (default) or the 3.7
one (`--v37`), exactly as make_38_darkprobe.py / make_37_darkprobe.py do for
the darkness world.  See those two for the field-by-field schema notes.

Carried over from the 3.9 world, plus a room nobody can reach:

    Test Room   north -> North Room, east -> Void Room
    North Room  the statue
    Void Room   NO long description (3.8 substitutes at load, 447FEE)
    Far Room    no exits in or out
    stone       no description, loose in the Test Room
    crate       no description, open container, holds the coin
    coin        "A gold coin.", in the crate
    statue      "A marble statue.", in the North Room
    gem         "A red gem.", in the Far Room -- never seen

The decompiles predicted that 3.8 would track 3.9 on every row.  It does not:
3.7 and 3.8 match objects anywhere in the world, so take / wear / examine /
open / close / buy of an absent object each have their own refusal, several
of them gated on the seen byte.  The gem separates seen from unseen.  3.7
also ends `open <present, not openable>` with a period (43D1E0) where 3.8
(42F071) and 3.9 (43A0C5) end in a bang.  Results in WINE-TRANSCRIPTS-TODO.md,
"PORTED 2026-09-14: the 3.7/3.8 absent-object refusals".

Feeds, each ending in an extra `probe` because 3.7/3.8 Save Transcript is
written before the last command:
  cmdfile_p3738exam.txt   cmdfile_p39exam.txt, cmdfile_p39exam2.txt, then
                          take / drop / wear / put rows on held, loose and
                          absent objects (also run on p39EXAM under run390)
  cmdfile_p3738exam2.txt  the gem and the statue, unseen then seen

Usage:   python3 make_3738_examprobe.py [--v37] [out.taf]
Session: sh fast.sh p38EXAM.taf cmdfile_p3738exam.txt run380.exe
         sh fast.sh p37EXAM.taf cmdfile_p3738exam.txt run370.exe
"""
import sys

args = sys.argv[1:]
V37 = "--v37" in args
args = [a for a in args if a != "--v37"]

L = []
def s(x): L.append(str(x))

# HEADER -- 3.7 carries the winning task's index here, -1 for none.
s("A synthetic %s examine-refusal probe." % ("3.7" if V37 else "3.8")); s("**")
s(0)
s("You have won."); s("**")
if V37:
    s(-1)                # WinTask

# GLOBAL -- seven fields.
s("Probe %sEXAM" % ("37" if V37 else "38"))   # GameName
s("SCARE probe")         # GameAuthor
s(10)                    # MaxCarried
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective (second person)
s(1)                     # ShowExits
s(0)                     # WaitTurns

# ROOMS
def room(short, long_, exits=()):
    s(short); s(long_); s("")        # Short Long LastDesc
    for slot in range(8):            # N E S W up down in out
        dest = dict(exits).get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0)
    s(""); s(0); s(""); s(0)         # AddDesc1 Task1 AddDesc2 Task2
    s(0); s(""); s(0)                # Obj AltDesc TypeHideObjects

s(4)
room("Test Room", "A bare room.", {0: 2, 1: 3})   # north -> North, east -> Void
room("North Room", "Another bare room.", {2: 1})
room("Void Room", "", {3: 1})                     # NO long description at all
room("Far Room", "Nobody comes here.")            # no way in: the gem is never seen

# OBJECTS -- positions: 1 held, 2 in parent, 2 + room number in a room.
def obj(prefix, short, desc, position, surfcont=0, capacity=0, openable=0,
        parent=0, sizeclass=0):
    s(prefix); s(short); s("")       # Prefix Short [1]Alias
    s(0)                             # Static (dynamic)
    s(desc)                          # Description
    s(position)                      # InitialPosition
    s(0); s(0); s("")                # Task TaskNotDone AltDesc
    s(surfcont); s(capacity)         # SurfaceContainer, Capacity (*10+2 in)
    s(0); s(sizeclass); s(parent)    # Wearable SizeWeightClass Parent
    s(openable)                      # Openable (on-disk 6 <-> internal 5 OPEN)
    s(0); s(0); s(0); s(0)           # SitLie Edible Readable Weapon

s(5)
obj("a", "stone", "", 3)                                   # 0: no description
obj("a", "crate", "", 3, surfcont=1, capacity=5,           # 1: no description,
    openable=6)                                            #    open, non-empty
obj("a", "coin", "A gold coin.", 2, parent=0)              # 2: inside the crate
obj("a", "statue", "A marble statue.", 4)                  # 3: the North Room
obj("a", "gem", "A red gem.", 6)                           # 4: Far Room, never seen

# TASKS -- one control, so the transcript proves the file is wired.
s(1)
s(0)                     # W$Command: count 0 -> 1 command
s("probe")
s("PROBE OK.")           # CompleteText
s("")                    # ReverseMessage
s("")                    # RepeatText (must stay empty)
s("")                    # AdditionalMessage
s(0)                     # ShowRoomDesc
s(1)                     # Repeatable
s(0)                     # Score
s(0)                     # SingleScore
for _ in range(6):       # [6]<TASK_MOVE>Movements: triples, pairs in 3.7
    s(0); s(0)
    if not V37:
        s(0)
s(0)                     # Reversible
s(0); s("")              # W$ReverseCommand
s(0); s(0)               # WearObj1 WearObj2
s(0); s(0); s(0)         # HoldObj1 HoldObj2 HoldObj3
s(0)                     # Obj1
s(0); s(0)               # Task TaskNotDone
s(""); s(""); s(""); s("")   # TaskMsg HoldMsg WearMsg CompanyMsg
s(0)                     # NotInSameRoom
s(0)                     # NPC
s("")                    # Obj1Msg
s(0)                     # Obj1Room
s(3)                     # Where: all rooms
s(0)                     # KillsPlayer
s(0)                     # HoldingSameRoom
s("")                    # Question (empty -> no hints)
s(0)                     # Obj2 (zero -> no state restriction)
if not V37:
    s(0)                 # WinGame

# EVENTS / NPCS
s(0)
s(0)

# tail
s(0)                     # RoomGroups
if V37:
    for w in ("north", "east", "south", "west", "up", "down", "in", "out",
              "look", "inventory", "examine", "pick up", "put down", "wear",
              "remove", "goto", "help"):
        s(w)             # [17]<COMMAND>Commands
else:
    s(0)                 # Synonyms
s("2026")                # CompileDate
s("    Wild    ")        # sPassword

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,
             0x39 if V37 else 0x36, 0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = args[0] if args else ("p37EXAM.taf" if V37 else "p38EXAM.taf")
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
