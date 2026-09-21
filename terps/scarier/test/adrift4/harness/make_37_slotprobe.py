"""ADRIFT 3.7 command-slot probe: the game's own words for built-in 10-14.

A 3.70 file carries a fixed 17-string COMMAND block in place of 3.80's
synonyms table, and run370 tests five of those words beside its own
spellings, whole-word and anywhere in the line (c() over the line):

    slot 10  examines  434E2A  c("x") Or ... Or c(MemVar_4460FC(&HA)) Or c("read")
    slot 11  takes     435E28  c("get") Or c("take") Or c("pick")
                               Or c(MemVar_4460FC(&HB)) And Not c("from")
    slot 12  drops     430475  c("drop") Or c("put down") Or c("leave")
                               Or c(MemVar_4460FC(&HC)) And (MemVar_4460E3 = 0)
    slot 13  wears     42C533  c("wear") Or c("put on") Or ... Or c(&HD)
    slot 14  removes   4295FF  c("remove") Or c("take off") Or c(&HE)

(slot 15, goto, is lib_goto_alias(); slots 8/9 look/inventory are whole-line
equalities.)  run370 has no synonym table, so the typed word reaches the
tasks unchanged.  Scarier instead turns a renamed slot into a whole-word
synonym at load (parse_fixup_v370's |V370_GLOBAL:_Synonyms_|), rewriting the
author's word back to the standard one before anything else sees the line.
This world is built to show where the two part:

  * tasks spelled with the author's word and with the standard one
    (`grab blip` / `pick up blip`, `peer at sky` / `examine sky`);
  * the take slot's `Not c("from")` binding to the slot word alone
    (`grab coin from box`);
  * the word standing anywhere (`blorp grab stone`, `stone grab`);
  * an object whose name holds the word (`grab bag`, alias `bag`);
  * bare words, and the standard spellings still working.

Renamed: 10 peer, 11 grab, 12 dump, 13 don, 14 doff.

    Lit Room  --n-->  Cave  (--s--> back)

    1 stone     loose in the Lit Room
    2 hat       loose, wearable
    3 box       STATIC container, open
    4 coin      inside the box
    5 grab bag  loose, alias "bag"

p37SLOT2 (--namesakes) adds the two cases the two-verb pass leaves alone,
a static object (the box) under two handler words and a name two objects
answer to:

    6 red ball  loose, alias "ball"
    7 blue ball loose, alias "ball"

Usage:
    python3 make_37_slotprobe.py              -> p37SLOT.taf
    python3 make_37_slotprobe.py --namesakes  -> p37SLOT2.taf
Session (from ~/adrift-battle/runner/wine):
    cmdfile_pslot37.txt on run370x.exe (p37SLOT)
    cmdfile_pslot37b.txt on run370x.exe (p37SLOT2)
"""
import sys

import make_surfprobe as surf

LIT = surf.LIT

surf.OBJECTS = [
    ("stone",    "a", "A grey stone.",   ("room", LIT),  "",          0, 0, 0),
    ("hat",      "a", "A felt hat.",     ("room", LIT),  "",          0, 0, 0),
    ("box",      "a", "A wooden box.",   ("room", LIT),  "container", 5, 1, 1),
    ("coin",     "a", "A gold coin.",    ("in", "box"),  "",          0, 0, 0),
    ("grab bag", "a", "A lucky bag.",    ("room", LIT),  "",          0, 0, 0),
]
NAMESAKES = "--namesakes" in sys.argv[1:]
if NAMESAKES:
    surf.OBJECTS += [
        ("red ball",  "a", "A red ball.",  ("room", LIT), "", 0, 0, 0),
        ("blue ball", "a", "A blue ball.", ("room", LIT), "", 0, 0, 0),
    ]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {"grab bag": "bag", "red ball": "ball", "blue ball": "ball"}
surf.WEARABLE = {"hat"}
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("grab blip", "GRABBLIP."),
    ("pick up blip", "PICKUPBLIP."),
    ("peer at sky", "PEERSKY."),
    ("examine sky", "EXAMSKY."),
]
surf.COMMANDS_370 = list(surf.COMMANDS_370)
for slot, word in ((10, "peer"), (11, "grab"), (12, "dump"), (13, "don"),
                   (14, "doff")):
    surf.COMMANDS_370[slot] = word
surf.OUT = {370: "p37SLOT2.taf" if NAMESAKES else "p37SLOT.taf"}

if __name__ == "__main__":
    surf.emit(370)
