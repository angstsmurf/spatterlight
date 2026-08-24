#!/usr/bin/env python3
"""ADRIFT 4.0 probe: WHEN does a follow-the-player walk announce its arrival?

sophie.taf (4.00) has Grumble on a one-stop "follow the player", Times=1 walk
with ShowEnterExit on, so every tick is an arrival tick and every player move
drags him along.  The Wine transcript of its first fifty commands announces

    Grumble complaining of beer deprivation staggers in from the <direction>.

on twenty-eight of those turns but stays silent on eight of them, and the eight
are all turns whose player move came from a TASK's move-player action rather
than from the library's own go handler.  Not all of them, though: "kill scab"
and "talk to shamuel" are task moves that DO announce, so "a task moved the
player" cannot be the whole rule.  Comparing the tasks involved, the ones that
announce put their move-player action LAST while the silent ones put it first
and run more actions after it -- a difference no reading of the decompiled walk
tick (run400 loc_468841, whose arrival gate is only "ShowEnterExit AND old <>
playerroom AND old <> 0") predicts.  So measure it.

One NPC, Bob, on exactly sophie's walk: one stop, Rooms = 1 (follow player),
Times = 1, ShowEnterExit on.  Two rooms, Alpha and Bravo, joined east/west, and
a cell per shape of "get the player from Alpha to Bravo":

    e     the library's own go handler -- the baseline that announces
    t1    CompleteText, ShowRoomDesc = Bravo, one action: move the player
    t2    the same, but a second action (n = 1) AFTER the move
    t3    the same as t1 with NO ShowRoomDesc
    t4    the same as t1 plus an AdditionalMessage, which prints after the
          room description
    t5    the same as t1 with the move-player action LAST, after n = 1
    t6    no ShowRoomDesc, an AdditionalMessage, move-player only

Every cell is followed by a library "w" back to Alpha, which is another
announcing baseline and re-arms the next cell.  Reading the transcript, an
announcement after a cell says the walk tick still printed its arrival; a
silence says the Runner swallowed it, and the cells then say which of
ShowRoomDesc, AdditionalMessage or action ORDER is what does the swallowing.

Usage:
    python3 make_400_walkannounceprobe.py p4WALKANN.plain
    python3 taftool.py pack p4WALKANN.plain <donor.taf> p4WALKANN.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Walk announce probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Walk Announce Probe 400")
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

# ROOMS -- Alpha east to Bravo, Bravo west to Alpha.  Exit order is
# north, east, south, west, up, down, in, out; a zero means no exit, and a
# real one is Dest (1-based) Var1 Var2 Var3.
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

# OBJECTS -- three takeable trinkets in Alpha, so the object-move cells have
# something real to move.
def obj(short):
    s("a")               # Prefix
    s(short)             # Short
    s(0)                 # V$Alias count
    s(0)                 # Static
    s("A trinket.")      # Description
    s(4)                 # InitialPosition: 4 + room -> Alpha
    s(0); s(0); s("")    # Task, TaskNotDone, AltDesc
    s(0); s(0); s(0)     # Container, Surface, Capacity
    s(0); s(0); s(0)     # Wearable, SizeWeight, Parent
    s(0); s(0); s(0); s(0); s(0)   # Openable, SitLie, Edible, Readable, Weapon
    s(0); s(0)           # CurrentState, ListFlag
    s(""); s(0)          # InRoomDesc, OnlyWhenNotMoved

s(3)
obj("coin")
obj("ring")
obj("gem")

# TASKS
def task(cmd, text, addmsg="", srd=0, actions=None, where=None, repeat=1):
    s(1); s(cmd)
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s(addmsg)            # AdditionalMessage
    s(srd)               # ShowRoomDesc: a 1-based room, or 0 for none
    s(repeat)            # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    if where is None:
        s(3)             # Where: all rooms
    else:
        s(1); s(where)   # Where: that one room (0-based)
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
# Action type 3 = change variable: Var1 = variable, Var2 = 0 ("Var ="),
# Var3 = the value, then $Expr and #Var5.
SET_N = (3, 0, 0, 1, "", 0)
# Action type 0 = move object: Var1 = 3 + the dynamic object, Var2 = 4
# ("held by"), Var3 = 0 (the player) -- sophie task 1740's crucifix shape.
def take(dynamic):
    return (0, 3 + dynamic, 4, 0)
# Action type 5 = execute task: Var1 = 0 ("execute"), Var2 = the task -- sophie
# task 576's shape, which is how its rock scene gets printed.
def run(task_index):
    return (5, 0, task_index)
# Action type 1 with Var1 = 2 + the NPC and Var2 = 2 is "move that NPC to the
# player's room", the action sophie's "+ grumble follows" task carries.
BOB_TO_PLAYER = (1, 2, 2, 0)

s(20)
task("t1", "T1.<br><br>", srd=2, actions=[TO_BRAVO])
task("t2", "T2.<br><br>", srd=2, actions=[TO_BRAVO, SET_N])
task("t3", "T3.<br><br>", srd=0, actions=[TO_BRAVO])
task("t4", "T4.<br><br>", addmsg="AM4.", srd=2, actions=[TO_BRAVO])
task("t5", "T5.<br><br>", srd=2, actions=[SET_N, TO_BRAVO])
task("t6", "T6.<br><br>", addmsg="AM6.", srd=0, actions=[TO_BRAVO])
task("t7", "T7.<br><br>", srd=2, actions=[TO_BRAVO, take(0)])
task("t8", "T8.<br><br>", srd=2, actions=[TO_BRAVO, run(12)])
task("t9", "T9.<br><br>", srd=2, actions=[TO_BRAVO, BOB_TO_PLAYER])
task("t10", "T10.<br><br>", srd=2,
     actions=[TO_BRAVO, SET_N, take(1), SET_N])
task("t11", "T11.<br><br>", srd=2, actions=[TO_BRAVO, run(13)])
task("t12", "T12.<br><br>", addmsg="<br><br>AM12.", srd=2,
     actions=[TO_BRAVO, SET_N, take(2), SET_N])
task("t13", "T13.<br><br>", srd=2, actions=[TO_BRAVO, run(17)])
task("t14", "T14.<br><br>", srd=2, actions=[TO_BRAVO, run(18)])
task("t15", "T15.<br><br>", srd=2, actions=[TO_BRAVO, run(19)])
task("sil", "")          # 12 -- silent, the task t8's action executes
task("tex", "TEX scene.")  # 13 -- prints, the task t11's action executes
# 17 -- sophie task 581's exact shape: room-restricted to the room the move
# lands in, not repeatable, its text a paragraph of its own.
task("rbravo", "<br><br>R13 scene.", where=1, repeat=0)
# 18 -- the same, but restricted to the room the player LEFT.
task("ralpha", "<br><br>R14 scene.", where=0, repeat=0)
# 19 -- unrestricted but not repeatable.
task("rnorep", "<br><br>R15 scene.", repeat=0)

# EVENTS -- none.
s(0)

# NPCS -- one, on sophie's walk.
s(1)
s("Bob")                 # Name
s("")                    # Prefix
s(0)                     # V$Alias count
s("A patient walker.")   # Descr
s(1)                     # StartRoom (1-based): Alpha
s("")                    # AltText
s(0)                     # Task
s(0)                     # Topics
s(1)                     # Walks
s(1)                     # NumStops
s(1)                     # Loop
s(0)                     # StartTask: none, so the walk runs from the start
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
s("Bob is here.")        # InRoomText
s(0)                     # Gender
# [4] Res: nothing, sound and graphics both being off.

s(0); s(0)               # RoomGroups, Synonyms

# VARIABLES
s(1)
s("n"); s(0); s("0")

s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4WALKANN.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
