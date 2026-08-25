#!/usr/bin/env python3
"""ADRIFT 3.9 probe: every absent-object / no-description refusal in examines().

Scarier answers this whole family with strings taken from run400, because
upstream SCARE was written against 4.0.  The 3.7/3.8 decompiles are readable
VB and say otherwise; run390's p-code agrees with them.  One row of the family
-- `x <noun that names nothing>` -- has since been measured off a run390
transcript (Merry_Murders, `x pocket` -> "Nothing special.") and fixed.  This
probe is built to measure the rest in a single 3.9 session.

What the decompiles predict, row by row, with the command that reads it:

    x stone      empty Description, object present
                 pre-4.0  "Nothing special."     (run370 435BF4, run380
                          43D545, run390 44C3DC -- all three are the SAME
                          `If msg = vbNullString` tail that the measured
                          `x pocket` row falls through to)
                 4.0      "You see nothing special about the stone."
                          (run400 471A08/471A1C fills the empty message in
                          before the tail can see it)
                 scarier  the 4.0 wording, for every version (sclibrar.cpp
                          lib_cmd_examine_object)

    x crate      empty Description but a listed content -- does the contents
                 sentence count as "described"?  Both generations append to
                 the message, so the tail should never fire: expect
                 "Inside the crate is a coin." alone.

    x statue     seen in the North Room, then absent
                 pre-4.0  "You can't see the statue from here!" (435937)
                 scarier  "You see no such thing."
    open statue  pre-4.0  "You can't see the statue."   (run400 475966)
    close statue pre-4.0  "You can't see the statue."   (run400 475C10)
                 scarier  "Open what?" / "Close what?" for both

    x statues    the plural row: "You can't see any statues here." (435A13),
                 a string that exists in run370.exe and run380.exe only

    x door       no such object at all, never seen
    open door    pre-4.0  "You can't open that."
    take door
    x zzzz       CONTROL: the measured row, expect "Nothing special."

    open         bare verb -- the row that decides whether SCARE's
    close        "<Verb> what?" survives anywhere
    take
    drop

    read stone   pre-4.0 `read` is not a verb of its own: it is ORed into
    read coin    the words that ENTER examines (run370 434E2A, run380
    read zzzz    43C69D, run390 44B7FF), so all three of these should answer
                 exactly as the matching `x` does.  Scarier disagrees twice:
                 `read coin` (in the open crate, and `x coin` finds it) says
                 "You see no such thing.", and `read zzzz` says the same
                 instead of falling to the measured "Nothing special." tail.
                 That last one is lib_cmd_read_other, a 400-only string.
    buy statue   the literal "that is for sale." is in run370/380.exe only,
                 but 390/400 compose the same sentence from the else arm
                 (run390 45E68F) -- so this row is a CONTROL too: it should
                 come back word for word.
    get off      "You are not standing on anything!" is in run390/400.exe only
    x all        "Please examine one object at a time." (435A43)
    x me         the self row

Read every answer off the echo, and check that all 26 commands echoed before
believing any of it.

The feed, in order (cmdfile_p39exam.txt; CRLF, and mind the bare-Return rule):

    x stone / x crate / x coin / x zzzz / x me / n / x statue / s /
    x statue / open statue / close statue / x statues / x door /
    open door / take door / open / close / take / drop / read stone /
    read coin / read zzzz / buy statue / get off / x all / probe

What scarier answers today, for the diff (harness/scare, SCR_SKIP_WAITKEY=1):

    x stone       You see nothing special about the stone.
    x crate       The crate is open.  A coin is inside the crate.
    x zzzz        Nothing special.          <- the fixed row, a control
    x statue      Nothing special.          (absent; no "from here!")
    open statue   Open what?
    close statue  You can't close that.
    x statues     Nothing special.
    x door        Nothing special.
    open door     Open what?                (run400 says "You can't open that.")
    take door     Take what?
    open/close    Open what? / You can't close that.
    take/drop     Take what? / Drop what?
    read stone    You can't read the stone!   (matches run370 6825 /
                                              run380 7217 -- not a split)
    read coin     You see no such thing.      (but `x coin` -> A gold coin.)
    read zzzz     You see no such thing.
    buy statue    I don't think that is for sale.
    get off       You are not standing on anything!
    x all         Please examine one object at a time.

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

s(2)
room("Test Room", "A bare room.", {0: 2})     # north -> North Room
room("North Room", "Another bare room.", {2: 1})

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
