# A Quest 4 ground-truth oracle for geas

The transcripts in `../../goldens` are *self*-goldens: they record what geas
prints, bugs and all, so they catch regressions but can never catch a place
where geas has always been wrong.  This directory replays the very same command
scripts through a second engine and diffs the two.

The second engine is [QuestViva](https://github.com/textadventures/quest)'s
`src/Legacy`.  That is not a reimplementation of Quest 4 — `V4Game.cs` is a
line-by-line C# translation of Axe's VB6 Quest 4, down to `Strings.Mid` and
one-based arrays — so where it and geas disagree, geas is almost always the one
that is wrong.  It plays the same role here that FrankenDrift plays for
`scarier`.  (Almost always, not always: the translation has its own defects, and
a few are listed at the bottom.)

## Running it

```sh
./build.sh                     # clones QuestViva if needed, then builds qv4
./compare.sh                   # whole corpus: summary table + out/<label>.diff
./compare.sh MagicWorld Hobbit # just these labels
./firstdiff.py                 # first divergence per game -- read this first
./firstdiff.py Kingdom Wizard  # just these labels
./triage.py                    # bucket every diff line by kind of divergence
SHOW=1 ./triage.py             # ... and print the lines that fit no bucket
```

`firstdiff.py` and `triage.py` answer different questions.  One early
disagreement desynchronises everything after it, so a 700-line diff is usually
one bug plus 699 lines of cascade: `firstdiff.py` shows only each diff's head,
which is the part that has a cause, and is the right place to start.
`triage.py` measures the whole diff and is how you size a bucket once you know
what is in it.

Labels, game files, command scripts and per-game flags all come out of
`../run_walkthroughs.sh`, so the oracle and the regression suite cannot drift
apart.  `qv4` alone plays one game:

```sh
QVH_SEED=1 dotnet bin/Release/net10.0/qv4.dll [--tick] <game.asl|.cas> <script.txt>
QV4_TRACE=1 ...                # trace prompts, commands and output to stderr
```

The clone lives outside the repo (`~/questviva-oracle/questviva` by default,
`ORACLE_HOME` to move it) and is shared with the Quest 5 oracle, whose
`patch_questviva.py` also routes both engines' RNG through the same
xoshiro128\*\* stream — so `QVH_SEED=1` and `GEAS_SEED=1` draw identical numbers
in identical order, and a random event is not automatically a diff.

`dotnet` must be invoked as `dotnet …/qv4.dll`; the generated apphost fails to
find the framework on this machine.

## What is normalised away, and why

geas talks to Glk and Quest talks to a web view, so a raw diff is mostly
frontend noise.  Both sides are put through the same normalisation
(`compare.py:normalise`) first:

* **Blank lines are dropped entirely.**  Quest's `Print` ends a paragraph and
  its Player adds the vertical space with CSS; geas emits its blank lines
  itself.  Comparing them measures the frontends, not the engines — and it
  drowns out everything else.
* **Runs of spaces are squeezed and lines are stripped.**  HTML collapses
  whitespace, a Glk text buffer does not.
* **geas's banner line is dropped** — `name[, v<version>][ <author>]`, which
  Quest puts in the window title instead.  qv4 echoes the name Quest reports
  through `UpdateGameName` as `[gamename] …`, and only a first line starting
  with that name is dropped, so a game that really does print its own title
  (Defenders of Gondor) keeps it on both sides.
* **The runner's own `[runner] …` summary** is not game output.

`qv4` itself absorbs the harness-shape differences, always bending QuestViva
towards geas so that what is left is engine behaviour: menus are printed in
geas's `caption` / `N) option` / `[choice] N` shape, `ask` is rendered as a
Yes/No menu the way geas's `choose_yes_no` is, and Quest's default
`Press a key to continue...` — the one a `wait` with no message of its own
prints — is dropped, because geas prints nothing there in any frontend and the
diff would otherwise report finding 2 once per `wait`.  A `wait <message>` is
kept on both sides: the runner prints the author's text the way GeasGlk does.
See the comment at the top of `Program.cs`.

## Findings

See `FINDINGS.md`.
