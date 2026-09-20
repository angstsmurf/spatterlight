#!/usr/bin/env python3
"""A task command holding a `[a/b]` or `{word}` GROUP, in all four versions.

The last half of the checkwild lead in WINE-TRANSCRIPTS-TODO.md.  Scarier
routes a task command containing `[` or `{` to the 4.0 pattern tree at EVERY
version -- `run_match_task_commands()` reaches its pre-4.0 arm only under
`!strpbrk (pattern, "[{")` -- and the tree EXPANDS the group, so `zog
[rock/gem]` is offered `zog rock` and `zog gem`.  The decompiles say the
three older Runners cannot do that:

  * run390 checktask (body 44AA5A-44B6E6), run380 (43B6A3-43C51D) and
    run370 (433227-433E4A) hold NO `[`, `]`, `{` or `}` string literal
    anywhere in the routine -- the only hit in the three bodies is an
    annotation banner in run370.bas:2394.  After the reference substitution
    the command meets exactly two tests: an all-lowercase equality (run390
    44B0E2) and, `If InStr(cmd, "*") > 0`, `checkwild(line, cmd)` (44B139).
    There is no third arm to route a group to.  The group syntax is a 4.0
    Generator feature; a pre-4.0 file that carries one at all was written by
    a hand or an upconverter, and to the Runner it is just punctuation.

  * run400's matcher (458E6C) has the arm the others lack: a `*` command
    goes to 457D68 and a `[`/`{` one to 45D940 (NewParse), so only 4.0
    expands.

The reading held, and then some: a pre-4.0 group command matches the line
that spells the brackets OUT and nothing else, so Scarier was matching where
three Runners refuse -- and 4.0 turned out to try that same literal FIRST,
before it expands anything.  The answers are below the feed.

The world, deliberately lit, one room's worth of nouns:

    Lit Room  --n-->  Cave  (--s--> back)

    0 rock  "a big"  Lit Room
    1 gem   "a"      Lit Room

    Tasks:
      0 `probe`                -> "PROBE OK."
      1 `zog [rock/gem]`       -> "GRP1."   alternatives, at the end
      2 `nurb {the} rock`      -> "GRP2."   an optional word, in the middle
      3 `* blip [red/blue] *`  -> "GRP3."   a group INSIDE a `*` command --
                                            the cell that says whether the
                                            pre-4.0 checkwild pieces are cut
                                            at the bracket or carried whole
      4 `frob {up}`            -> "GRP4."   an optional word, at the end,
                                            so the 4.0 tree accepts the bare
                                            verb and pre-4.0 cannot

`zog`, `nurb`, `blip` and `frob` are in no Runner's vocabulary, so a cell
that misses every task falls to the pre-4.0 object catch-all or to
DontUnderstand and a hit is never in doubt.  `look` separates the cells.

The cells, in feed order.  Each group task is fed both its EXPANSIONS (what
the 4.0 tree accepts) and its LITERAL (what a pre-4.0 equality would have to
accept, brackets and all):

    probe
    zog rock                 expansion 1  -- 4.0 only, if the reading holds
    zog gem                  expansion 2
    zog [rock/gem]           THE LITERAL -- pre-4.0 must run task 1 here and
                             4.0 must refuse it
    nurb the rock            the optional word present
    nurb rock                the optional word absent
    nurb {the} rock          THE LITERAL
    xxx blip red yyy         a group inside a `*` command, expansion
    xxx blip [red/blue] yyy  THE LITERAL inside a `*` command -- pre-4.0
                             checkwild has to find "blip [red/blue]" in the
                             line by InStr, brackets included
    frob                     the trailing optional word absent -- the 4.0
                             tree's bare-verb cell
    frob up                  present
    frob {up}                THE LITERAL
    probe

Whether a Runner even lets `[` through its input filter is part of the
answer: if the brackets are stripped before matching, the literal cells read
like the expansions and the two engines agree for a different reason.

A second short feed (cmdfile_pgroup2.txt) asks what KIND of literal test it
is, once the first said there is one:

    probe
    zog [gem/rock]          the alternatives reordered
    zog [rock/gem ]         a stray space inside the group
    ZOG [ROCK/GEM]          upper case
    zog  [rock/gem]         a double space in the typed line
    zog rock/gem            the brackets dropped
    nurb {the} rock         the control
    nurb {THE} rock         upper case again
    frob {up} x             the literal with something after it
    probe

What the four Runners answered, 2026-09-20 (scrollback_gr370.txt,
scrollback_gr380.txt, Adrift_209_gr390.txt/_gr390b.txt,
Adrift_210_gr400.txt/_gr400b.txt):

  3.70/3.80/3.90  a group is PUNCTUATION.  `zog rock` and `zog gem` are the
              object catch-all, `nurb rock` and `nurb the rock` are too,
              `frob` and `frob up` are "I don't understand.", and the four
              cells that spell the brackets out -- `zog [rock/gem]`, `nurb
              {the} rock`, `xxx blip [red/blue] yyy`, `frob {up}` -- are the
              only ones that run their task.  The three Runners agree cell
              for cell.
  4.00        BOTH.  Every expansion runs, and so does every literal, except
              inside a `*` command: `xxx blip red yyy` matches nothing while
              `xxx blip [red/blue] yyy` runs task 3.  run400's command loop
              (45D9FC-45DBA4) says why -- equality (45DA51) is tried first,
              then the `*` matcher (45DA8C -> 457D68), and only a command
              that survived both reaches NewParse (45DADB -> 45D940), so a
              group in a `*` command is never expanded.
  the literal test is plain equality, both at 3.90 and at 4.00: reordering
              the alternatives, adding a space inside the group and dropping
              the brackets are all refused, while case is folded.  `frob
              {up} x` is refused too (3.90 answers "Nothing special.", 4.00
              "I don't understand.").

PORTED 2026-09-20: scrunner.cpp run_match_task_commands().  Below 4.0 a
group command joins the pre-4.0 arm instead of skipping it -- checkwild when
it holds a `*`, equality when it does not -- and at 4.0 the equality and the
`*` matcher are tried for a group command before the tree's expansion is
believed.  Scarier now answers all eleven cells of the first feed exactly as
the Runner of its own version does.

One cell of the second feed is knowingly left: `zog  [rock/gem]`, a DOUBLE
SPACE in the typed line, runs the task in run390 and run400 and does not in
Scarier.  The Runner collapses a keyboard line's spaces before any of the
three tests, but not a line it builds itself -- which is exactly what
run_rerun_skips_tasks exists for, a 4.0 "with?" continuation whose two
spaces match no task -- and telling the two apart here buys nothing: the
cell needs a typed line carrying both a double space and a bracket.

Zero corpus exposure, measured 2026-09-20 over every .taf in games/ and
downloaded/ (SCR_DUMP_TASKS on the 405 that load, plus a raw
taf_pattern_scan.plaintext() scan of the 14 pre-4.0 stragglers whose dump
never fires): 6685 task commands carry a group and every one is in a 4.00
file, and NO game at any version puts a `*` and a group in the same command.
The suite is 429 PASS either way.

Usage:
    python3 make_groupprobe.py [370|380|390|400|all]
Session (from ~/adrift-battle/runner/wine):
    TRANSCRIPT=Adrift_group390.txt ./fast.sh p39GROUP.taf \
        cmdfile_pgroup.txt run390x.exe
    TRANSCRIPT=Adrift_group400.txt ./fast.sh p4GROUP.taf \
        cmdfile_pgroup.txt run400x.exe
    TRANSCRIPT=Adrift_group380.txt ./fast.sh p38GROUP.taf \
        cmdfile_pgroup.txt run380.exe
    TRANSCRIPT=Adrift_group370.txt ./fast.sh p37GROUP.taf \
        cmdfile_pgroup.txt run370.exe
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
    ("zog [rock/gem]", "GRP1."),
    ("nurb {the} rock", "GRP2."),
    ("* blip [red/blue] *", "GRP3."),
    ("frob {up}", "GRP4."),
]
surf.NPCS = []
surf.BATTLE = 0
surf.OUT = {370: "p37GROUP.taf", 380: "p38GROUP.taf",
            390: "p39GROUP.taf", 400: "p4GROUP.taf"}

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        surf.emit(v)
