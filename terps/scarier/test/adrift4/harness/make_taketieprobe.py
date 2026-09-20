#!/usr/bin/env python3
"""Which of 4.0's two tie answers a crowded `take` or `drop` reaches.

run400 has both strings and uses both.  p4TAKEP's two loose pins, Short
"pin" and Prefixed "old red" / "new red", answer `take red pin` with the
QUESTION -- "Which pin.  Old red pin or new red pin?" -- and `drop red pin`
with the FLAT refusal, "It is not clear which pin you are referring to."
(Adrift_241_pc400, 2026-09-20).  But p4OPENA's `take gem`, with the gem in
hand and a rock aliased "gem" on the floor, is the flat refusal too
(Adrift_235_oy400), so the split is not simply take-asks / drop-refuses.

Two differences could carry it, and that feed confounds them: whether the
tied objects are held or loose, and whether one of them answers by ALIAS
rather than by its Short.  (A third, the drop resolver's Me(424) quirk --
each tied object's Short compared with the object two indexes past the
previous tie, wilkins T110-T117 -- is why a filler object sits at the end
here, so that every pair has an index+2 to look at.)

So the world is five pairs, every pair Prefixed "a" so no Prefix word ever
narrows anything, everything in the one lit room:

    0 gem  loose   1 gem  loose            both loose, both by Short
    2 cog  HELD    3 cog  loose            one held, one loose, by Short
    4 pad  HELD    5 pad  HELD             both held, both by Short
    6 rock loose   7 orb  loose            both loose, the rock by ALIAS
    8 tin  HELD    9 pin  loose            p4OPENA's own shape: the held
                                           one by Short, the loose one by
                                           ALIAS
   10 slab loose                           filler, for index+2

The feed (cmdfile_ptaketie.txt) asks each pair both ways, with `i` between
the halves since a take that succeeds changes what the drop half sees:

    probe / i
    take gem / take cog / take pad / take orb / take tin / i
    drop gem / drop cog / drop pad / drop orb / drop tin / i
    probe

WHAT IT MEASURED (2026-09-20, Adrift_242_pe390 and Adrift_243_pe400):

    take gem   both loose, one Short    Which gem.  The gem or the gem?
    take cog   one held, one loose      You take the cog.
    take pad   both HELD, one Short     It is not clear which pad ...
    take orb   both loose, one by ALIAS It is not clear which orb ...
    take tin   held Short, loose alias  You take the pin.
    drop gem   both loose               It is not clear which gem ...
    drop cog   both held, one Short     Which cog.  The cog or the cog?
    drop pad   both held, one Short     Which pad.  The pad or the pad?
    drop orb   both loose, one by ALIAS It is not clear which orb ...
    drop tin   both held, one by ALIAS  It is not clear which tin ...

So neither difference carries it alone.  Every cell falls out of the
resolver already read off the drop side (lib_name_object_resolve_400 in
sclibrar.cpp), run in mode 1 for take: the question needs two tied objects
with the same Short, which is why every alias pair is flat, AND a tie the
verb's own pass made, because a pass-1 tie with more hits than pass 0
restores pass 0's Me(424) -- empty when the verb's side of the room held
nothing.  That is the whole of `take pad` vs `drop pad`: the same two held
pads, asked about by the verb that walks what is held.

It also settles p4OPENA's `take gem` (Adrift_235_oy400): the feed's earlier
`take gem` was itself ambiguous and took nothing, so by then the gem and the
rock aliased "gem" were BOTH loose -- the `take orb` cell, flat.

One more thing the drop half shows: name_object's prompt is not the
generaltasks scan's, which prints "That is still ambiguous!" instead of a
second prompt.  `drop cog` asks, and the `drop pad` after it asks in full.

3.90 (Adrift_242_pe390) has one answer and prints it ten times: co()'s
"Which gem.  The gem or the gem?", for alias pairs as readily as for
namesakes, for take and drop alike.  But the two `i` lines around the take
half disagree with it -- the loose cog and the loose pin ARE taken while
their prompt prints, the pads and the gems are not, and the drop half moves
nothing -- so the prompt is the end-of-turn co() question over a take that
had already happened for want of a rival its co(idx, 1) admitted.  Scarier
matches this feed turn for turn already.

Usage:
    python3 make_taketieprobe.py [390|400|all]
Session (from ~/adrift-battle/runner/wine):
    sh par.sh job_taketie.txt 2
"""
import sys

import make_surfprobe as surf

LIT = surf.LIT

surf.OBJECTS = [
    ("gem",  "a", "A green gem.",  ("room", LIT), "", 0, 0, 0),
    ("gem",  "a", "A red gem.",    ("room", LIT), "", 0, 0, 0),
    ("cog",  "a", "A brass cog.",  ("held",),     "", 0, 0, 0),
    ("cog",  "a", "An iron cog.",  ("room", LIT), "", 0, 0, 0),
    ("pad",  "a", "A soft pad.",   ("held",),     "", 0, 0, 0),
    ("pad",  "a", "A hard pad.",   ("held",),     "", 0, 0, 0),
    ("rock", "a", "A grey rock.",  ("room", LIT), "", 0, 0, 0),
    ("orb",  "a", "A glass orb.",  ("room", LIT), "", 0, 0, 0),
    ("tin",  "a", "A rusty tin.",  ("held",),     "", 0, 0, 0),
    ("pin",  "a", "A steel pin.",  ("room", LIT), "", 0, 0, 0),
    ("slab", "a", "A stone slab.", ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {"rock": "orb", "pin": "tin"}
surf.WEAPONS = set()
surf.TASKS = [("probe", "PROBE OK.")]
surf.NPCS = []
surf.BATTLE = 0
surf.OUT = {390: "p39TAKER.taf", 400: "p4TAKER.taf"}


if __name__ == "__main__":
    arg = (sys.argv[1:] or ["all"])[0]
    for v in ([390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
