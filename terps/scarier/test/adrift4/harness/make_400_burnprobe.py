#!/usr/bin/env python3
"""ADRIFT 4.0 probe: why does run400 refuse a `%object%` task Scarier runs?

CLOSED 2026-08-25, in four Wine rounds: Adrift_6_p4burn.txt (thirteen restriction
cells), Adrift_2_p4burn.txt (Repeatable), Adrift_12_p4burn.txt and
Adrift_13_p4burn.txt (case), plus the bisect of the game itself in
Adrift_1/3/4/5/7/8/9/10/11_xfilesbisect.txt.  The answer is in the last
section; the earlier rounds are
kept because each one killed a hypothesis, and a killed hypothesis is the only
thing that stops the next session re-testing it.

The_X-Files_A_New_Beginning.taf (4.00) has task 24, `Burn %object%`, restr
mask `#A#`:

    RESTR type=0 v1=1 v2=3 v3=0 fail=[There's nothing here to burn!]
    RESTR type=3 v1=0 v2=2 v3=-1 fail=[]

i.e. "any object is visible to the player" AND "the player is alone".  Scarier
passed both and printed the CompleteText; run400 answers

    I don't understand what you want me to do with The Memo.

which is generaltasks' end-of-turn fallback -- so run400 refused the task and
printed NOTHING.

ROUND ONE -- thirteen restriction shapes.  Two rooms, one nowhere NPC, a
trinket each side, and a cell per shape:

    pa   no restrictions at all              pf   sure-fail 1, message
    burn ditto, on xfiles' own verb          pg   sure-fail 2, message
    pb   restriction 1 alone                 ph   sure-fail 1 + passing 2
    pc   restriction 2 alone                 pi   passing 1 + sure-fail 2
    pd   xfiles' EXACT shape (msg on 1 only) pj   sure-fail 1 no msg + 2 msg
    pe   both, a message on each             pk   sure-fail 1 msg + 2 none
                                             pl   one sure-failer, no message

**Every one of the thirteen agreed with scarier, `pd` included.**  So the
restrictions are not the refusal, and neither is `burn` being intercepted.
Two by-products worth keeping:

  * run400 prints the FIRST failing restriction's message and stops; an empty
    message is still "the message" -- it does not skip forward to a later
    failing restriction that has one (pj is silent, pk prints A).
  * the nowhere NPC does NOT collide with an unnormalised StartRoom 0: `pc`
    passed in the start room and after a round trip alike.

ROUND TWO -- Repeatable.  Task 24 has Repeatable OFF and every round-one cell
had it ON.  `pm`/`pn`/`po` re-run the interesting shapes with it OFF.  All
three ran.  Not it either.

ROUND THREE/FOUR -- case, and it is the whole answer.  The one thing task 24
has that no probe cell had is CAPITAL LETTERS: its command is `Burn %object%`
and its objects are `Memo`, `Coffee Mug`, `Gun Holster`.  Bisecting the game
itself (lowering the verb at plain line 5929 and the Memo's Short at 2376,
separately and together) showed the task only fires when BOTH are lowered
(Adrift_11_xfilesbisect.txt line 11).
The probe pins each half:

    pattern `PX %object%`, Short `coin`     px coin      -> PASS
                                            PX coin      -> PASS
    pattern `PY`   (literal)                py           -> PASS
    pattern `PZ *` (wildcard)               pz anything  -> PASS
    pattern `pa %object%`, Short `Widget`   pa widget    -> refused
                                            pa Widget    -> refused
    pattern `pa %object%`, Short `brass key`, Prefix `a small`
                                            pa brass key -> PASS
                                            pa key       -> "I don't understand."
                                            pa a brass key / pa the brass key
                                            pa small brass key -> refused
    pattern `pa %object%`, Short `gem`, Alias `jewel`
                                            pa gem       -> PASS
                                            pa jewel     -> PASS
                                            pa a gem / pa the gem -> refused

So run400 matches a `%object%` task command as:

    LCase(pattern) with %object% textually Replace()d by the object's Short or
    one of its Aliases VERBATIM, compared for exact equality to LCase(input).

Verb case is irrelevant (both sides are lowered); literals and wildcards are
case-insensitive; but a capitalised Short can never bind, and no article, no
Prefix and no partial name is tolerated.  The listing is
mdlSpreadTheLoad.bas Proc_19_37_458E6C (loc_458BBC-loc_458E69) and there is no
LCase() anywhere on the substituted name.  Its `%character%` twin at loc_46918C
DOES lower the Name (4691B4) and each Alias (469207) before the Replace(), so
the asymmetry is one-sided and `%character%` stays tolerant -- ADRIFTMAS
Party's `[kiss {the} %character%]` over an NPC named "Mystery" runs in both.

Ported as uip_compare_reference_strict() in scparser.cpp, gated on
TAF_VERSION_400 and on the reference being an object.  Corpus: 64 of the 432
.taf are 4.0 games with `%object%` in a task command, and the change moved
exactly one golden line -- xfiles' `burn memo`, which now answers what the two
Wine transcripts answer.

THE 3.90 HALF, measured the same day by make_39_caseprobe.py / p39CASE.taf
(Adrift_1_p39case.txt): 3.90 binds just as strictly -- no article, no Prefix,
no partial name -- but it DOES fold case, so `pa Widget` and `pa widget` both
run there.  The port is therefore gated at 3.90 for the binding and at 4.0 for
the case sensitivity.  run370/run380 have no `"%object%"` literal at all, so
before 3.90 a `%object%` pattern matches nothing at all.

FOOTGUN: Capacity packs as tens = object count, units = size index, so
Capacity 99 is invalid and hangs run400 for ever at "Loading...".  Use 52.

Usage:
    python3 make_400_burnprobe.py p4BURN.plain
    python3 taftool.py pack p4BURN.plain <donor.taf> p4BURN.taf
    LOAD_SLEEP=22 sh measure.sh p4BURN.taf cmdfile_burn.txt
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Burn restriction probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Burn Restriction Probe 400")
s("SCARE probe")
s("I don't understand.")
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

# OBJECTS -- one trinket each side, so "any object is visible to the player"
# has something to see in either room and every command has a real referent.
def obj(short, pos, prefix="a", aliases=()):
    s(prefix)            # Prefix
    s(short)             # Short
    s(len(aliases))      # V$Alias count
    for a in aliases:
        s(a)
    s(0)                 # Static
    s("A trinket.")      # Description
    s(pos)               # InitialPosition: 4 + the 0-based room
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(0); s(0); s(0)     # Container, Surface, Capacity
    s(0); s(0); s(0)     # Wearable, SizeWeight, Parent
    s(0); s(0); s(0); s(0); s(0)   # Openable, SitLie, Edible, Readable, Weapon
    s(0); s(0)           # CurrentState, ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(5)
obj("coin", 4)           # Alpha
obj("ring", 5)           # Bravo
obj("Widget", 4)         # Alpha -- a CAPITALISED Short name (round three)
# Round four: a two-word Short behind a two-word Prefix, and an alias, both
# all-lowercase, to pin what else the substituted string will and will not
# tolerate -- a partial name, an article, the Prefix itself.
obj("brass key", 4, prefix="a small")
obj("gem", 4, aliases=("jewel",))

# RESTRICTIONS -- (Type, Var1, Var2, Var3, FailMessage).  Types 0 and 3 both
# serialise as #Type #Var1 #Var2 #Var3 $FailMessage.
#
# Type 0 Var1 = 1 is "any object", Var2 = 3 is "is visible to", Var3 = 0 is the
# player: xfiles' restriction 1.  Var1 = 0 is the negated form, "no object is
# visible to the player", which is false whenever anything is in the room --
# the sure-failing twin.
def VIS(msg):   return (0, 1, 3, 0, msg)
def NOVIS(msg): return (0, 0, 3, 0, msg)
# Type 3 Var1 = 0 is the player, Var2 = 2 is "is alone", Var2 = 3 is "is not
# alone", Var3 = -1 (no target): xfiles' restriction 2 and its twin.  With the
# game's only NPC nowhere, "alone" is true and "not alone" is the sure-failer.
def ALONE(msg):    return (3, 0, 2, -1, msg)
def NOTALONE(msg): return (3, 0, 3, -1, msg)

# TASKS
def task(cmd, text, restrs=(), mask="", repeat=1):
    s(1); s(cmd)
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(repeat)            # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(3)                 # Where: all rooms
    s("")                # Question
    s(len(restrs))
    for r in restrs:
        for field in r:
            s(field)
    s(0)                 # Actions
    s(mask)              # RestrMask

s(20)
task("pa %object%",   "PA PASS.")
task("burn %object%", "BURN PASS.")
task("pb %object%",   "PB PASS.", [VIS("PB FAIL.")],   "#")
task("pc %object%",   "PC PASS.", [ALONE("PC FAIL.")], "#")
# xfiles task 24's exact shape: a message on the visibility test, none on the
# alone test.
task("pd %object%",   "PD PASS.", [VIS("PD FAIL A."), ALONE("")],
     "#A#")
task("pe %object%",   "PE PASS.", [VIS("PE FAIL A."), ALONE("PE FAIL B.")],
     "#A#")
task("pf %object%",   "PF PASS.", [NOVIS("PF FAIL.")],    "#")
task("pg %object%",   "PG PASS.", [NOTALONE("PG FAIL.")], "#")
task("ph %object%",   "PH PASS.", [NOVIS("PH FAIL A."), ALONE("PH FAIL B.")],
     "#A#")
task("pi %object%",   "PI PASS.", [VIS("PI FAIL A."), NOTALONE("PI FAIL B.")],
     "#A#")
task("pj %object%",   "PJ PASS.", [NOVIS(""), NOTALONE("PJ FAIL B.")],
     "#A#")
task("pk %object%",   "PK PASS.", [NOVIS("PK FAIL A."), NOTALONE("")],
     "#A#")
task("pl %object%",   "PL PASS.", [NOVIS("")], "#")
# The second round, added 2026-08-25 after the first thirteen cells came back
# with every restriction shape AGREEING between run400 and scarier -- including
# `pd`, xfiles task 24's exact shape.  So the refusal is not the restrictions.
# The one field left that differs is Repeatable, which task 24 has OFF and
# every cell above has ON.
task("pm %object%",   "PM PASS.", (), "", repeat=0)
task("pn %object%",   "PN PASS.", [VIS("PN FAIL A."), ALONE("")], "#A#",
     repeat=0)
task("po %object%",   "PO PASS.", [VIS("PO FAIL A.")], "#", repeat=0)
# Round three, added 2026-08-25.  Repeatable is not it either -- `pm`/`pn`/`po`
# all ran.  The one thing left that xfiles' task 24 has and no probe cell had is
# CAPITAL LETTERS: its command is `Burn %object%` and its objects are `Memo`,
# `Coffee Mug`, `Gun Holster`.  Bisecting the game itself showed that lowering
# BOTH the verb and the object's Short makes the task fire, so measure each
# half here: a capitalised verb (px), a capitalised literal (PY), a capitalised
# wildcard (PZ), and a capitalised object Short behind a lowercase verb (pq).
task("PX %object%",   "PX PASS.")
task("PY",            "PY PASS.")
task("PZ *",          "PZ PASS.")
task("pq %object%",   "PQ PASS.")

# EVENTS -- none.
s(0)

# NPCS -- one, and it starts NOWHERE, as xfiles' NPC 12 does.
s(1)
s("Ghost")               # Name
s("")                    # Prefix
s(0)                     # V$Alias count
s("Not in play.")        # Descr
s(0)                     # StartRoom: nowhere
s("")                    # AltText
s(0)                     # Task
s(0)                     # Topics
s(0)                     # Walks
s(0)                     # ShowEnterExit
s("")                    # InRoomText
s(0)                     # Gender
# [4] Res: nothing, sound and graphics both being off.

s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4BURN.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
