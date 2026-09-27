#!/usr/bin/env python3
"""ADRIFT 4.0 probe: WHY does run400 refuse a carried weapon it did resolve?

make_400_battlewith{2,3}probe.py left one case unexplained.  In both probes and
in Illegal Socks, run400 refuses `attack <npc> with cool sword` with the OBJECT
catch-all ("I don't understand what you want me to do with cool sword.", no
turn) even though the Cool Sword is carried, is flagged Weapon, and is the
strict winner of 4.0's name score -- while `with sword2` and `with club`, also
strict winners, are wielded and swung.

The one property the refused object has in both games, and the working ones do
not, is that its Alias DUPLICATES its own Short ("Sword"/"Sword").  This probe
tests that directly, on a weapon whose Short nothing else shares, with three
controls beside it:

    object 0  "cool" sword    alias "sword"   HitValue 30   Alias == Short
    object 1  "awesome" sword alias "sword2"  HitValue 40
    object 2  "a" club        alias "club"    HitValue 50   Alias == Short, and
                                                            Short is unique
    object 3  "a" axe         alias "axe2"    HitValue 60   control
    object 4  "a" mace        no alias        HitValue 70   control
    object 5  "a" pike        alias "spear"   HitValue 80   control

All six are held, all Weapon with Method 2, so `status`'s "wielding" line and
the (hit) bonus name whichever object the engine actually bound.  If the
Alias == Short reading is right, `with club` is refused with the object
catch-all while `with axe`, `with mace` and `with spear` all swing.

`with axe pike` and `with pike axe` are the second question: they score 1 each
and nothing else, so they say which way 4.0 breaks a tie -- last object named
(pike, 80), first (axe, 60), or the phrase's head noun (word order would make
the two commands differ).

The gargoyle has 9999 stamina so it survives every swing in the script.

Measured (run400x, Adrift_305_battlew4.txt): `with club`, `with axe`, `with
mace` and `with spear` ALL swing, so "Alias duplicates its own Short" is not
the disqualifier -- the real gate, settled in make_400_battlewith5probe.py, is
an Alias that is ANOTHER object's name.  `with sword` is still refused with the
object catch-all naming the cool sword.  The tie question is answered: `with
axe pike` and `with pike axe` both bind the pike, so 4.0 breaks a tie on the
last object in object order and ignores word order.

Commands: v4_full_rerun_cmds/battlewith4.txt.

Build:
    python3 make_400_battlewith4probe.py p4BATTLEW4.plain
    python3 taftool.py pack p4BATTLEW4.plain p4TAKE.taf p4BATTLEW4.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# ---- HEADER ----
ml("Battle with-alias probe.")
s(0)                                        # StartRoom
ml("Won.")

# ---- GLOBAL ----
s("Battle With Alias Probe")
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
def obj(prefix, short, aliases, descr, weapon, method, hit):
    s(prefix); s(short)
    s(len(aliases))
    for alias in aliases:
        s(alias)
    s(0)                                    # Static
    s(descr); s(1); s(0); s(0); s("")       # Descr InitialPosition Task TaskNotDone AltDesc
    s(0); s(0); s(0); s(0)                  # Container Surface Capacity Wearable
    s(2); s(0)                              # SizeWeight Parent
    s(0); s(0); s(0); s(0)                  # Openable SitLie Edible Readable
    s(weapon); s(0); s(0)                   # Weapon CurrentState ListFlag
    s(0); s(hit); s(method); s(20)          # Protection HitValue Method Accuracy
    s(""); s(0)                             # InRoomDesc OnlyWhenNotMoved

s(6)
obj("cool", "sword", ["sword"], "A cool sword.", 1, 2, 30)
obj("awesome", "sword", ["sword2"], "An awesome sword.", 1, 2, 40)
obj("a", "club", ["club"], "A plain club.", 1, 2, 50)
obj("a", "axe", ["axe2"], "A plain axe.", 1, 2, 60)
obj("a", "mace", [], "A plain mace.", 1, 2, 70)
obj("a", "pike", ["spear"], "A plain pike.", 1, 2, 80)

# ---- TASKS / EVENTS ----
s(0)
s(0)

# ---- NPCS ----
s(1)
s("Gargoyle"); s("")                        # Name, empty Prefix
s(0)                                        # no aliases
s("A stone gargoyle.")
s(1)                                        # StartRoom (room 0)
s(""); s(0); s(0); s(0); s(0)               # AltText Task Topics Walks ShowEnterExit
s("Gargoyle is here.")
s(2)                                        # Gender (it)
s(1)                                        # Attitude (1 = neutral)
s(9999); s(9999)                            # Stamina (survives every swing)
s(8);  s(8)                                 # Strength
s(10); s(10)                                # Accuracy
s(3);  s(3)                                 # Defense
s(4);  s(4)                                 # Agility
s(0); s(0); s(0); s(0)                      # Speed KilledTask Recovery StaminaTask

# ---- tail ----
s(0); s(0); s(0); s(0); s(0)                # RoomGroups Synonyms Variables ALRs CustomFont
s("2026")

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4BATTLEW4.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
