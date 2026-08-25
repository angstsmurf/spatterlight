"""ADRIFT 3.9 probe: every absent-object / no-description refusal in examines().

MEASURED AND CLOSED 2026-08-25.  Two run390 sessions -- Adrift_41_p39exam.txt
(the 29-command feed below) and Adrift_43_p39exam.txt (a 19-command follow-up,
cmdfile_p39exam2.txt, adding the open/close-a-real-object rows, the held-object
`x`, and a second reading of the Void Room).  Every command echoed in both.  A
4.00 twin, make_400_examprobe.py / p4EXAM.taf, was built to answer the same
rows one Runner up (run400 refuses to load a 3.90 file outright).

Four splits came out of it and are ported; after them **run390 and Scarier
agree on all 48 rows, word for word**:

    x <object, empty Description>   pre-4.0 "Nothing special."      lib_cmd_examine_object
    read <noun naming nothing>      pre-4.0 "Nothing special."      lib_cmd_read_other
    open <anything unresolvable>    all     "You can't open that."  lib_cmd_open_other
    close <present, not closeable>  pre-4.0 ends in "." not "!"     lib_cmd_close_object
    room with no description        3.8/3.9 "There is nothing of interest here."

What was measured, row by row -- run390 on the left, run400 (p4EXAM.taf) right:

    x stone       Nothing special.        | You see nothing special about the stone.
    x crate       (contents sentence; the tail never fires, as predicted)
    x zzzz        Nothing special.        | You see no such thing.
    x statue      Nothing special.        | You can't see the statue from here!
    x statues     Nothing special.        | You see no such thing.
    x door        Nothing special.        | You see no such thing.
    open          You can't open that.    | You can't open that.
    open door     You can't open that.    | You can't open that.
    open statue   You can't open that.    | You can't see the statue.
    close         You can't close that.   | You can't close that.
    close statue  You can't close that.   | You can't see a statue.
    open stone    You can't open the stone!  | You can't open the stone!
    close stone   You can't close the stone. | You can't close the stone!
    read stone    You can't read the stone!  | You can't read the stone!
    read coin     You can't read the coin!   | You see no such thing.   (in the closed crate)
    read zzzz     Nothing special.        | You see no such thing.
    buy statue    I don't think that is for sale. | You can't see the statue.
    get off       You are not standing on anything!   (both)
    x all         Please examine one object at a time. (both)
    x me          A test subject.                     (both)
    take door     Take what?  /  take, drop: Take what? / Drop what?   (both)
    e / look      Void Room: "There is nothing of interest here.  You can only
                  move west." | run400 prints the exits alone.

Two predictions in the earlier draft of this docstring were WRONG, and are worth
keeping as warnings:

  * `x <seen but absent object>` was predicted to answer "You can't see the
    statue from here!" in 3.9, from run370 435937.  It does not -- 3.9 answers
    the flat tail.  In run390 `co()` never matches the absent statue at all, so
    the command falls to the generic handler at 45D454 (`push "that"`).  The
    "You can't see ..." family is 4.0 behaviour, and needs the seen-but-absent
    resolver, which is still unported.
  * `read coin` was predicted to expose a scarier bug.  It does not: scarier
    already answered "You can't read the coin!", which is what run390 says.
    Re-run ./scare before trusting any "what scarier answers today" block.

Still open after this probe (all four are one port -- the 4.0 resolver):

    x statue      You can't see the statue from here!
    open statue   You can't see the statue.        (definite article)
    close statue  You can't see a statue.          (indefinite!)
    buy statue    You can't see the statue.

Still unmeasured: the 3.70 and 3.80 halves.  The decompiles say 3.8 tracks 3.9
throughout except that it substitutes "There is nothing of interest here." into
the empty Long at LOAD (447FEE) rather than at print; 3.7 differs on at least
one row, composing `open <present, not openable>` with a period at 43D1E0 where
3.8 (42F071) and 3.9 (43A0C5) have a separate branch ending in a bang.

The feed, in order (cmdfile_p39exam.txt; CRLF, and mind the bare-Return rule):

    x stone / x crate / x coin / x zzzz / x me / n / x statue / s /
    x statue / open statue / close statue / x statues / x door /
    open door / take door / open / close / take / drop / read stone /
    read coin / read zzzz / buy statue / get off / x all / probe / e / look / w

and the follow-up (cmdfile_p39exam2.txt):

    e / open / close / take / drop / open door / w / open stone / close stone /
    open crate / close crate / x crate / open crate / take stone / i /
    x stone / read stone / drop stone / probe

`x zzzz` is a control -- the row already measured off Merry_Murders -- and
`probe` is a repeatable no-restriction task that must answer "PROBE OK.",
proving the file is wired.  Read every answer off the echo, and check that all
commands echoed before believing any of it.

Usage:   python3 make_39_examprobe.py [out.taf]
Session: sh runner_savetranscript.sh p39EXAM.taf cmdfile_p39exam.txt run390.exe
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 examine-refusal probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39EXAM")        # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective (second person)
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem (0 -> no <BATTLE> block, no OBJ_BATTLE)
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test subject.")     # PlayerDesc
s(0)                     # Task (0 -> no AltDesc)
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(102)                   # MaxSize   -> 10 * 3^2 = 90
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

# ROOMS -- two, wired north/south, so the statue can be seen and then left.
def room(short, long_, exits=()):
    s(short); s(long_); s("")
    for slot in range(8):            # N E S W up down in out
        dest = dict(exits).get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0)
    s(""); s(0); s(""); s(0); s(0); s(""); s(0)
    s(0)                             # HideOnMap (NoMap == 0)

s(3)
room("Test Room", "A bare room.", {0: 2, 1: 3})   # north -> North, east -> Void
room("North Room", "Another bare room.", {2: 1})
room("Void Room", "", {3: 1})                     # NO long description at all

# OBJECTS -- all dynamic and loose, so the room listing SEES them (a static is
# never listed, and an unlisted object is not referenceable; see the object
# seen model note in notes/WINE-TRANSCRIPTS-TODO.md).
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

s(4)
obj("a", "stone", "", 4)                                   # 0: no description
obj("a", "crate", "", 4, container=1, capacity=52,         # 1: no description,
    openable=6, sizeweight=21)                             #    open, non-empty
obj("a", "coin", "A gold coin.", 2, parent=0)              # 2: inside the crate
obj("a", "statue", "A marble statue.", 5)                  # 3: the North Room

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

out = sys.argv[1] if len(sys.argv) > 1 else "p39EXAM.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
