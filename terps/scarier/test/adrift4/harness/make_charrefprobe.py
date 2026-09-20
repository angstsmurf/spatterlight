#!/usr/bin/env python3
"""A task command's %character%: which characters may bind it.

The %object% half of this is measured and ported -- run400's task matcher
skips any object whose seen byte is clear (458E6C, gate on [48]) and run390's
checktask does the same at 44ABEA -- and `uip_match_entity()` carries it as
`uip_strict_reference && !is_character && !gs_object_seen(...)`.  The
character half has never been measured, and the TODO has carried it as an
open lead twice ("the NPC seen gate for %character% (xfiles `look up
byers`)").

Read first, so the probe knows what to ask:

  * run400's %character% matcher (468DFC, loop 469162) gates the NPC array
    on `CInt(npc.global_26) = 1` -- the NPC's own SEEN byte, field 26, the
    field npc_in_command() tests as var_DC(26) -- and on NOTHING ELSE.  No
    room test.  So a character the player has met and walked away from
    should still bind a task's %character%, while one never met should not.
    It tries the Name (4691A9) and then each Alias (4691F8), both LCase()d.

  * run390's checktask (44AD48 and again at 44B323) loops the NPC array with
    no gate at all and tests `c(Name)` only -- no alias, no seen byte, no
    room.  So 3.9 should bind a character who is nowhere at all, and should
    NOT bind one by an alias.

The world, deliberately lit, with two rooms so a character can be met and
then left behind:

    Lit Room  --n-->  Cave  (--s--> back)

    NPCs:
      0 Dave  no alias      in the Lit Room -- present and seen from turn 1
      1 Eve   alias "spook" in the Cave     -- absent AND unseen until `n`
      2 Fay   no alias      nowhere (room 0) -- never seen, never present

    Objects: one rock in the Lit Room, so the world is not empty and the
    library has something to fall through to.

    Tasks:
      0 `probe`            -> "PROBE OK."   (must answer at both ends)
      1 `frob %character%` -> "FROBBED %character%."

The cells, in feed order.  `frob` is in no Runner's vocabulary, so a cell
that does not reach the task falls to the character catch-all or to
DontUnderstand, and either way the two engines must agree:

    frob dave   present, seen        -- binds everywhere
    frob eve    absent, UNSEEN       -- 4.0 must miss, pre-4.0 must bind
    frob spook  alias, absent/unseen -- 4.0 misses (unseen), 3.9 misses (alias)
    frob fay    nowhere, unseen      -- 4.0 must miss, 3.9 must bind
    n                                -- meet Eve
    frob eve    present, seen        -- binds everywhere
    frob dave   absent but SEEN      -- the gate's shape: seen-only says yes
    s
    frob eve    absent but SEEN      -- the KEY cell for 4.0
    frob spook  alias, absent, seen  -- 4.0 binds by alias, 3.9 does not

Usage:
    python3 make_charrefprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    ./fast.sh p4CHREF.taf  cmdfile_pchref.txt run400x.exe
    ./fast.sh p39CHREF.taf cmdfile_pchref.txt run390x.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("rock", "a big", "A grey rock.", ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEAPONS = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("frob %character%", "FROBBED %character%."),
    ("nurb %object%", "NURBED %object%."),
]
surf.NPCS = [
    ("Dave", "a big", "",  "A quiet man.",   LIT,  "Dave is here.",  [], 0),
    ("Eve",  "", "spook", "A quiet woman.", CAVE, "Eve is here.",   [], 1),
    ("Fay",  "", "",      "A missing woman.", 0,  "Fay is here.",   [], 1),
]
surf.BATTLE = 0
surf.OUT = {370: "p37CHREF.taf", 380: "p38CHREF.taf",
            390: "p39CHREF.taf", 400: "p4CHREF.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
