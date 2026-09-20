#!/usr/bin/env python3
"""Which object `open X with Y` and `read X with Y` answer about below 4.0.

The lead left open by make_withprefixprobe.py.  With the gem held,
`open rock with gem` is

    3.70  "You can't open the rock with the gem."   the head, plus the suffix
    3.80  "You can't open the gem!"                 the instrument, and a bang
    3.90  "You can't open the gem!"                 ditto
    4.00  "You can't open the rock with the gem."   the head, plus the suffix

(p*WITHPFX, cmdfile_pwithpfx4.txt, Adrift_222_ws370 .. 225_ws400), and
`read rock with gem` is the examine ambiguity prompt "Which rock would you
like to examine.  The gem or the rock?" at 3.70/3.80, "You can't read the
gem!" at 3.90 and "You can't read the rock!" at 4.00.  Scarier answers the
4.0 wording about the wrong object at 3.70, asks "Please be more clear,
what do you want to open?" at 3.80/3.90, and says "Nothing special." to
every pre-4.0 read.

Two things were unknown, and " with " could not tell them apart: whether
3.80/3.90 name the instrument BECAUSE it is the instrument or simply
because openclose's walk keeps some other name, and what the 3.9 split
does with a STATIC instrument.

So the world is deliberately flat -- everything in the one lit room, so
every cell is about wording and never about scope:

    0 gem   "a",  dynamic          -- the instrument, taken on turn 3
    1 rock  "a",  dynamic          -- the object: not openable, not readable
    2 slab  "a",  STATIC           -- a static instrument
    3 chest "a",  dynamic CONTAINER, CLOSED, capacity 5
                                   -- a real open target, to see whether the
                                      clause pre-empts a handler that would
                                      otherwise DO something

plus the usual `probe` task at both ends.

The cells, in feed order (cmdfile_popenw.txt):

    open rock / read rock         the bare baselines in this world
    open rock gem / open gem rock TWO objects, no " with ": first or last?
    open rock with gem            the p*WITHPFX cell again, for continuity
    open rock with slab           a STATIC instrument
    open slab with gem            a static OBJECT with a held instrument
    read rock gem / read gem rock the same pair for the examine tail
    read rock with gem            continuity
    close rock with gem           the other half of openclose
    open chest with gem           does the clause pre-empt a real open?
    look                          ... and did the chest open?
    close chest with gem          and a real close
    examine rock with gem         examines() sits above therest pre-4.0

WHAT IT MEASURED (2026-09-20, Adrift_228_ow370 .. 235_oy400; feeds
cmdfile_popenw.txt, cmdfile_popenw2.txt, cmdfile_popena.txt; the second
feed adds `close rock`, `close rock gem`, `close gem rock`, `close rock
with slab`, `open slab rock`, `open rock gem chest`, `open zzz`, `read
slab`, `read rock with slab`, `examine rock gem`, `x gem rock`, `open
chest`, `close chest gem`, and the third runs p*OPENA, where the rock
also answers to "gem"):

  * "Please be more clear, what do you want to <verb>?" is in NONE of the
    four Runner exes, ASCII or UTF-16LE.  Every pre-4.0 line that named
    several objects used to get it out of Scarier; not one of them does in
    a Runner.
  * open acts on the one OPENABLE object the line names, whatever else it
    names: `open rock gem chest` is "You open the chest." at 3.70, 3.80
    and 3.90.  With none openable, 3.70 falls through to therest's tail --
    first name by WORD POSITION, full stop, " with <the X>" suffix kept
    (`open rock gem` -> rock, `open gem rock` -> gem, `open slab rock` ->
    slab, `open rock with slab` -> "the rock with the slab.") -- while
    3.80/3.90 have openclose's own refusal: lowest object INDEX, a bang,
    no suffix (`open rock gem` and `open gem rock` -> "the gem!", `open
    slab rock` and `open rock with slab` -> "the rock!").
  * close has no refusal of its own before 4.0, so all three versions use
    the therest tail: `close rock gem` -> rock, `close gem rock` -> gem,
    `close rock with slab` -> "You can't close the rock with the slab.".
  * a STATIC instrument therefore falls through to the suffix at 3.70 as
    the listing said, at every version: no "Don't be daft!" below 4.0.
  * read is examines()' object: 3.90 takes referencedob()'s last-word pass
    (`read rock gem` -> gem, `read gem rock` -> rock, `read rock with
    slab` -> slab), 3.70/3.80 ask "Which <Short of the last match by
    index> would you like to examine.  <matches in index order>?".
  * one noun naming TWO objects (p*OPENA) is co()'s own question "Which
    gem.  The gem or the rock?" at 3.80/3.90, for open, close, x, read and
    take alike, and it beats the openable-object rule (`open chest gem`
    asks instead of opening).  3.70 raises no co() question at all: `open
    gem` is "You can't open the gem." and `take gem` "You pick up the
    rock." -- and `i` then lists BOTH, takes() having taken each.
  * 4.00, for the record: `open rock gem chest` and `open chest gem` are
    "Which chest.  The gem, the rock or the chest?", `take gem` is "It is
    not clear which gem you are referring to.", and `open chest with gem`
    is the therest refusal, not an open.  See the open leads in
    notes/WINE-TRANSCRIPTS-TODO.md.

Usage:
    python3 make_openwithprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    sh par.sh job_openw.txt 4
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("gem",   "a", "A green gem.",   ("room", LIT), "",          0, 0, 0),
    ("rock",  "a", "A grey rock.",   ("room", LIT), "",          0, 0, 0),
    ("slab",  "a", "A stone slab.",  ("room", LIT), "",          0, 0, 1),
    ("chest", "a", "A wooden chest.", ("room", LIT), "container", 5, 2, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {}
surf.WEAPONS = set()
surf.TASKS = [("probe", "PROBE OK.")]
surf.NPCS = []
surf.BATTLE = 0
surf.OUT = {370: "p37OPENW.taf", 380: "p38OPENW.taf",
            390: "p39OPENW.taf", 400: "p4OPENW.taf"}

# The same world with the rock answering to "gem" as well, so that ONE typed
# noun names two objects: the only shape that should reach the Runners'
# "Please be more clear, what do you want to open?  The gem or the rock?",
# which Scarier prints for two DIFFERENT nouns on the line.  `amb` on the
# command line emits it as p*OPENA.taf.
AMBIGUOUS = {"rock": "gem"}


def amb():
    surf.ALIASES = AMBIGUOUS
    surf.OUT = {370: "p37OPENA.taf", 380: "p38OPENA.taf",
                390: "p39OPENA.taf", 400: "p4OPENA.taf"}


if __name__ == "__main__":
    args = sys.argv[1:] or ["all"]
    if args[0] == "amb":
        amb()
        args = args[1:] or ["all"]
    arg = args[0]
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
