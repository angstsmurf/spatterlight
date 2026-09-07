#!/usr/bin/env python3
"""ADRIFT 4.0 probe: which refusal does a named take that resolves to nothing
get -- "Take what?" or "There is nothing worth taking here."?

Three rows of the 48-row Wine batch end on this one cell (`cellar` T114
`take satchel`, `vendetta` `get key`, `zelda` `get key`): run400 answers
"There is nothing worth taking here." where scarier answers "Take what?".
The Runner uses BOTH, in the same game and often within a few turns of each
other -- The Cellar's `take satchel` gets the first and its `take hand` the
second -- so something about the noun, the room, or the object decides.

Three hypotheses the transcripts cannot separate:

  (a) VOCABULARY.  A noun that names some object SOMEWHERE in the game
      enters the take loop and comes out empty ("nothing worth taking");
      a noun that names nothing at all never enters it ("Take what?").
  (b) UNIVERSE.  The message is about the ROOM: with no loose takeable
      object on the floor, any take says "nothing worth taking"; with one,
      an unmatched noun says "Take what?".
  (c) SEEN.  Only an object the player has been shown counts, so the same
      noun changes answer once its object has been in a room description.

Two rooms, four objects, and no tasks at all (DontUnderstand is the
unmistakable "NO IDEA." so a fall-through can never be read as a refusal):

    object 0  "a coin"    dynamic, on Alpha's floor -- the only thing that
                          makes Alpha's take-all universe non-empty
    object 1  "a statue"  STATIC in Alpha -- present, named, never takeable
    object 2  "a widget"  dynamic, on Bravo's floor
    object 3  "a gizmo"   dynamic, on Bravo's floor

`cmdfile_take1.txt` never leaves Alpha, so widget and gizmo are unseen
throughout; it walks the room from "coin on the floor" to "floor empty" and
asks the same three takes on both sides:

     1  look           baseline; Alpha lists the coin
     2  take zzz       unknown noun, universe NON-empty
     3  take widget    known noun, absent, unseen, universe NON-empty
     4  take statue    present, named, static
     5  take coin      the take that empties the universe
     6  take zzz       unknown noun, universe EMPTY
     7  take widget    known noun, absent, unseen, universe EMPTY
     8  take all       control for the flat message itself
     9  drop coin      universe NON-empty again
    10  take zzz       repeat of 2 -- did anything else change?
    11  take widget    repeat of 3

Rows 2 vs 6 and 3 vs 7 are hypothesis (b); rows 2 vs 3 (and 6 vs 7) are
hypothesis (a).

`cmdfile_take2.txt` visits Bravo first, so the SAME nouns are now seen:

     1  e              to Bravo, which lists the widget and the gizmo
     2  look           baseline for Bravo
     3  w              back to Alpha
     4  take widget    known noun, absent, SEEN, universe non-empty
     5  take coin      empty the universe again
     6  take widget    known noun, absent, SEEN, universe empty
     7  take zzz       unknown noun, universe empty
     8  x widget       control: the absent-noun EXAMINE wording, which is
                       already ported (463640 pass 2), so it says whether
                       this game's "seen" bookkeeping is working at all

Feed 2 rows 4/6 against feed 1 rows 3/7 are hypothesis (c).

Usage (add --tie for p4TAKE2, --hidden for p4TAKE3):
    python3 make_400_takeprobe.py
    python3 taftool.py pack p4TAKE.taf.plain <donor.taf> p4TAKE.taf

Drive it with, from ~/adrift-battle/runner/wine:
    sh fast.sh p4TAKE.taf cmdfile_take1.txt run400
    sh fast.sh p4TAKE.taf cmdfile_take2.txt run400
"""
import sys

SEP = "\xbd\xd0"

TIE = "--tie" in sys.argv[1:]
HIDDEN = "--hidden" in sys.argv[1:]

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Take-refusal probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Take Probe 400")
s("SCARE probe")
s("NO IDEA.")            # DontUnderstand -- deliberately unmistakable
s(1)                     # Perspective: second person
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player"); s(0); s("A test subject.")
s(0)                     # Task
s(0); s(0); s(0)         # Position, ParentObject, PlayerGender
s(100); s(100)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0); s(0); s(0)         # NoDebug, NoScoreNotify, NoMap
s(0); s(0); s(0)         # NoAutoComplete, NoControlPanel, NoMouse
s(0); s(0)               # Sound, Graphics -- both off, so no resource fields
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS -- Alpha east to Bravo, Bravo west to Alpha.
def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0)                 # Alts
    s(0)                 # HideOnMap

s(2)
room("Alpha", "The first room.", {1: 2})
room("Bravo", "The second room.", {3: 1})

# OBJECTS.  A dynamic object's home is #InitialPosition, which the Runner's
# loader maps `- 1`, then folds 1/2 onto "in object"/"on object" and drops
# everything above them a further 2, so a room lands one-based: room 0 is
# InitialPosition 4 (see obj_initial_location_code(), scobjcts.cpp).
def obj(short, room=None, static=False, prefix="a", aliases=()):
    s(prefix)            # Prefix
    s(short)             # Short
    s(len(aliases))      # V$Alias count
    for a in aliases:
        s(a)
    s(1 if static else 0)
    s("A plain thing.")  # Description
    s(0 if static else room + 4)         # InitialPosition
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    if static:
        s(1); s(room + 1)                # Where: ROOM_LIST1 Type 1, 1-based
    s(0); s(0); s(0)     # Container, Surface, Capacity
    if not static:
        s(0); s(0); s(0) # Wearable, SizeWeight, Parent
                         # (a static's {OBJECT:#Parent} special reads nothing)
    s(0)                 # Openable: not openable, so no Key
    s(0)                 # SitLie
    if not static:
        s(0)             # Edible
    s(0)                 # Readable
    if not static:
        s(0)             # Weapon
    s(0)                 # CurrentState: 0, so no States/StateListed
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

if HIDDEN:
    # The p4TAKE3 variant: does an object the player has SEEN but that a task
    # has since moved to HIDDEN still answer a named take?  Two namesakes so
    # that the answer can be read twice over -- once as "one candidate or
    # none" and once as "a tie or not".
    s(5)
    obj("coin",   room=0)                    # 0 -> Alpha, keeps it non-empty
    obj("widget", room=1, prefix="a red")    # 1 -> Bravo, hidden by `vanish`
    obj("widget", room=1, prefix="a blue")   # 2 -> Bravo, hidden by `vanish2`
    obj("lamp",   room=1)                    # 3 -> Bravo, stays put: control
    obj("gizmo",  room=1)                    # 4 -> Bravo, stays put
elif TIE:
    # The p4TAKE2 variant: everything absent-and-seen is in Bravo, and the
    # nouns collide, so a feed that visits Bravo once and comes back can ask
    # whether the flat refusal survives a TIE and whether it can be reached
    # by an alias or by a Prefix word alone.
    s(4)
    obj("widget", room=1, prefix="a red")    # 0 \ two seen absent namesakes
    obj("widget", room=1, prefix="a blue")   # 1 /
    obj("lamp",   room=1, aliases=("light",))# 2 -> reachable only by alias
    obj("coin",   room=0)                    # 3 -> keeps Alpha non-empty
else:
    s(4)
    obj("coin",   room=0)                 # 0 -> Alpha floor, the whole universe
    obj("statue", room=0, static=True)    # 1 -> Alpha, present but never takeable
    obj("widget", room=1)                 # 2 -> Bravo floor
    obj("gizmo",  room=1)                 # 3 -> Bravo floor

# TASKS, EVENTS, NPCS.  The --hidden variant needs two, one per widget: a
# bare command whose only action is TASK_ACTION Type 0 (move object) Var1 =
# 3 + dynamic index, Var2 = 0 (to room), Var3 = 0, which is ADRIFT's "move
# to hidden".  Everything else has none of any kind, so every line the probe
# types reaches the library.
def hide_task(cmd, dynamic_index):
    s(1); s(cmd)
    s("POOF.")           # CompleteText
    s(""); s(""); s("")  # ReverseMessage, RepeatText, AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(3)                 # Where: ROOM_LIST0 Type 3 = all rooms
    s("")                # Question (empty, so no Hint1/Hint2)
    s(0)                 # Restrictions
    s(1)                 # Actions
    s(0)                 # Type 0 = move object
    s(3 + dynamic_index) # Var1: 0 all held, 1 all worn, 2 referenced, 3+ = object
    s(0)                 # Var2: 0 = "to room"
    s(0)                 # Var3: room 0 = hidden
    s("")                # RestrMask

if HIDDEN:
    s(2)
    hide_task("vanish", 1)
    hide_task("vanish2", 2)
else:
    s(0)                 # Tasks
s(0)                     # Events
s(0)                     # NPCs

s(0); s(0)               # RoomGroups, Synonyms

s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
args = [a for a in sys.argv[1:] if not a.startswith("--")]
out = args[0] if args else ("p4TAKE3.taf.plain" if HIDDEN
                            else "p4TAKE2.taf.plain" if TIE
                            else "p4TAKE.taf.plain")
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
