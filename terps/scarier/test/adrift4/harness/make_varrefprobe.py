#!/usr/bin/env python3
"""A task command's %<user variable>%, at 3.90 and 4.00.

The last arm of the checktask lead in WINE-TRANSCRIPTS-TODO.md.  Scarier
matches a %foo% command with the PATTERN TREE, positionally: the tokenizer
(scparser.cpp, the `sscanf (uip_pattern + uip_index, "%%%[^%]%c", ...)` arm)
turns any `%word%` it does not know into a TOK_VARIABLE carrying the name,
and `uip_match_variable()` looks the variable up and compares its value
against the line at the point the pattern reached.  The decompiles say the
Runners substitute instead, exactly as they do for %object% and %number%:

  * run390 checktask, straight after the %number% / %t_number% arms and
    before the two tests: `For n = 0 To <variable count> - 1` (44AF07), build
    `"%" & var(n).Name & "%"` (44AF25-44AF3B) and `InStr(1, cmd, that, 0)`
    (44AF43).  On a hit, Format(var(n).Value) -> CStr (44AF78-44AF92) and
    Replace(cmd, "%name%", digits, 1, -1, 0) written back over the command
    (44AFC2-44AFDA).  Then the same loop body again for `"%t_" & Name & "%"`
    (44AFF8-44B098), replaced with int2text(CInt(var(n).Value)) (44B03D) --
    the number SPELLED OUT.  Only then the equality at 44B0E2 and, for a '*'
    command, checkwild at 44B139.

  * run400 does the same inside the shared reference substituter
    Proc_19_36_45F268 (mdlSpreadTheLoad): the variable loop is at
    45F105-45F259, `%name%` at 45F11E-45F1B3 and `%t_name%` at
    45F1C6-45F248, with Proc_19_54_452ABC as int2text.  4.0 then tests the
    substituted command the three ways of 45D9FC.

  * The InStr is passed a compare argument of 0 -- vbBinaryCompare -- so the
    variable name is matched CASE-SENSITIVELY against the command as the
    author typed it.  (The command is LCase'd at 44B0B7, but only for the
    equality, long after the substitution.)  A task written `%NUM%` against
    a variable named `num` therefore substitutes nothing and stays a
    literal.

  * Below 3.90 the question does not arise: the TAF file has no Variables
    section at all (sctafpar.cpp gives V<VARIABLE> to the 3.9 and 4.0
    schemas only), so there is nothing a `%foo%` could ever stand for and it
    can only be a literal -- the same answer the %number% probe measured for
    3.70/3.80, and the reason this probe is 3.90/4.00 only.

The world is make_surfprobe's, two variables added:

    num   numeric, 7
    word  string,  "quux"  at 4.00, numeric 42 at 3.90 -- 3.90's VARIABLE
          record has no Type field at all (sctafpar.cpp spells it `ZType`,
          a defaulted zero read from nothing), so every 3.90 variable is a
          number and the string half of this probe is 4.00-only

    Tasks:
      0 `probe`               -> "PROBE OK."
      1 `zork %num% apples`   -> "VAR1 [%num%] [%word%]."
      2 `* zog * %num% *`     -> "VAR2."
                                 THE KEY TASK: a '*' command, so pre-4.0
                                 checkwild decides it and order is free
      3 `frob %t_num%`        -> "TNUM."
                                 the spelled-out twin -- unlike %t_number%,
                                 which the matcher spells with digits and
                                 which can therefore never match, this one
                                 is int2text on BOTH sides
      4 `nurb %word%`         -> "VARS."      a string variable
      5 `blip %nosuch%`       -> "UNK."       no such variable: literal?
      6 `zap`                 -> "ZAP [%num%] [%t_num%] [%word%]."
      7 `wibb %NUM%`          -> "CASE."      an upper-case MARKER
      8 `bork %Big%`          -> "BIGA."      an upper-case variable NAME
      9 `snib %big%`          -> "BIGB."      ... named in lower case

`zork`, `zog`, `frob`, `nurb`, `blip`, `wibb`, `zap`, `quux` and `apples` are
in no Runner's vocabulary, so a cell that misses every task falls to the
pre-4.0 object catch-all or to DontUnderstand and a hit is never in doubt.

The cells, in feed order (cmdfile_pvarref.txt):

    probe
    zap                  the print side, before anything is touched
    zork 7 apples        the flat control: the value, spelled
    zork num apples      the NAME, spelled -- must miss
    zork %num% apples    the MARKER, typed -- must miss, the command having
                         been spelled "zork 7 apples" by then
    zog 7 blip           task 2, in order
    blip 7 zog           task 2 OUT OF ORDER -- THE KEY CELL: pre-4.0
                         checkwild finds its pieces with InStr in any order,
                         4.0's cutting matcher does not
    frob seven           %t_num%: int2text(7) on the command side too
    frob 7               the digits against %t_num% -- must miss
    nurb quux            a string variable substitutes (4.00 only)
    nurb 42              ... and at 3.90 `word` is the number 42 instead
    nurb QUUX            case: the final equality LCase's both sides
    nurb word            the name again -- must miss
    blip %nosuch%        a marker naming no variable: does it survive as a
                         literal the player can type?
    blip nosuch          the bare word -- must miss
    wibb 7               the case-sensitive InStr: if `%NUM%` substituted,
                         this would run task 7
    wibb %NUM%           ... and if it did not, this does
    bork 5               the variable `Big` is stored with a capital: does
                         its own marker still reach it?
    snib 5               and does a lower-case marker reach it?  Between
                         them and `wibb 7` these decide whether the Runner
                         folds case on both sides or merely lower-cases the
                         command before a binary InStr -- which would leave
                         a capitalised variable name (Riding_Home's
                         %NewPlayer%) unreachable from any command at all
    zap
    probe

Usage:
    python3 make_varrefprobe.py [390|400|all]
Session (from ~/adrift-battle/runner/wine):
    TRANSCRIPT=Adrift_varref390.txt ./fast.sh p39VARREF.taf \
        cmdfile_pvarref.txt run390x.exe
    TRANSCRIPT=Adrift_varref400.txt ./fast.sh p4VARREF.taf \
        cmdfile_pvarref.txt run400x.exe
"""
import sys

import make_surfprobe as surf

LIT, CAVE = surf.LIT, surf.CAVE

surf.OBJECTS = [
    ("rock", "a big", "A grey rock.", ("room", LIT), "", 0, 0, 0),
    ("gem",  "a",     "A green gem.", ("room", LIT), "", 0, 0, 0),
]
surf.NAMES = [o[0] for o in surf.OBJECTS]
surf.WEAPONS = set()
surf.TASKS = [
    ("probe", "PROBE OK."),
    ("zork %num% apples", "VAR1 [%num%] [%word%]."),
    ("* zog * %num% *", "VAR2."),
    ("frob %t_num%", "TNUM."),
    ("nurb %word%", "VARS."),
    ("blip %nosuch%", "UNK."),
    ("zap", "ZAP [%num%] [%t_num%] [%word%]."),
    ("wibb %NUM%", "CASE."),
    ("bork %Big%", "BIGA."),
    ("snib %big%", "BIGB."),
]
surf.NPCS = []
surf.BATTLE = 0
surf.OUT = {390: "p39VARREF.taf", 400: "p4VARREF.taf"}

# 3.90 has no Type field, so `word` can only be a string at 4.00; at 3.90 it
# is a second number and the cell that reaches it is `nurb 42`.
VARIABLES = {390: [("num", 0, "7"), ("word", 0, "42"), ("Big", 0, "5")],
             400: [("num", 0, "7"), ("word", 1, "quux"), ("Big", 0, "5")]}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([390, 400] if arg == "all" else [int(arg)]):
        surf.VARIABLES = VARIABLES[v]
        surf.emit(v)
