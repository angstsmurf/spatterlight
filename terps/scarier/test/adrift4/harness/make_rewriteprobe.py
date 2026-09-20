#!/usr/bin/env python3
"""The Runner's own built-in command rewrites, in all four file versions.

WINE-TRANSCRIPTS-TODO's "3.90 rewrites the line too, and Scarier stops at
3.80" lead.  Every Runner edits the typed line before anything -- task
matcher or library -- ever looks at it, but it does so through two different
mechanisms, and `scprintf.cpp` BUILTIN[] currently knows only the older one:

    run370 43B41F  change(line, "everything", "all")
           43B430  change(line, "slap", "hit")
    run380 441C3F  everything->all, 441C50 slap->hit,
           441C61  take->get,       441C72 except->but
    run390 45F225  Replace(line, "everything", "all", 1, -1, 0)
           45F246  slap->hit, 45F267 except->but, 45F288 "apart from"->"but"
    run400 48A30F  Replace(line, " everything ", " all ", 1, -1, 0)
           48A330  " slap "->" hit ",  48A351 " but "->" except ",
           48A372  " apart from "->" except "

4.0's literals carry their own spaces -- so its rewrites fire only in the
MIDDLE of a line -- and its exclusion word runs the OTHER WAY, but->except
where 3.90 goes except->but.  Neither 3.90 nor 4.00 touches `take`.

change() (run380 425634, run370 4203B0) is a loop over the WHOLE-WORD
matcher c(); VB's Replace() is a plain SUBSTRING replace.  So two things are
owed, and this probe measures both at once:

  1. Do the rows simply belong at 3.90 and 4.00?  BUILTIN[] caps
     everything/slap/except at TAF_VERSION_380 and has no "apart from" row
     at all, on the reading that 3.9+ "covers them by grammar alternatives
     instead".  If they belong, a task spelled `zog hit` must answer a typed
     `zog slap` at every version, and `zog apart from` must reach a `zog but`
     task at 3.90 -- and, if the 4.0 reading is right, there too.
  2. Does the substring reach show?  An object named `exception` is
     `bution` to a 3.9 Runner and still `exception` to a 3.8 one, so from
     3.90 it cannot be examined at all.  `zog unslap` tests the same thing
     away from the front of the word, since InStr is not anchored.
     (`slapper` becomes `hitper`, not `hitter`: the substring eats the
     stem, so it proves the reach by DISAPPEARING, not by reaching a task.)

The world is one lit room, so nothing the seen byte gates can confound a
cell:

    1 coin       "a",  the control -- no rewrite string anywhere in it
    2 exception  "an", disappears from 3.90 (`x exception` -> `x bution`)
    3 slapper    "a",  disappears from 3.90 (`x slapper` -> `x hitper`)

and thirteen unrestricted, all-rooms, repeatable tasks, whose commands are
the POST-rewrite spellings.  A task only ever runs when the rewrite it is
named for has fired:

    probe        PROBE OK.       both ends of the feed, so a shift shows
    zog hit      ZOGHIT.         typed straight, and via `zog slap`
    zog hitter   ZOGHITTER.      typed straight -- the control that shows
                                 `zog slapper` does NOT reach it
    zog unhit    ZOGUNHIT.       reached only from `zog unslap` (mid-word)
    zog but      ZOGBUT.         typed straight, via `zog except`, and via
                                 `zog apart from` (3.90 only)
    zog bution   ZOGBUTION.      reached only from `zog exception`
    zog all      ZOGALL.         typed straight, and via `zog everything`
    zog get      ZOGGET.         reached from `zog take` at 3.80 ONLY

The `... blip` tasks are the 4.00 half: 4.0's literals are space-bounded, so
only a keyword with a word on BOTH sides of it can be rewritten there.

    zog all blip    ZOGALLB.     via `zog everything blip` everywhere
    zog hit blip    ZOGHITB.     via `zog slap blip` everywhere, and via
                                 `zog SLAP blip` -- the case cell: Replace's
                                 compare is binary, so a hit proves the line
                                 was lower-cased before these ran
    zog but blip    ZOGBUTB.     3.70 typed straight; 3.80/3.90 also from
                                 `zog except blip`; 3.90 from `apart from`
    zog except blip ZOGEXCB.     3.70 typed straight; 4.00 reached from
                                 `zog but blip` and `zog apart from blip`
    all blip        ALLLEAD.     `everything blip` reaches it below 4.0 and
                                 not at 4.00, where the leading word has no
                                 space in front of it

`zog take` is the negative control for the one row that is already version
-gated correctly: take->get is run380's alone (441C61), so only 3.80 reaches
`zog get` from it.  The other three answer "Take what?" -- which Scarier does
NOT: it says "I don't understand."  That is the ONLY cell of this feed that
still differs, and it is a different rule, written up in
WINE-TRANSCRIPTS-TODO as "the library verb is matched anywhere in the line",
measured on these same four probes with cmdfile_pcasc.txt (Adrift_250_
casc37b.rtf, 249_casc38.rtf, 250_casc39.txt, 251_casc40.txt, 2026-09-20).

Usage:
    python3 make_rewriteprobe.py [370|380|390|400|all]

Session (from ~/adrift-battle/runner/wine), job_prew2.txt + cmdfile_prew2.txt:
    sh par.sh job_prew2.txt 4
-> Adrift_246_rew37b.rtf, 247_rew38b.rtf, 248_rew39b.txt, 249_rew40b.txt
   (2026-09-20).  cmdfile_prew.txt is the FIRST feed, and useless for 4.00:
   every keyword sat at end-of-line, where 4.0 rewrites nothing.
"""
import sys

import make_surfprobe as surf

LIT = surf.LIT

surf.OBJECTS = [
    ("coin",      "a",  "A gold coin.",   ("room", LIT), "", 0, 0, 0),
    ("exception", "an", "A rare exception.", ("room", LIT), "", 0, 0, 0),
    ("slapper",   "a",  "A leather slapper.", ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {}
surf.SITLIE = {}
surf.WEARABLE = set()
surf.WEAPONS = set()
surf.EVENTS = []
surf.NPCS = []
surf.VARIABLES = []
surf.BATTLE = 0
surf.TASKS = [
    ("probe",      "PROBE OK."),
    ("zog hit",    "ZOGHIT."),
    ("zog hitter", "ZOGHITTER."),
    ("zog unhit",  "ZOGUNHIT."),
    ("zog but",    "ZOGBUT."),
    ("zog bution", "ZOGBUTION."),
    ("zog all",    "ZOGALL."),
    ("zog get",    "ZOGGET."),
    ("zog all blip",    "ZOGALLB."),
    ("zog hit blip",    "ZOGHITB."),
    ("zog but blip",    "ZOGBUTB."),
    ("zog except blip", "ZOGEXCB."),
    ("all blip",        "ALLLEAD."),
]
surf.OUT = {370: "p37REW.taf", 380: "p38REW.taf",
            390: "p39REW.taf", 400: "p4REW.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
