#!/usr/bin/env python3
"""ADRIFT 4.0 probe: what does dobattle's " with " scan do with a CROWD?

Scarier models the scan on run390 44CD63-44CE44 / run400 47EC16 as "walk every
object the text after ' with ' names, no break, last weapon wins"
(lib_battle_scan_with()), measured on thesorc, a 3.90 game.  Illegal Socks
(4.00) contradicts that at 4.0: with the Cool Sword (Short "Sword", Alias
"Sword") and the Awesome Sword (Short "Sword", Alias "Sword2") both held,
run400 strikes for `attack dr myanus hurts with sword2` -- one object named --
but refuses `with sword`, `with cool sword` and `with awesome sword`, each of
which names both, with no swing and no turn (Adrift_305_socks6 /
Adrift_306_socks7, 2026-09-27).

This probe isolates the rule without Illegal Socks' other oddities: three
weapons held, one gargoyle to hit.

    object 0  "a cool" sword       alias "sword"    HitValue 30
    object 1  "an awesome" sword   alias "sword2"   HitValue 40
    object 2  "a" club             no alias         HitValue 50

Result (Wine, run400x, 2026-09-27; Adrift_305_battlew2.txt and
Adrift_305_battlew3.txt are identical turn for turn, so the multi-word Prefix
makes no difference):

    attack gargoyle with sword          I don't understand what you want me to
                                        do with [the] cool sword.   [no swing]
    attack gargoyle with cool sword     same
    attack gargoyle with awesome sword  Player hit Gargoyle with [the] awesome
                                        sword.                      [hit 40]
    attack gargoyle with sword club     Player hit Gargoyle with the club. [50]
    attack gargoyle with sword2         ... with [the] awesome sword.      [40]
    attack gargoyle with club           ... with the club.                 [50]

So 4.0 puts the weapon term through the ordinary 4.0 object scorer (Short as a
whole word = 1, first matching Alias +1, +1 per matching Prefix word) and lets
the strict maximum decide -- it never walks the crowd, and "last weapon wins"
never happens:

  * a strict winner that is a usable weapon is wielded and swung (`sword2`,
    `club`; `sword2` in Illegal Socks);
  * a strict winner that is NOT usable ends the command with the OBJECT
    catch-all "I don't understand what you want me to do with X." and takes no
    turn (Illegal Socks `with full suit of armor`, which is not a weapon);
  * a tie leaves no object at all and the command ends with the CHARACTER
    catch-all "I don't understand what you want to do with <NPC>.", again with
    no turn (Illegal Socks `with armor` -- two Armors at 1 each -- and `with
    awesome sword`, Adrift_306_socks7.txt).

Scarier instead asks "Which Sword?  Cool Sword or Awesome Sword?" and swings
on the repeat, so at 4.0 lib_battle_scan_with() over-fires.

Two cases here looked wrong when this probe was first run and are now
explained; see make_400_battlewith5probe.py, which bisects them and states the
measured 4.0 rule.  In short: `with cool sword` names a carried weapon and is
still refused because the Cool Sword matched through an Alias that is the
Awesome Sword's Short, which disqualifies it; and `with awesome sword` swings
here but not in Illegal Socks because Illegal Socks capitalises its Shorts,
and run400 compares the Short to the input case-sensitively, so there the
Awesome Sword is not a candidate at all.

Commands: v4_full_rerun_cmds/battlewith2.txt.

Build:
    python3 make_400_battlewith2probe.py p4BATTLEW2.plain
    python3 taftool.py pack p4BATTLEW2.plain p4TAKE.taf p4BATTLEW2.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# ---- HEADER ----
ml("Battle with-crowd probe.")
s(0)                                        # StartRoom
ml("Won.")

# ---- GLOBAL ----
s("Battle With Crowd Probe")
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

s(3)
obj("a cool", "sword", ["sword"], "A cool sword.", 1, 2, 30)
obj("an awesome", "sword", ["sword2"], "An awesome sword.", 1, 2, 40)
obj("a", "club", [], "A plain club.", 1, 2, 50)

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
s(500); s(500)                              # Stamina
s(8);  s(8)                                 # Strength
s(10); s(10)                                # Accuracy
s(3);  s(3)                                 # Defense
s(4);  s(4)                                 # Agility
s(0); s(0); s(0); s(0)                      # Speed KilledTask Recovery StaminaTask

# ---- tail ----
s(0); s(0); s(0); s(0); s(0)                # RoomGroups Synonyms Variables ALRs CustomFont
s("2026")

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4BATTLEW2.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
