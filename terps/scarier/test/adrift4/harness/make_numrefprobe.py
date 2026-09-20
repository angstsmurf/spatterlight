#!/usr/bin/env python3
"""A task command's %number% and %t_number%, in all four versions.

The last half of the checkwild lead in WINE-TRANSCRIPTS-TODO.md.  Scarier
matches a %number% command with the PATTERN TREE, positionally:
`uip_match_number()` (scparser.cpp) sscanf's a number out of the line at the
point the pattern reached, binds it and walks on.  A pre-4.0 command holding
one never reaches `run_pre400_substitute_references()` at all -- that helper
returns FALSE for any token but %object% and %character%, so the tree's
answer stands unchecked.  The decompiles say the Runners do something else
entirely:

  * run390 checktask, straight after the %object% (44AAD6) and %character%
    (44AD2A) walks and before the two tests: `If InStr(cmd, "%number%") > 0`
    (44ADDF) calls numintext() (4332C8) and, if it found one, Replaces
    "%number%" with Format(MemVar_4681AC) -- the DIGITS of the number it
    found -- writing the result back over the command (44AE61).  Then the
    same for "%t_number%" (44AE8B) via numintext2() (42946C), replaced with
    CStr(MemVar_4681AC), digits again (44AEC3).  Only then comes the
    equality at 44B0E2 and, for a '*' command, checkwild at 44B139.

    So %number% is not a positional wildcard below 4.0: it is a
    SUBSTITUTION, like %object%, and a '*' command carrying one is decided
    by checkwild, whose pieces are found by InStr anywhere in the line and
    in any order.

  * numintext() (4332C8) is how the number is found, and it is nobody's
    idea of a parser.  It takes the LOWEST position in the line at which any
    of the characters "0".."9" occurs (a 0..9 loop over InStr, keeping the
    minimum, 433112-43317E), collects the non-space run STARTING AT THAT
    CHARACTER (433198-433208), prepends "-" when the character before it is
    one (433227-433265), and then takes Val() of that (433268) and clamps it
    to a signed 32-bit range.  So the number the command is spelled with is
    Val's, not the line's: "007" becomes 7 and "3x" becomes 3, and the
    substituted command no longer equals the line that produced it.

  * numintext2() (42946C) walks n = 0 To 20 and asks `c(int2text(n), "")` --
    c() with an empty second argument searches MemVar_468118, the input line
    (433320-433329) -- so it is looking for a number SPELLED OUT, whole-word,
    and it does not stop at the first hit: the LAST n whose word is in the
    line wins.  What it hands back is still a number, and checktask spells
    the command with its DIGITS.  The output filter (45CBD0) does the
    opposite -- %number% is Format(Me(420)) at 45B1BF and %t_number% is
    int2text(Me(420)) at 45B21B -- so the words-vs-digits split is real
    everywhere except in the matcher, where both are digits.  If that
    reading is right a %t_number% command can never match anything: the line
    says "five" and the command it is compared against says "5".

  * run400 does exactly the same, in a routine of its own:
    Proc_19_36_45F268 (mdlSpreadTheLoad, body 45EE58-45F264) is the 4.0
    reference substituter, with %text% (45EE99) ahead of %number% (45F03C,
    Proc_19_52_453A48 = numintext) and %t_number% (45F0AA, Proc_19_53_444458
    = numintext2), each Replaced the same way and with the same Format /
    CStr split.  4.0 then tests the substituted command the three ways of
    45D9FC.

  * run370 and run380 hold NO "%number%", "%t_number%" or "%text%" string
    literal ANYWHERE in the exe -- the same census that finds "%character%"
    from 3.80 and "%text%" at 4.00 only.  So below 3.90 all three are
    literals the player has to type, exactly as %character% is below 3.80.

The world, deliberately lit, two nouns so a miss is visible:

    Lit Room  --n-->  Cave  (--s--> back)

    0 rock  "a big"  Lit Room
    1 gem   "a"      Lit Room

    Tasks:
      0 `probe`                -> "PROBE OK."
      1 `zork %number% apples` -> "NUM1 [%number%] [%t_number%]."
                                  flat, the reference in the middle
      2 `* zog * %number% *`   -> "NUM2 [%number%]."
                                  THE KEY TASK: a '*' command, so pre-4.0
                                  checkwild decides it and order is free
      3 `frob %t_number%`      -> "TNUM [%number%] [%t_number%]."
      4 `nurb %number%`        -> "NUM3 [%number%]."
      5 `zap`                  -> "ZAP [%number%] [%t_number%]."
                                  binds nothing: what does a Runner print
                                  for a referenced number nothing set, and
                                  does numintext's write survive a command
                                  that did NOT match?

`zork`, `zog`, `frob`, `nurb`, `zap`, `blip` and `apples` are in no Runner's
vocabulary, so a cell that misses every task falls to the pre-4.0 object
catch-all or to DontUnderstand and a hit is never in doubt.  `look`
separates the cells.

The cells, in feed order (cmdfile_pnumref.txt):

    probe
    zap                  turn 1, nothing ever bound
    zork 5 apples        the flat control -- 3.9/4.0 must run task 1, and
                         3.7/3.8 must not, the command being a literal
    zap                  straight after a bind: does it linger?
    zork 12 apples       multi-digit
    zork five apples     a WORDED number against %number%: numintext scans
                         for digits only, so nothing substitutes
    zork 007 apples      LEADING ZEROS -- Val() makes the command "zork 7
                         apples", which no longer equals the line
    zork 3x apples       a digit glued to a letter -- Val("3x") = 3, same
                         trap
    zork -5 apples       the "-" lookbehind at 433227
    zap                  after two lines that set the number but matched
                         nothing
    zork 5 apples now    trailing junk against a flat command
    zork apples 5        the number in the wrong place, flat: must refuse
                         everywhere, there being no '*'
    zog 7 blip           task 2, in order
    blip 7 zog           task 2 OUT OF ORDER -- THE KEY CELL: checkwild
                         searches each piece with InStr, so pre-4.0 must run
                         the task where the tree and 4.0's cutting matcher
                         refuse
    zog blip             no number at all: "%number%" stands in the command
    blip 9 zog 3 blip    TWO numbers, the first one out of position:
                         numintext takes the 9 (lowest position in the line),
                         so pre-4.0 must answer "NUM2 [9]" while a
                         positional matcher can only see the 3
    frob five            %t_number% against a worded number -- the cell that
                         says whether the digits-for-words substitution
                         leaves it able to match at all
    frob 5               %t_number% against digits
    nurb 0               zero
    nurb 20              the top of numintext2's 0..20 loop
    nurb 5 blip          flat command, trailing junk
    zap
    probe

What the four Runners answered, 2026-09-20 (Adrift_209_nr370.rtf,
Adrift_210_nr380.rtf, Adrift_211_nr390.txt, Adrift_212_nr400.txt).  Every
prediction above held, and the reading is now measured end to end:

  3.70/3.80   %number% and %t_number% are LITERALS, on both sides.  Not one
              of tasks 1-4 ever runs -- `zork 5 apples`, `zog 7 blip`, `frob
              5`, `nurb 0` and the other nineteen cells are all "I don't
              understand." -- and task 5's `zap` prints "ZAP [%number%]
              [%t_number%]." at every turn, the output filter leaving the
              markers alone too.  The two Runners agree cell for cell.
  3.90        the substitution, with checkwild deciding a '*' command:
              `zog 7 blip` AND `blip 7 zog` both run task 2, and `blip 9 zog
              3 blip` answers "NUM2 [9]." -- numintext took the 9 because it
              is the leftmost digit in the LINE, not because the pattern
              reached it.
  4.00        the same substitution, then its own cutting matcher: `zog 7
              blip` runs task 2 and `blip 7 zog` and `blip 9 zog 3 blip` are
              refused.  That one pair of cells is the whole 3.90/4.00 split;
              the other twenty-one are identical.
  Val() bites at both.  `zork 007 apples` and `zork 3x apples` are refused
              everywhere: the command is spelled "zork 7 apples" and "zork 3
              apples" and no longer equals the line that spelled it.  `zork
              -5 apples` runs, the "-" lookbehind holding, and prints "NUM1
              [-5] [-5]." -- int2text of a negative is its digits.
  %t_number%  can match nothing, as predicted: `frob five` is refused
              (numintext2 finds "five" and spells the command "frob 5") and
              so is `frob 5` (no word to find, so "%t_number%" stands).
  the number is ONE Long and a non-matching command still writes it: `nurb 5
              blip` runs nothing and the `zap` after it prints "ZAP [5]
              [five].".  A line with no digit leaves it alone rather than
              clearing it -- numintext's whole body is inside `If var_8A <
              32000`.  Unset it is 0: turn 1's `zap` is "ZAP [0] [zero]." at
              3.90 and 4.00.

PORTED 2026-09-20: scrunner.cpp run_line_number() / run_line_number_word() /
run_substitute_number_references(), reached from
run_pre400_substitute_references() at 3.90 and from a number-only arm of the
4.0 command loop in run_match_task_commands(); scvars.cpp
var_is_unknown_reference() for the print side, and t_number's unset answer,
which used to be "[Number unknown]" and is "zero".  Scarier now answers all
23 cells exactly as the Runner of its own version does, at all four
versions.

Corpus exposure is real, unlike the group lead's: 50 3.90 task commands in
six games carry a %number%, several of them glued to wildcards
(druggy_lane's "take *%number%*" and "*pay *%number%*", Vampire's "push *
%number% *", circus's "turn* lock* %number%", The Town Of Azra's "sell **
deer carcass ** Drako for %number% dollars", The Screen Savers On Planet X's
"set * dial to %number%").  All six have goldens and none of them moved.

Usage:
    python3 make_numrefprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    TRANSCRIPT=Adrift_numref390.txt ./fast.sh p39NUMREF.taf \
        cmdfile_pnumref.txt run390x.exe
    TRANSCRIPT=Adrift_numref400.txt ./fast.sh p4NUMREF.taf \
        cmdfile_pnumref.txt run400x.exe
    TRANSCRIPT=Adrift_numref380.txt ./fast.sh p38NUMREF.taf \
        cmdfile_pnumref.txt run380.exe
    TRANSCRIPT=Adrift_numref370.txt ./fast.sh p37NUMREF.taf \
        cmdfile_pnumref.txt run370.exe
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
    ("zork %number% apples", "NUM1 [%number%] [%t_number%]."),
    ("* zog * %number% *", "NUM2 [%number%]."),
    ("frob %t_number%", "TNUM [%number%] [%t_number%]."),
    ("nurb %number%", "NUM3 [%number%]."),
    ("zap", "ZAP [%number%] [%t_number%]."),
]
surf.NPCS = []
surf.BATTLE = 0
surf.OUT = {370: "p37NUMREF.taf", 380: "p38NUMREF.taf",
            390: "p39NUMREF.taf", 400: "p4NUMREF.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
