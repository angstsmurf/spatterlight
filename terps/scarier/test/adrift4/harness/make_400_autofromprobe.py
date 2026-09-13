#!/usr/bin/env python3
"""ADRIFT 4.0 probe: the take handler's auto-"from" rewrite and its pre-matches.

run400's get_piece (473A34) does three things scarier did not model when the
warlord T104 port was first tried (WINE-TRANSCRIPTS-TODO.md, "Analysed
2026-09-13: warlord T104"):

  1. 472DC8: with no whole-word "all"/"and" in the line it pre-matches the
     TYPED line (453C50 mode 1) and dispatches a hit; a return of 1 claims
     the line, a return of 2 (a restriction's FailMessage) prints and then
     get_piece CONTINUES into the take.
  2. 47301F: an object resolved in mode 1 that sits in or on a parent, with
     no typed "from", rewrites the line to `<line> from <parent>` and hands
     the parent to the take piece.
  3. 46302C: the take piece pre-matches `get <the object> from <the parent>`
     and dispatches the case-kept line; only a 1 exits the piece, a 2 falls
     through to the library take.

One room.  Static holders: a CLOSED container+surface desk, a stove, a sofa,
a plastic table, a "Mailbox on-a Rope" (capitals in the Short), a "crate
on-a cord", and a shelf.  Every dynamic object starts in or on one of them
except the medicine and the coin, which lie loose.

Tasks (index order matters -- the ticket row is T0 before T1):

   T0 [get]{the}[notepad]          loud fail (notepad held)   ticket task 113
   T1 get *desk*                   "T1 NO CAN DO."            ticket task 415
   T2 get *stove*                  "T2 BOLTED."               warlord task 2103
   T3 get * token from * table     silent restr (token ON table) humbug 192
   T4 get * rope                   loud fail (mail held)      professor task 7
   T5 get * cord                   loud fail, lowercase twin of T4
   T6 [get]{the}[pad]              no restriction             pattern anchoring
   T7 [get]{the}[medicine]         loud fail (medicine held)  typed-line hit 2
   T8 get the tin from *           only the rewritten form matches
   T9 probe                        control

Cells (a length-1 self-restarting event prints TICK. per counted turn):

   x desk / get notepad         T0's fail alone?  or T1's "no can do"?
   x stove / get treat          T2 via the rewrite (warlord)
   x table / Get token          T3 runs (restriction passes on the rewrite)?
   x mailbox / take mail        T4 pre-matches with a fail; case-kept
                                dispatch misses "Rope" -> library take?
   x crate / take letter        lowercase twin: T5's fail, and then?
   x shelf / get pad from shelf does [get]{the}[pad] match a longer line?
   get pad                      the rewrite against T6
   get medicine                 T7's fail on the typed line, then the take?
   x sofa / get tin and string  the and-loop: per-piece pre-match hits T8?
   get cup and plate            the and-loop with no task at all
   i                            what was actually taken

Measured in run400 (Adrift_p4autofrom.txt, 2026-09-13).  This corrects 1
and 2 above: 453C50 returns 1 for a hit with text or ANY fallback hit (a
restriction's fail message), and 2 only for a silent first-pass hit.

   get notepad         T0 STOPS YOU.  TICK.     typed pre-match claims first
   get treat           T2 BOLTED.  TICK.
   Get token           T3 TOKEN TASK.  TICK.
   take mail           NO IDEA.  (no TICK)      case-kept dispatch misses
   take letter         T5 CORD FAIL.  TICK.
   get pad from shelf  You take the pad from the shelf.  TICK.
   get pad             T6 PAD RAN.  TICK.
   get medicine        T7 MED FAIL.  TICK.
   get tin and string  You take the piece of string from the sofa.  T8 TIN.
   get cup and plate   You take the cup and the plate from the shelf.
   i                   a piece of string, a pad, a cup and a plate

warlord_autofrom_400.wip.patch reproduces every cell; see the parked note in
WINE-TRANSCRIPTS-TODO.md for why it is not applied (professor task 7).

Usage:
    python3 make_400_autofromprobe.py p4AUTOFROM.plain
    python3 taftool.py pack p4AUTOFROM.plain <donor.taf> p4AUTOFROM.taf
    (then, from ~/adrift-battle/runner/wine)
    sh fast.sh p4AUTOFROM.taf cmdfile_p4autofrom.txt run400.exe
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Auto-from probe.")
s(0)                     # StartRoom
ml("You have won.")

# GLOBAL
s("AutoFrom Probe 400")
s("SCARE probe")
s("NO IDEA.")
s(1)                     # Perspective
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

# ROOMS
def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0); s(0)           # Alts, HideOnMap

s(1)
room("Alpha", "The only room.", {})

# OBJECTS
def obj(short, room=0, static=False, prefix="a", aliases=(),
        container=0, surface=0, capacity=0, openable=0,
        position=None, parent=0):
    s(prefix); s(short)
    s(len(aliases))
    for a in aliases:
        s(a)
    s(1 if static else 0)
    s("A plain thing.")
    if static:
        s(0)
    else:
        s(room + 4 if position is None else position)   # InitialPosition
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    if static:
        s(1); s(room + 1)                # Where: ROOM_LIST1 Type 1, 1-based
    s(container); s(surface); s(capacity)
    if not static:
        s(0); s(0); s(parent)            # Wearable, SizeWeight, Parent
    s(openable)          # Openable: 0 none, 5 open, 6 closed
    if openable:
        s(0)             # Key
    s(0)                 # SitLie
    if not static:
        s(0)             # Edible
    s(0)                 # Readable
    if not static:
        s(0)             # Weapon
    s(0)                 # CurrentState
    s(0)                 # ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

#  idx  dyn cont surf
s(19)
obj("desk", static=True, prefix="the", container=1, surface=1,
    capacity=52, openable=6)                          #  0   -   0    0
obj("iron stove", static=True, prefix="the", surface=1, capacity=52)  # 1 - - 1
obj("sofa", static=True, prefix="the", surface=1, capacity=52)        # 2 - - 2
obj("table", static=True, prefix="the plastic", surface=1,
    capacity=52)                                      #  3   -   -    3
obj("Mailbox on-a Rope", static=True, prefix="the", aliases=("mailbox",),
    container=1, capacity=52)                         #  4   -   1    -
obj("crate on-a cord", static=True, prefix="the", aliases=("crate",),
    container=1, capacity=52)                         #  5   -   2    -
obj("shelf", static=True, prefix="the", surface=1, capacity=52)       # 6 - - 4
obj("notepad", position=3, parent=0)                  #  7   0        on desk
obj("treat", position=3, parent=1)                    #  8   1        on stove
obj("tin", position=3, parent=2, container=1, capacity=12,
    openable=6)                                       #  9   2   3    on sofa
obj("string", prefix="a piece of", position=3, parent=2)  # 10  3     on sofa
obj("token", position=3, parent=3)                    # 11   4        on table
obj("mail", position=2, parent=1)                     # 12   5        in mailbox
obj("letter", position=2, parent=2)                   # 13   6        in crate
obj("coin")                                           # 14   7        loose
obj("pad", position=3, parent=4)                      # 15   8        on shelf
obj("medicine")                                       # 16   9        loose
obj("cup", position=3, parent=4)                      # 17  10        on shelf
obj("plate", position=3, parent=4)                    # 18  11        on shelf

# TASKS
def task(cmds, text, restr=()):
    if isinstance(cmds, str):
        cmds = [cmds]
    s(len(cmds))
    for c in cmds:
        s(c)
    s(text)
    s(""); s(""); s("")  # Reverse/Repeat/Additional
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand
    s(3)                 # Where: anywhere
    s("")                # Question
    s(len(restr))
    for (v1, v2, v3, fail) in restr:
        s(0); s(v1); s(v2); s(v3); s(fail)   # Type 0: object location
    s(0)                 # Actions
    s("#" * len(restr))  # RestrMask

def held(dyn, fail):
    return (dyn + 3, 1, 0, fail)             # dynamic object held by player

s(10)
task("[get]{the}[notepad]", "T0 RAN.", [held(0, "T0 STOPS YOU.")])
task("get *desk*", "T1 NO CAN DO.")
task("get *stove*", "T2 BOLTED.")
task("get * token from * table", "T3 TOKEN TASK.",
     [(4 + 3, 5, 3 + 1, "")])                # token ON surface 3 (table)
task("get * rope", "T4 RAN.", [held(5, "T4 ROPE FAIL.")])
task("get * cord", "T5 RAN.", [held(6, "T5 CORD FAIL.")])
task("[get]{the}[pad]", "T6 PAD RAN.")
task("[get]{the}[medicine]", "T7 RAN.", [held(9, "T7 MED FAIL.")])
task("get the tin from *", "T8 TIN.")
task("probe", "PROBE OK.")

# EVENTS -- length 1, restarts immediately: FinishText marks every counted turn.
s(1)
s("Ticker")
s(1)                     # StarterType: immediate
s(1)                     # RestartType: restart immediately
s(0)                     # TaskFinished
s(1); s(1)               # Time1, Time2
s(""); s(""); s("TICK.") # StartText, LookText, FinishText
s(3)                     # Where: all rooms
s(0); s(0)               # PauseTask, PauserCompleted
s(0); s("")              # PrefTime1, PrefText1
s(0); s(0)               # ResumeTask, ResumerCompleted
s(0); s("")              # PrefTime2, PrefText2
s(0); s(0); s(0); s(0); s(0); s(0)   # Obj2/Dest Obj3/Dest Obj1/Dest
s(0)                     # TaskAffected

s(0)                     # NPCs
s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
args = [a for a in sys.argv[1:] if not a.startswith("--")]
out = args[0] if args else "p4AUTOFROM.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
