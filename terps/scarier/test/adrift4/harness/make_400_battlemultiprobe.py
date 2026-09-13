#!/usr/bin/env python3
"""ADRIFT 4.0 probe: does one attack line strike every NPC it names?

run400's dobattle (Battles.bas 47EB0E-47F006) walks every NPC in index order
and, for each whose Name is a whole word of the line and who is in the
player's room, runs the whole attack branch -- weapon choice, "What do you
want to attack X with?", the blow -- and then falls through to `Next`
(47EF5B GoTo 47EFF8): nothing breaks the loop.  run390's twin (44CC1C-44D1D5)
has the same shape.  Read that way, a line naming two present NPCs strikes
both, in index order and not line order, and two NPCs sharing one Name are
both struck by a line naming it once.  Scarier instead binds one
%character% and asks "Please be more clear, who do you want to attack?".

One room, the player holding a blaster, a rock on the floor, and three
neutral NPCs with fat stamina and Speed 0 so nobody fights back or dies:

    0  Guard   "a guard"    alias "sentry"
    1  Droid   "a droid"
    2  Guard   "a guard"    same Name as NPC 0

Commands (probe_battlemulti.cmds):

     1  attack droid               control: one blow on the droid
     2  attack guard               both guards (0 then 2)?
     3  attack guard and droid     index order: guard 0, droid, guard 2?
     4  attack droid guard with blaster
     5  attack sentry              alias only: "Who do you want to attack?"
     6  attack sentry and droid    droid only (the alias names nothing)?
     7  take rock
     8  attack droid and guard     two weapons: one prompt per NPC?
     9  status

Build:
    python3 make_400_battlemultiprobe.py p4BATTLEMULTI.plain
    python3 taftool.py pack p4BATTLEMULTI.plain p4TAKE.taf p4BATTLEMULTI.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# ---- HEADER ----
ml("Battle multi-target probe.")
s(0)                                        # StartRoom
ml("Won.")

# ---- GLOBAL ----
s("Battle Multi Probe")
s("Scarier probe")
s("I don't understand.")
s(2)                                        # Perspective (second person)
s(1)                                        # ShowExits
s(0)                                        # WaitTurns
s(1)                                        # DispFirstRoom
s(1)                                        # BattleSystem ON
s(0)                                        # MaxScore
s("Player")
s(0)                                        # PromptName
s("A test fighter.")
s(0)                                        # Task
s(0)                                        # Position
s(0)                                        # ParentObject
s(0)                                        # PlayerGender
s(100)                                      # MaxSize
s(100)                                      # MaxWt
s(500); s(500)                              # StaminaLo/Hi
s(10);  s(10)                               # StrengthLo/Hi
s(60);  s(60)                               # AccuracyLo/Hi
s(5);   s(5)                                # DefenseLo/Hi
s(5);   s(5)                                # AgilityLo/Hi
s(0)                                        # Recovery
s(0)                                        # EightPointCompass
s(0)                                        # bNoDebug
s(0)                                        # NoScoreNotify
s(0)                                        # NoMap
s(0)                                        # bNoAutoComplete
s(0)                                        # bNoControlPanel
s(0)                                        # bNoMouse
s(0)                                        # Sound
s(0)                                        # Graphics
s(0)                                        # StatusBox
s("")                                       # StatusBoxText
s(0)                                        # iUnk1
s(0)                                        # iUnk2
s(0)                                        # Embedded

# ---- ROOMS ----
s(1)
s("Test Arena")
s("A bare arena for testing combat.")
for _ in range(8):
    s(0)
s(0)                                        # Alts count
s(0)                                        # bHideOnMap

# ---- OBJECTS ----
s(2)

def weapon(prefix, short, desc, init_pos, is_weapon,
           protection, hitvalue, method, accuracy):
    s(prefix); s(short); s(0); s(0)         # Prefix Short Aliases Static
    s(desc); s(init_pos); s(0); s(0); s("") # Descr InitPos Task TaskNotDone AltDesc
    s(0); s(0); s(0); s(0)                  # Container Surface Capacity Wearable
    s(2); s(0)                              # SizeWeight Parent
    s(0); s(0); s(0); s(0)                  # Openable SitLie Edible Readable
    s(is_weapon); s(0); s(0)                # Weapon CurrentState ListFlag
    s(protection); s(hitvalue); s(method); s(accuracy)
    s(""); s(0)                             # InRoomDesc OnlyWhenNotMoved

weapon("a", "blaster", "A laser blaster.", 1, 1, 0, 30, 3, 20)
weapon("a", "rock", "A heavy throwing rock.", 4, 1, 0, 12, 5, 10)

# ---- TASKS / EVENTS ----
s(0)
s(0)

# ---- NPCS ----
def npc(name, aliases, descr, inroom):
    s(name); s("a")
    s(len(aliases))
    for a in aliases:
        s(a)
    s(descr)
    s(1)                                    # StartRoom (room 0)
    s(""); s(0); s(0); s(0); s(0)           # AltText Task Topics Walks ShowEnterExit
    s(inroom)
    s(0)                                    # Gender
    s(1)                                    # Attitude (1 = neutral)
    s(500); s(500)                          # Stamina
    s(8);  s(8)                             # Strength
    s(10); s(10)                            # Accuracy
    s(3);  s(3)                             # Defense
    s(4);  s(4)                             # Agility
    s(0); s(0); s(0); s(0)                  # Speed KilledTask Recovery StaminaTask

s(3)
npc("Guard", ["sentry"], "The first guard.", "A guard stands by the door.")
npc("Droid", [], "A droid.", "A droid hovers here.")
npc("Guard", [], "The second guard.", "Another guard leans on the wall.")

# ---- tail ----
s(0); s(0); s(0); s(0); s(0)                # RoomGroups Synonyms Variables ALRs CustomFont
s("2026")

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4BATTLEMULTI.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
