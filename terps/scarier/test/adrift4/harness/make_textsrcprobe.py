"""WHICH string checktask's %object% walk searches, and in WHAT order.

The last open arm of the checktask lead in WINE-TRANSCRIPTS-TODO.md.  Every
Runner substitutes an object's name into a `%object%` command before it
tests the command (see make_wildrefprobe.py), but they do not all search the
same string for the name, and the walk's shape is not what Scarier assumed:

  * run380 43B6A3 saves the c() line global, sets `MemVar_44F0B0 = text` --
    checktask's own argument, the line as it stands NOW -- walks the object
    array once, and restores the global (43B77C / 43B80E).  ONE source.
    run370 433227 is the same shape (plus its sticky rewrite: 3.70 never
    restores the command record, see make_wildrefprobe.py).

  * run390 44AAD6 walks over a DIFFERENT string: `c(Short, MemVar_468224)`
    and `c(Alias, MemVar_468224)`, and only `If MemVar_4681A8 = &HFF` --
    nothing bound at all -- does it repeat the walk against `text` (44ABFE
    for the Short, 44AC98 for the Alias).  MemVar_468224 is a SNAPSHOT,
    written in exactly two places: Text1_KeyPress 436253, the raw typed line
    just after it is lower-cased, and generaltasks 45F20F, at the end of the
    synonym pass.  Everything generaltasks does to the line after that --
    "everything" -> "all" (45F225), "slap" -> "hit" (45F246),
    "except"/"apart from" -> "but" (45F267/45F288) and the `with ` history
    prepend (45F2AF-45F2D1) -- is invisible to the first pair, and so is
    every synthesised command a caller hands checktask (insides' per-object
    put sweep rebuilds "put <Short> <prep> <container>" at 4625BD/462652).

  * and the Short test and the Alias test are in the SAME `For var_138 ...
    Next var_138` loop (Next at 44ABE5): Short name 44AAEA, seen byte
    44AAFD, store 44AB0D, Replace 44AB40; then Alias name 44AB6D, seen
    44AB80, store 44AB90, Replace 44ABC3 -- and the Alias arm substitutes
    the ALIAS (`.global_8`), not the Short.  So the order is obj0.Short,
    obj0.Alias, obj1.Short, obj1.Alias, ... and NOT every Short then every
    Alias, which is what run_pre400_substitute_references() used to do.

The world is make_surfprobe's, five objects in the Lit Room, in this order
and no other -- the loops have no break, so the FIRST name found spells the
command and the LAST one found is what %object% expands to:

    0 gem   "a"   alias "stone"
    1 rock  "a"
    2 hit   "a"
    3 slap  "a"
    4 but   "a"

    Tasks:
      0 `probe`                     -> "PROBE OK."
      1 `blip %object%`             -> "FLAT [%object%]."
      2 `* zug * %object% *`        -> "WILD [%object%]."
      3 `zog %object%`              -> "SRC1 [%object%]."
      4 `nurb %object%`             -> "SRC2 [%object%]."
      5 `frob %object% with zzz`    -> "WITH1 [%object%]."
      6 `wibb %object% with rock`   -> "WITH2 [%object%]."

Two independent questions, two sets of cells.

ORDER.  `zug rock stone` and `zug stone rock` both name the gem through its
alias and the rock through its Short.  Interleaved (run390's real shape) the
gem's alias is found FIRST -- it spells the command `* zug * stone *` -- and
the rock is found LAST, so `%object%` prints the ROCK.  Short-pass-first
(Scarier's old shape) spells `* zug * rock *` and prints the GEM.  The two
orderings of the line prove the index order rules, not the word order.

SOURCE.  `with ` is void at 3.90 -- the Runner answers "With what?" and
routes the next lines through its pending-prefix handler -- so the lever is
the SYNONYM pass instead, the one rewrite that changes which OBJECT a line
names.  Objects `hit` (index 2) and `slap` (index 3) exist for it:

  CONTROL  `zog slap`
           snapshot "zog slap" names the slap and spells task 3 "zog slap";
           the argument is "zog hit" after 45F246, so the test fails.  A
           Runner that searches the ARGUMENT binds the hit, spells "zog hit"
           and matches.  CONTROL prints => argument-only.
  KEY      `nurb except`
           snapshot "nurb except" names NOTHING; the argument is "nurb but"
           after 45F267, which names the but (index 4).  A snapshot-only
           Runner refuses; snapshot-with-fallback and argument-only both
           print "SRC2 [a but]".
  Sanity   `zog hit` and `nurb but` -- both strings agree, both must print.

  So: KEY + no CONTROL = the snapshot with an argument fallback (the model
  the decompile predicts for run390); KEY + CONTROL = the argument alone
  (run370/run380/run400, whose synonym pass runs before checktask sees
  anything); no KEY = a snapshot with no fallback, which nobody predicts.

`zog`, `zug`, `nurb`, `blip`, `frob`, `wibb` and `zzz` are in no Runner's
vocabulary, so a cell that misses every task falls to the object catch-all
or to DontUnderstand and a hit is never in doubt.  (`hit` and `slap` ARE
verbs, but only as the first word of a line.)

The cells, in feed order (cmdfile_ptextsrc.txt):

    probe
    look                 stamp the seen byte on all five objects
    blip rock            the flat control: substitution works at all
    blip stone           the alias spells the command with the ALIAS
    zug rock stone       ORDER
    zug stone rock       ORDER, index order not word order
    zog hit              SOURCE sanity
    zog slap             CONTROL
    nurb but             SOURCE sanity
    nurb except          KEY
    probe
    frob rock            history, and a line no task claims
    with zzz             the 3.80/4.00 `with ` prepend, for the record
    wibb gem             history
    with rock            ditto
    blip gem             the flat control again, after every rewrite
    probe

Usage:
    python3 make_textsrcprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    TRANSCRIPT=Adrift_ts390.txt ./fast.sh p39TEXTSRC.taf \
        cmdfile_ptextsrc.txt run390x.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("gem",  "a", "A green gem.", ("room", LIT), "", 0, 0, 0),
    ("rock", "a", "A grey rock.", ("room", LIT), "", 0, 0, 0),
    ("hit",  "a", "A small hit.", ("room", LIT), "", 0, 0, 0),
    ("slap", "a", "A small slap.", ("room", LIT), "", 0, 0, 0),
    ("but",  "a", "A small but.", ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.ALIASES = {"gem": "stone"}
surf.WEAPONS = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("blip %object%", "FLAT [%object%]."),
    ("* zug * %object% *", "WILD [%object%]."),
    ("zog %object%", "SRC1 [%object%]."),
    ("nurb %object%", "SRC2 [%object%]."),
    ("frob %object% with zzz", "WITH1 [%object%]."),
    ("wibb %object% with rock", "WITH2 [%object%]."),
]
surf.NPCS = []
surf.BATTLE = 0
surf.OUT = {370: "p37TEXTSRC.taf", 380: "p38TEXTSRC.taf",
            390: "p39TEXTSRC.taf", 400: "p4TEXTSRC.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
