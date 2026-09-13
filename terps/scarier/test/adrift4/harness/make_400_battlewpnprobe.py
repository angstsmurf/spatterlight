#!/usr/bin/env python3
"""ADRIFT 4.0 probe: does "What do you want to attack X with?" take an answer?

dobattle's weapon question (run400 47ED0B) stores "attack " & LCase(Name) &
" with" in MemVar_494234 (47ED3E), the continuation the Who question uses.
The player holds two weapons (blaster, sword) and a rock, wields nothing, and
faces three neutral gargoyles with fat stamina and Speed 0.

Commands: v4_full_rerun_cmds/battlewpn.txt.

Build:
    python3 make_400_battlewpnprobe.py p4BATTLEWPN.plain
    python3 taftool.py pack p4BATTLEWPN.plain p4TAKE.taf p4BATTLEWPN.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# ---- HEADER ----
ml("Battle weapon-question probe.")
s(0)                                        # StartRoom
ml("Won.")

# ---- GLOBAL ----
s("Battle Weapon Probe")
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
s("Main Hall")
s("A massive hall with four stone pillars.")
for _ in range(8):
    s(0)
s(0)                                        # Alts count
s(0)                                        # bHideOnMap

# ---- OBJECTS ----
def obj(short, descr, weapon, method):
    s("a"); s(short); s(0); s(0)            # Prefix Short Aliases Static
    s(descr); s(1); s(0); s(0); s("")
    s(0); s(0); s(0); s(0)                  # Container Surface Capacity Wearable
    s(2); s(0)                              # SizeWeight Parent
    s(0); s(0); s(0); s(0)                  # Openable SitLie Edible Readable
    s(weapon); s(0); s(0)                   # Weapon CurrentState ListFlag
    s(0); s(30); s(method); s(20)           # Protection HitValue Method Accuracy
    s(""); s(0)                             # InRoomDesc OnlyWhenNotMoved

s(3)
obj("blaster", "A laser blaster.", 1, 3)
obj("sword", "A short sword.", 1, 0)
obj("rock", "A grey rock.", 0, 0)

# ---- TASKS / EVENTS ----
s(0)
s(0)

# ---- NPCS ----
def npc(name, descr, inroom):
    s(name); s("")                          # Name, empty Prefix (as Shadowpeak)
    s(0)                                    # no aliases
    s(descr)
    s(1)                                    # StartRoom (room 0)
    s(""); s(0); s(0); s(0); s(0)           # AltText Task Topics Walks ShowEnterExit
    s(inroom)
    s(2)                                    # Gender (it)
    s(1)                                    # Attitude (1 = neutral)
    s(500); s(500)                          # Stamina
    s(8);  s(8)                             # Strength
    s(10); s(10)                            # Accuracy
    s(3);  s(3)                             # Defense
    s(4);  s(4)                             # Agility
    s(0); s(0); s(0); s(0)                  # Speed KilledTask Recovery StaminaTask

s(3)
for n in (1, 2, 3):
    npc("Gargoyle #%d" % n, "A stone gargoyle.", "Gargoyle #%d is here." % n)

# ---- tail ----
s(0); s(0); s(0); s(0); s(0)                # RoomGroups Synonyms Variables ALRs CustomFont
s("2026")

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4BATTLEWPN.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
