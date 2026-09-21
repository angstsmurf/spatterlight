#!/usr/bin/env python3
"""Where does dobattle sit in generaltasks' call order?

The two-verb entries in WINE-TRANSCRIPTS-TODO ("Below 4.0 a two-verb line is
decided by the call order", "At 4.0 a two-verb line is decided by a
DIFFERENT call order") left dobattle alone because nothing had measured it.
Both Runners call it as a plain `Call` straight after removes (run390
45F4AF, run400 48A4A2), gated only on the Battle System byte, so it can
never claim: a claiming handler ABOVE it (takes/drops/inventory/insides/
tasks at 3.9; put_drop_list/get_outer/tasks at 4.0) should keep it from
running at all, and a writer BELOW it (openclose, examines, whereis,
gotoplace, therest, characters) may overwrite or append to its blows.

The world is make_orderprobe.py's (hat held and wearable, coin loose, a
closed box holding a nut, Bob in the Lit Room with a "hat" topic, Cave to
walk to) with the Battle System ON.  Bob is neutral with stamina 9999, so he
never strikes back and never dies; nothing is a weapon, so every blow is a
punch and the weapon question never comes up.  Run the feed on the x
Runners under VBRNG=xoshiro: a blow is 4 draws, so the per-turn draw count
says whether dobattle struck even where its text was overwritten.

Usage:
    python3 make_battleorderprobe.py [390|400|all]
Session (from ~/adrift-battle/runner/wine):
    sh xoshiro_par.sh job_p2batt.txt 2
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("hat",  "a", "A felt hat.",   ("held",),      "",          0, 0, 0),
    ("coin", "a", "A gold coin.",  ("room", LIT),  "",          0, 0, 0),
    ("box",  "a", "A wooden box.", ("room", LIT),  "container",  5, 2, 0),
    ("nut",  "a", "A brass nut.",  ("in", "box"),  "",          0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {}
surf.SITLIE = {}
surf.WEARABLE = {"hat"}
surf.WEAPONS = set()
surf.EVENTS = []
surf.VARIABLES = []
surf.NPCS = [
    ("Bob", "", "", "An ordinary man.", LIT, "Bob is here.",
     [("hat", "Bob says, 'That is a fine hat.'")], 0),
]
surf.BATTLE = 1
surf.BATTLE_PLAYER = dict(stamina=9999, strength=5, defence=99, accuracy=9,
                          agility=9, recovery=0)
surf.BATTLE_NPC = dict(attitude=0, stamina=9999, strength=1, defence=0,
                       accuracy=1, agility=1, speed=1, killed_task=0,
                       recovery=0, stamina_task=0)
surf.TASKS = [("probe", "PROBE OK.")]
surf.OUT = {390: "p39BORD.taf", 400: "p4BORD.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
