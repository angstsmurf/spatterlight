"""ADRIFT 3.7 probe: the 3.8 darkness/put probe world, in a 3.70 file.

The 3.7 twin of `make_38_darkprobe.py`, which is in turn the 3.8 twin of
`make_39_darkprobe.py`.  Same world, same objects in the same order, same one
repeatable `probe` task -- so a rule measured on all three files is measured
across the whole pre-4.0 range with nothing but the schema changing.

A 3.70 file is a 3.80 file in all but the four places sctafpar.cpp's
V370_PARSE_SCHEMA lists, and only three of them touch a file this small:

  HEADER      one extra integer after the win text, the 0-based index of the
              task that wins the game.  -1 is "no winning task", which is
              what a probe wants; 3.8 replaced it with a per-task flag.
  TASK        a movement is a PAIR, not a triple (3.7 has one flat
              destination list where 3.8 has separate "where" and "how"),
              and the record has no BWinGame at the end.
  _GAME_      where 3.8 has a counted synonyms table, 3.7 has a fixed block
              of seventeen strings: the (renameable) words for its built-in
              commands, in the order sctafpar.cpp's V370_COMMANDS lists them.

The fourth -- an object cannot start out on an NPC, so a held or worn object's
Parent is ignored -- makes no difference here: the probe has no NPCs, and its
one held object is held by the player either way.

Usage:   python3 make_37_darkprobe.py [out.taf]
Session: sh fast.sh p37DARK.taf cmdfile_p39putin.txt run370.exe
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER -- the trailing -1 is 3.7's "no task wins this game".
s("A synthetic 3.7 darkness probe."); s("**")
s(0)
s("You have won."); s("**")
s(-1)                    # WinTask

# GLOBAL -- seven fields, exactly as 3.8.
s("Probe 37DARK")        # GameName
s("SCARE probe")         # GameAuthor
s(10)                    # MaxCarried (also the pooled burden limit)
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective (second person)
s(1)                     # ShowExits
s(0)                     # WaitTurns

# ROOMS.  The cave's Obj/TypeHideObjects is the whole point: Obj is a 1-based
# DYNAMIC object number, TypeHideObjects is condition * 10 + hide, and
# condition 0 is "player isn't holding it".  So the cave is dark until the
# torch is taken, and lit afterwards.
def room(short, long_, exits=(), obj=0, altdesc="", typehide=0):
    s(short); s(long_); s("")        # Short Long LastDesc
    for slot in range(8):            # N E S W up down in out
        dest = dict(exits).get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0)
    s(""); s(0); s(""); s(0)         # AddDesc1 Task1 AddDesc2 Task2
    s(obj); s(altdesc); s(typehide)  # Obj AltDesc TypeHideObjects

s(2)
room("Lit Room", "A bright room.", {0: 2})
room("Dark Cave", "A cave with rough walls.", {2: 1},
     obj=1, altdesc="It is too dark to see.", typehide=1)

# OBJECTS -- byte-for-byte the 3.8 record.
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

s(6)
obj("an", "torch", "An unlit torch.", 3)                   # 1: the light switch
obj("a", "lamp", "A brass lamp.", 1)                       # 2: held from turn 1
obj("a", "stone", "A grey stone.", 4)                      # 3: in the cave
obj("a", "pebble", "", 4)                                  # 4: no description
obj("a", "box", "A wooden box.", 4, surfcont=1,            # 5: open, non-empty
    capacity=5, openable=6)
obj("a", "coin", "A gold coin.", 2, parent=0)              # 6: inside the box

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

# EVENTS / NPCS
s(0)
s(0)

# tail
s(0)                     # RoomGroups
for w in ("north", "east", "south", "west", "up", "down", "in", "out",
          "look", "inventory", "examine", "pick up", "put down", "wear",
          "remove", "goto", "help"):
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

out = sys.argv[1] if len(sys.argv) > 1 else "p37DARK.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
