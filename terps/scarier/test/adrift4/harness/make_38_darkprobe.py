"""ADRIFT 3.8 probe: the 3.9 darkness/put probe world, in a 3.80 file.

`make_39_darkprobe.py` builds p39DARK.taf, and every put and darkness answer
measured so far on the pre-4.0 arm came out of driving it under run390.  The
3.7 and 3.8 halves of those rules stayed census-derived because run380 and
run370 cannot load a 3.90 file and ADRIFT's own Generators only convert
UPWARD -- gen370 handed a 3.80 file opens Untitled.  So the 3.8 twin has to
be authored, which is what this is: the same world, the same objects in the
same order, the same one repeatable `probe` task, laid out to the version 3.8
schema (sctafpar.cpp V380_PARSE_SCHEMA) behind the version 3.8 signature.

What version 3.8 does differently, field for field:

  _GAME_    no Variables, no ALRs, no CustomFont flag; the tail is just
            $CompileDate and the unstored password string.
  GLOBAL    seven fields only -- GameName, GameAuthor, MaxCarried,
            DontUnderstand, Perspective, ShowExits, WaitTurns.  Everything
            else 3.9 stores (player name, description, gender, MaxScore, the
            size and weight limits, the sound and graphics flags) is absent
            and synthesised: MaxScore by summing the tasks, MaxSize and MaxWt
            from MaxCarried.
  ROOM      as 3.9 without the trailing HideOnMap (3.8 has no NoMap global).
  OBJECT    one #SurfaceContainer integer (1 container, 2 surface) where 3.9
            has the BContainer/BSurface pair, a container capacity that is
            multiplied by ten plus two on the way in (so on-disk 5 is the 52
            the 3.9 file writes literally), and a SizeWeight that is a 0..4
            burden CLASS, not a 4.0 packed pair.
  TASK      the pre-restriction/action shape: Score, SingleScore, six
            three-integer movement slots, the Wear/Hold/Obj1/Obj2/NPC
            restriction fields and their messages, all inline, with the 4.0
            restriction and action lists derived from them.
  OBJECT    initial positions run 0 hidden, 1 held, 2 in/on the parent,
  position  3..2+rooms the rooms, 3+rooms worn -- one lower than 4.0's,
            which is why the cave objects are 4 here and 5 in the 3.9 twin.

Usage:   python3 make_38_darkprobe.py [out.taf]
Session: sh measure38.sh p38DARK.taf cmdfile_p39putin.txt run380.exe
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.8 darkness probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL -- seven fields, and that is the whole of a 3.8 game's globals.
s("Probe 38DARK")        # GameName
s("SCARE probe")         # GameAuthor
s(10)                    # MaxCarried (also the pooled burden limit)
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective (second person)
s(1)                     # ShowExits
s(0)                     # WaitTurns

# ROOMS.  The cave's Obj/TypeHideObjects is the whole point: Obj is a 1-based
# DYNAMIC object number, TypeHideObjects is condition * 10 + hide, and
# condition 0 is "player isn't holding it".  So the cave is dark until the
# torch is taken, and lit afterwards.  Identical to 3.9 but for the absent
# HideOnMap.
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
for _ in range(6):       # [6]<TASK_MOVE>Movements, #Var1 #Var2 #Var3 each
    s(0); s(0); s(0)
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
s(0)                     # WinGame

# EVENTS / NPCS
s(0)
s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s("2026")                # CompileDate
s("    Wild    ")        # sPassword

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x36,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p38DARK.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
