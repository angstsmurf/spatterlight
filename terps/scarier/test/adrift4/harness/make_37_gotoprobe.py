"""ADRIFT 3.7 gotoplace probe: the 3.8 goto world in a 3.70 file.

The 3.7 twin of `make_38_gotoprobe.py`; the schema changes are the ones
`make_37_darkprobe.py` lists (header WinTask, movement pairs, no BWinGame, the
seventeen command words in place of the synonyms table).  run370's gotoplace
(42BE2C) differs from run380's (432054) in one place: where run380 tests the
map window (Form17.Tag), run370 also enters on the author's word for built-in
command 15, "goto" -- c(word) anywhere in the line, bare word exits, and the
place is Right(line, Len(line) - Len(word) - 1).  Pass a second argument to
rename that word (the alias variant).

Rooms: First Room -N-> Hall; Hall -E-> Kitchen, -N-> Garden (exit needs task 1
`probe` done), -W-> Red Hall, -U-> Blue Hall; Tower has no exits.

gotoplace() run380 432054 and its route finder Proc_2_8_434C50.

Usage:
    python3 make_37_gotoprobe.py p37GOTO.taf
    python3 make_37_gotoprobe.py p37GOTOW.taf travel
Session:
    ./par.sh jobs_pgoto37.txt 2    (cmdfile_pgoto*.txt, run370x.exe)
"""
import sys

GOTO_WORD = sys.argv[2] if len(sys.argv) > 2 else "goto"

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.7 goto probe."); s("**")
s(0)
s("You have won."); s("**")
s(-1)                    # WinTask (3.7: none)

# GLOBAL -- seven fields, exactly as 3.8.
s("Probe 37GOTO")        # GameName
s("SCARE probe")         # GameAuthor
s(10)                    # MaxCarried (also the pooled burden limit)
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective (second person)
s(1)                     # ShowExits
s(0)                     # WaitTurns

# ROOMS -- one, lit.
def room(short, long_, exits=(), obj=0, altdesc="", typehide=0):
    s(short); s(long_); s("")        # Short Long LastDesc
    for slot in range(8):            # N E S W up down in out
        dest = dict(exits).get(slot)
        if dest is None:
            s(0)
        else:
            if isinstance(dest, tuple):
                s(dest[0]); s(dest[1]); s(dest[2])
            else:
                s(dest); s(0); s(0)
    s(""); s(0); s(""); s(0)         # AddDesc1 Task1 AddDesc2 Task2
    s(obj); s(altdesc); s(typehide)  # Obj AltDesc TypeHideObjects

s(7)
room("First Room", "The first room.", {0: 2})
room("Hall", "A hall.", {2: 1, 1: 3, 0: (4, 1, 0), 3: 5, 4: 6})
room("Kitchen", "A kitchen.", {3: 2})
room("Garden", "A garden.", {2: 2})
room("Red Hall", "A red hall.", {1: 2})
room("Blue Hall", "A blue hall.", {5: 2})
room("Tower", "A tower.")

# OBJECTS -- all dynamic and loose, so the room listing sees them.
def obj(prefix, short, desc, position, surfcont=0, capacity=0, openable=0,
        parent=0, sizeclass=0):
    s(prefix); s(short); s("")       # Prefix Short [1]Alias
    s(0)                             # Static (dynamic)
    s(desc)                          # Description
    s(position)                      # InitialPosition: 1 held, 2 in parent,
                                     #   2 + room index in a room
    s(0); s(0); s("")                # Task TaskNotDone AltDesc
    s(surfcont); s(capacity)         # SurfaceContainer, Capacity (*10+2 in)
    s(0); s(sizeclass); s(parent)    # Wearable SizeWeightClass Parent
    s(openable)                      # Openable (on-disk 6 <-> internal 5 OPEN)
    s(0); s(0); s(0); s(0)           # SitLie Edible Readable Weapon

s(1)
obj("a", "stone", "A grey stone.", 3)                      # 1: in the room

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
for _ in range(6):       # [6]<TASK_MOVE>Movements, #Var1 #Var2 each -- 3.7
    s(0); s(0)           #   has no Var3
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
                         # ... and no BWinGame: 3.7 keeps it in the header

# EVENTS
s(0)

# NPCS -- none.
s(0)

# tail
s(0)                     # RoomGroups
for w in ("north", "east", "south", "west", "up", "down", "in", "out",
          "look", "inventory", "examine", "pick up", "put down", "wear",
          "remove", GOTO_WORD, "help"):
    s(w)                 # [17]<COMMAND>Commands -- 3.7's whole synonym support
s("2026")                # CompileDate
s("    Wild    ")        # sPassword

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x39,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p37GOTO.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
