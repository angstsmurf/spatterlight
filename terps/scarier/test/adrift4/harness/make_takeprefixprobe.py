#!/usr/bin/env python3
"""Which namesake 3.7's takes() and drops() skip, and what the Prefix does.

p37TASK measured the 3.7 loops with two hats, "a red hat" and "a blue hat":
`take hat` is "Take what?", because every loose namesake whose Prefix's
LAST WORD is missing from the line is marked and skipped, and with both
skipped nothing is left.  p*OPENA then measured the same shape with the
bare Prefix "a" on both objects -- and run370 took BOTH and the last one
spoke ("You pick up the rock."), exactly as openclose does (p37OPENA,
Adrift_232_oy370, 2026-09-20).  Scarier, which implements the p37TASK rule
literally, says "Take what?" there: the line does not contain "a" either.

Two readings fit both measurements:

    R1  the leading article is dropped from the Prefix first, and a Prefix
        that is nothing BUT an article filters nothing;
    R2  a one-word Prefix filters nothing, article or not.

They part company on a one-word Prefix that is not an article, so the
objects come in pairs of namesakes, one pair per Prefix shape, everything
loose in the one lit room:

    0 gem  "big"       1 gem  "small"     one word, no article -- R1 skips
                                          both and says "Take what?", R2
                                          takes both
    2 orb  "a"         3 orb  "a"         the p*OPENA control
    4 cog  "the"       5 cog  "the"       is "the" an article here too?
    6 pin  "old red"   7 pin  "new red"   two words with the SAME last
                                          word: `take pin` should skip
                                          both, `take red pin` neither

The feed (cmdfile_ptakepfx.txt) walks them in an order that never needs a
reset -- the bare line first, the qualified line second, and `i` after
each, since the loop's real signature is how many objects moved, not the
one message that survived:

    probe / look
    take gem / i / take big gem / i
    take orb / i
    take cog / i
    take pin / i / take red pin / i
    drop orb / i / drop red pin / i
    probe

p*TAKEP measured (2026-09-20, Adrift_238_pc370 .. 241_pc400):

  * 3.70 and 3.80 IGNORE the first word of the Prefix.  `take big gem`
    with "big gem" and "small gem" on the floor is "Take what?" at 3.70
    and "Which gem.  Big gem or small gem?" at 3.80, exactly as bare `take
    gem` is, and `take orb` / `take cog` (Prefix "a", Prefix "the") are
    the same -- a one-word Prefix tells nothing apart at all.  Only the
    pins, whose Prefixes are "old red" and "new red", have a word left
    after the first, and `take red pin` is answered.
  * with several survivors the pre-3.9 loop takes them ALL and the last by
    index speaks: `take red pin` is "You pick up new red pin." at 3.70 and
    3.80, `i` lists BOTH, and `drop red pin` then drops both.
  * 3.90 does not drop the first word (`take big gem` is "You pick up big
    gem.") and takes ONE object, the lowest index on a tie: `take red pin`
    is "You pick up old red pin." and `i` lists only it.
  * 4.00 does not drop the first word either, but a tie is a question:
    `take red pin` is "Which pin.  Old red pin or new red pin?" and `drop
    red pin` "It is not clear which pin you are referring to.".

p*TAKEQ (`three`) then asks whether "ignore the first word" is really the
first word or a leading ARTICLE, and whether what is kept is the SECOND
word or the LAST -- one-word and two-word Prefixes cannot tell those
apart.  Its four objects are

    0 gem "a very red"    1 gem "a very blue"    2 pin "big red"
                                                 3 pin "small red"

so at 3.70/3.80 `take very gem` answers iff the word kept is the second,
`take red gem` iff it is the last, and `take big pin` iff the word dropped
is an article rather than the first word whatever it is.  The feed is
cmdfile_ptakeq.txt.

p*TAKEQ measured (2026-09-20, Adrift_240_pd370 .. 243_pd400): the word
DROPPED is the first whatever it is, and the word KEPT is the last of what
remains.  `take red gem` answers at 3.70 ("You pick up a very red gem.")
and 3.80, `take very gem` is "Take what?" / the co() question, and `take
big pin` -- Prefixes "big red" and "small red", no article anywhere -- is
refused at 3.70, 3.80 AND 3.90, all three reading "red" out of both.  3.90
keeps the whole Prefix, so its `take very gem` answers where 3.70's does
not, and 4.00 asks about every tie.  So neither R1 nor R2: the rule is
positional, and an article is only the commonest first word.

Usage:
    python3 make_takeprefixprobe.py [three] [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    sh par.sh job_takepfx.txt 4
"""
import sys

import make_surfprobe as surf

LIT = surf.LIT

surf.OBJECTS = [
    ("gem", "big",     "A big gem.",    ("room", LIT), "", 0, 0, 0),
    ("gem", "small",   "A small gem.",  ("room", LIT), "", 0, 0, 0),
    ("orb", "a",       "A glass orb.",  ("room", LIT), "", 0, 0, 0),
    ("orb", "a",       "A steel orb.",  ("room", LIT), "", 0, 0, 0),
    ("cog", "the",     "A brass cog.",  ("room", LIT), "", 0, 0, 0),
    ("cog", "the",     "An iron cog.",  ("room", LIT), "", 0, 0, 0),
    ("pin", "old red", "An old pin.",   ("room", LIT), "", 0, 0, 0),
    ("pin", "new red", "A new pin.",    ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {}
surf.WEAPONS = set()
surf.TASKS = [("probe", "PROBE OK.")]
surf.NPCS = []
surf.BATTLE = 0
surf.OUT = {370: "p37TAKEP.taf", 380: "p38TAKEP.taf",
            390: "p39TAKEP.taf", 400: "p4TAKEP.taf"}


def three():
    """Three-word and adjective-first Prefixes, as p*TAKEQ.taf."""
    surf.OBJECTS = [
        ("gem", "a very red",  "A red gem.",   ("room", LIT), "", 0, 0, 0),
        ("gem", "a very blue", "A blue gem.",  ("room", LIT), "", 0, 0, 0),
        ("pin", "big red",     "A big pin.",   ("room", LIT), "", 0, 0, 0),
        ("pin", "small red",   "A small pin.", ("room", LIT), "", 0, 0, 0),
    ]
    surf.NAMES = [o[0] for o in surf.OBJECTS]
    surf.OUT = {370: "p37TAKEQ.taf", 380: "p38TAKEQ.taf",
                390: "p39TAKEQ.taf", 400: "p4TAKEQ.taf"}


if __name__ == "__main__":
    args = sys.argv[1:] or ["all"]
    if args[0] == "three":
        three()
        args = args[1:] or ["all"]
    arg = args[0]
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
