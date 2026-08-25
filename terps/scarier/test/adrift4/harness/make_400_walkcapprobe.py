"""ADRIFT 4.0 probe: does the Runner CAPITALISE a walk announcement's Name?

make_400_walkalrprobe.py measured that the announcement is joined onto the
turn's text with pspace's two spaces rather than given a line of its own.  That
moves the sentence from the start of a line into the middle of a paragraph, and
SCARE used to call pf_new_sentence() before the Name, so the two behaviours
disagree for any NPC whose Name starts lower-case -- baroo.taf (4.00) has two,
Name "wizard" and Name "warlock", both with Prefix "the", and its golden read
"Wizard strides off to the east."

MEASURED 2026-08-25 (see below), and it is a version split; the listings say:

    run370 loc_43961A   buf = buf & Name & " " & EnterText & " from " & dir
    run380 loc_4417B0   the same
    run390 loc_45AA0F   the same
    run400 loc_468A79   buf = buf & Proc_21_3_446BB4(Name) & " " & EnterText
    run400 loc_4688E0   the same, in the departure half
    run400 loc_468CF9   the HIDDEN departure, raw Name -- 4.0 does not
                        capitalise there

where Proc_21_3_446BB4 (General.bas:75) is the Runner's one-line capitaliser,
UCase(Left(s, 1)) & Right(s, Len(s) - 1).  So 4.0 capitalises the Name at both
npc_announce() sites and nowhere else, and 3.7/3.8/3.9 never do.  Ported as the
is_400 pf_new_sentence() in npc_announce(); baroo keeps its capital, and a
pre-4.0 game in the same shape would not have had one.

The corpus agrees from the other side: circus.taf (3.90) names an NPC "Joe" and
its author wrote the ALR pair

    '  Joe'  ->  '  The vendor'          (the joined, sentence-initial case)
    'Joe'    ->  'the vendor'            (everywhere else)

i.e. supplied the capital by hand for exactly the position the two-space join
puts the Name in.

CONFIRMED LIVE 2026-08-25, Adrift_1_p4walkcap.txt, all four commands echoed.
The NPC's Name on disk is the lower-case "bob"; run400 prints "Bob" in every
cell, and the two shapes land the sentence in two different places:

    e     Bravo
          The second room.  You can only move west.  Bob wanders in from the west.
    w     Alpha
          The first room.  You can only move east.  Bob wanders in from the east.
    pb    PB.
          <blank>
          Bob wanders in from the west.
    pa    PA.
          <blank>
          Bob wanders in from the east.

so the `e`/`w` pair is the mid-paragraph join (pspace's two spaces after the
exits sentence) and the `pb`/`pa` pair is the Name opening a line, because the
CompleteText already ends in "<br><br>" and pspace adds nothing to a break.
The capital survives both -- 4.0 capitalises wherever the sentence lands, so
the listings' reading was right and nothing about it is positional.

Scarier reproduces all four cells verbatim; no change came out of this round.

    LOAD_SLEEP=22 sh measure.sh p4WALKCAP.taf cmdfile_walkcap.txt

Note: a task with an EMPTY CompleteText was the first shape tried for the
pb/pa cells, and Scarier answers "I don't understand." to its command.  That
is FAITHFUL, settled from the listings 2026-08-25: run400's typed-command task
dispatcher Proc_19_24_44CCE0 (mdlSpreadTheLoad.bas:21595) matches and runs the
task, then ends with "If MemVar_4941B0 = "" Then Result = 0" at loc_44CCC0 --
a turn that leaves the buffer empty is reported as not handled, and the caller
falls through to the unknown-command message.  See the Open leads entry in
notes/WINE-TRANSCRIPTS-TODO.md.  Hence the real CompleteText on pb/pa.

Usage:
    python3 make_400_walkcapprobe.py p4WALKCAP.plain
    python3 taftool.py pack p4WALKCAP.plain <donor.taf> p4WALKCAP.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Walk capitalisation probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Walk Cap Probe 400")
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
s(0); s(0)               # Sound, Graphics
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

s(0)                     # OBJECTS -- none

# TASKS
def task(cmd, text, srd=0, actions=None):
    s(1); s(cmd)
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(srd)               # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(3)                 # Where: all rooms
    s("")                # Question
    s(0)                 # Restrictions
    s(len(actions or []))
    for a in (actions or []):
        for field in a:
            s(field)
    s("")                # RestrMask

# Action type 1 = move: Var1 = 0 ("move player"), Var2 = 0 ("to room"),
# Var3 = the 0-based room.
TO_BRAVO = (1, 0, 0, 1)
TO_ALPHA = (1, 0, 0, 0)

s(2)
task("pb", "PB.<br><br>", srd=0, actions=[TO_BRAVO])
task("pa", "PA.<br><br>", srd=0, actions=[TO_ALPHA])

s(0)                     # EVENTS -- none

# NPCS -- one, on sophie's walk, with a lower-case Name.
s(1)
s("bob")                 # Name
s("the")                 # Prefix
s(0)                     # V$Alias count
s("A patient walker.")   # Descr
s(1)                     # StartRoom (1-based): Alpha
s("")                    # AltText
s(0)                     # Task
s(0)                     # Topics
s(1)                     # Walks
s(1)                     # NumStops
s(1)                     # Loop
s(0)                     # StartTask
s(0)                     # CharTask
s(0)                     # MeetObject
s(0)                     # ObjectTask
s(0)                     # StoppingTask
s(0)                     # MeetChar
s("")                    # ChangedDesc
s(1); s(1)               # Rooms[0] = 1 (follow the player), Times[0] = 1
s(1)                     # ShowEnterExit
s("wanders in")          # EnterText
s("wanders off")         # ExitText
s("bob is here.")        # InRoomText
s(0)                     # Gender

s(0); s(0)               # RoomGroups, Synonyms

s(0)                     # VARIABLES
s(0)                     # ALRS -- none, so nothing rewrites the Name

s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WALKCAP.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
