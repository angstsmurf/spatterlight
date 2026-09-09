"""ADRIFT 3.9 probe: examines() in a room whose objects are hidden.

There is no lamp model in ADRIFT 4.  "Darkness" is a room whose object
condition holds and whose "Hide objects" box is ticked, and every pre-4.0
Runner recomputes that one predicate at the top of examines() into a byte it
then consults twice -- run390 44B888-44BA5A into var_BC, run380 43C708 into
var_F0, run370 434E95 into the same.  This probe drives all three of the
answers that byte selects, in one wired file:

    x <object present>      44BC37  "<player> can't see <the obj> very clearly."
    x <noun naming nothing>  44C477  "<player> can't see that very clearly."
    x me                     44C430  "<player> can just make out that <you>
                                      <are> okay."

plus the same rows with the light on, as their own controls, and the
`get all from <container>` row that shares the corpus's other open darkness
question ("You can't get anything from that.", run390 loc_463E77).

The world: a Lit Room holding an unlit torch, north to a Dark Cave whose
Obj/TypeHideObjects pair is "player isn't holding the torch" + hide, so the
same room is dark before the torch is taken and lit after it -- every row is
its own control, in one session, with no second file to keep in step.  The
player starts holding a brass lamp so that the held-object arm is reachable
in the dark, and the cave holds an object with a description, one with an
empty description (whose lit answer is the flat "Nothing special."), and an
open box with a coin in it.

The feed (cmdfile_p39dark.txt; CRLF, and mind the bare-Return rule):

    probe / x me / i / x lamp / n / x stone / x pebble / x box / x lamp /
    x me / x zzzz / read zzzz / read stone / get all from box / look /
    take stone / i / s / take torch / n / x stone / x pebble / x box /
    x lamp / x me / x zzzz / get all from box / probe

`probe` is a repeatable no-restriction task that must answer "PROBE OK." at
both ends, proving the file is wired; read every answer off the echo, and
check that all commands echoed before believing any of it.

Usage:   python3 make_39_darkprobe.py [out.taf]
Session: sh fast.sh p39DARK.taf cmdfile_p39dark.txt run390.exe
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 darkness probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39DARK")        # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective (second person)
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test subject.")     # PlayerDesc
s(0)                     # Task (0 -> no AltDesc)
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(102)                   # MaxSize
s(102)                   # MaxWt
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
s(3)                     # SizeMultiple
s(3)                     # WeightMultiple

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
    s(0)                             # HideOnMap (NoMap == 0)

s(2)
room("Lit Room", "A bright room.", {0: 2})
room("Dark Cave", "A cave with rough walls.", {2: 1},
     obj=1, altdesc="It is too dark to see.", typehide=1)

# OBJECTS -- all dynamic and loose, so the room listing sees them.
def obj(prefix, short, desc, position, container=0, capacity=0, openable=0,
        parent=0, sizeweight=0):
    s(prefix); s(short); s("")       # Prefix Short [1]Alias
    s(0)                             # Static (dynamic)
    s(desc)                          # Description
    s(position)                      # InitialPosition: 1 held, 2 in container,
                                     #   4 + room index in a room
    s(0); s(0); s("")                # Task TaskNotDone AltDesc
    s(container); s(0); s(capacity)  # Container Surface Capacity
    s(0); s(sizeweight); s(parent)   # Wearable SizeWeight Parent
    s(openable)                      # Openable (on-disk 6 <-> internal 5 OPEN)
    s(0); s(0); s(0); s(0)           # SitLie Edible Readable Weapon

s(6)
obj("an", "torch", "An unlit torch.", 4)                   # 1: the light switch
obj("a", "lamp", "A brass lamp.", 1)                       # 2: held from turn 1
obj("a", "stone", "A grey stone.", 5)                      # 3: in the cave
obj("a", "pebble", "", 5)                                  # 4: no description
obj("a", "box", "A wooden box.", 5, container=1,           # 5: open, non-empty
    capacity=52, openable=6, sizeweight=21)
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
s(0)                     # Reversible
s(0); s("")              # W$ReverseCommand
s(3)                     # Where: all rooms
s("")                    # Question
s(0)                     # Restrictions
s(0)                     # Actions

# EVENTS / NPCS
s(0)
s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate
s("    Wild    ")        # sPassword

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p39DARK.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
