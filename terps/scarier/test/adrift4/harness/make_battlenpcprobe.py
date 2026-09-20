#!/usr/bin/env python3
"""Namesake characters with the Battle System ON, at 3.90 and 4.00.

`make_3738_npcambprobe.py` closed the pre-4.0 namesake question for
characters(): no Runner asks "Please be more clear, who do you want to
<verb>?", each arm just takes the first or the last NPC the line names.  It
could not reach dobattle, though, because its world has the Battle System
off, and with battle ON the attack verbs never get as far as characters():
run390 44CC1C and run400 47EB2D take the line themselves.  So scarier still
asks SCARE's question for `attack guard` with two guards in the room, at
every version -- `lib_battle_attack_bare()` passes NPC_PICK_ASK whenever
battle is on.  This is the world that settles it.

    Lit Room: Dave; Ann (female) and Bob, both "a guard"; a sword (a
              weapon, held), a club (a weapon, on the floor) and a stone
    Cave:     Cora, "a guard" too -- unseen until you go north

Every character is attitude 0 (neutral), so nobody strikes back and the
world is the same at every cell; stamina is 9999 and the blows take single
digits, so nobody dies either.  Exactly one weapon is carried, which is what
keeps the Runner's two-or-more-weapons question (lib_battle_weapon_question)
out of the transcript -- the club is deliberately left on the floor.

Usage:
    python3 make_battlenpcprobe.py [390|400|all]
Session (from ~/adrift-battle/runner/wine):
    ./fast.sh p39BATT.taf cmdfile_pbatt.txt run390x.exe
    ./fast.sh p4BATT.taf  cmdfile_pbatt.txt run400x.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("sword", "a", "A short sword.", ("held",),      "", 0, 0, 0),
    ("club",  "a", "A heavy club.",  ("room", LIT),  "", 0, 0, 0),
    ("stone", "a", "A grey stone.",  ("room", LIT),  "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEAPONS = {"sword", "club"}
surf.TASKS = [("probe", "PROBE OK.")]
surf.NPCS = [
    ("Dave", "", "", "A quiet man.", LIT, "Dave is here.",
     [("key", "DAVE KEY.")], 0),
    ("Ann", "a", "guard", "A tall guard.", LIT, "Ann is here.",
     [("key", "ANN KEY.")], 1),
    ("Bob", "a", "guard", "A short guard.", LIT, "Bob is here.",
     [("key", "BOB KEY.")], 0),
    ("Cora", "a", "guard", "A third guard.", CAVE, "Cora is here.",
     [("key", "CORA KEY.")], 0),
]

surf.BATTLE = 1
# The player hits for something and is never hit back.
surf.BATTLE_PLAYER = dict(stamina=9999, strength=5, defence=99, accuracy=9,
                          agility=9, recovery=0)
surf.BATTLE_NPC = dict(attitude=0, stamina=9999, strength=1, defence=0,
                       accuracy=1, agility=1, speed=1, killed_task=0,
                       recovery=0, stamina_task=0)
surf.BATTLE_OBJECTS = {"sword": dict(hit=3), "club": dict(hit=2)}

surf.OUT = {390: "p39BATT.taf", 400: "p4BATT.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
