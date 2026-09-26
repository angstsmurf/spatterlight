# geas tests

Two engines live in `terps/geas`, and the test tree is split the same way, with
mirrored subfolders in each half (the layout `terps/scarier/test` uses):

| | |
| --- | --- |
| [`quest4/`](quest4/README.md) | Quest 4 (`.asl`/`.cas`) — the geas engine |
| [`quest5/`](quest5/README.md) | Quest 5 (`.aslx`/`.quest`) — the aslx engine and its QuestViva oracle |

Each half holds `fixtures/` (small hand-written games that are ours, committed),
`goldens/` (the committed answer keys), `harness/` (the tooling), an untracked
`games/` for the third-party corpus, and the `games.manifest.tsv` that pins it.
Only engine-agnostic things stay at this level: this README,
[`GAMES.md`](GAMES.md) and `fetch_games.sh` (the corpora, for both engines), the
`Makefile` that builds both halves, `questglk_unit_tests.cc` (the helpers both
Glk frontends share, `../questglk-common.inc`), `glkdrive.py` (a fake
Spatterlight app that drives the built terp over the glkimp protocol) and
`autosave/`, the one suite that spans both engines (below). `glkdrive.py` is
terp-agnostic and is the same file as `terps/scarier/test/glkdrive.py`, kept
byte-identical so each tree stays self-contained; `make check` fails if the
two copies drift, so edit one and copy it over the other.

## Build

```sh
make            # build every in-repo harness, both engines
make syntax     # per-file syntax check of every engine source (see below)
make check      # syntax, + the unit tests, + the Quest 4 fixture games
make asan       # same under AddressSanitizer/UBSan -- see quest4/README.md
make clean

make gamescheck # verify both game corpora against their manifests
make gamesfetch # download whatever of them is still online

make autosave   # Spatterlight autosave/autorestore, both engines (Xcode build)
```

Everything `make check` runs is self-contained — no game corpus — so it is the
part worth wiring into CI. The harnesses build beside their own sources
(`quest4/harness/`, `quest5/harness/`) and are gitignored; those that open a
fixture at run time resolve it relative to their own directory, so run them from
there (`cd quest5/harness && ./aslx_runtime_test`), which is what the Makefile's
own rules do.

## Per-file syntax check (`make syntax`)

Every harness here unity-includes the engine: `geas_walkthrough_runner.cc`
`#include`s all six engine `.cc` files, so they become **one** translation unit.
That means a function defined in one `.cc` and called from another compiles
cleanly even when no header declares it — the caller simply sees the earlier
definition. The Xcode build compiles those files separately and rejects it.

This is a real failure mode, not a hypothetical one: a missing `starts_with_i`
prototype in `geas-util.hh` passed the fixtures, the unit tests *and* all 111
walkthroughs, and broke the app build. `make syntax` runs `-fsyntax-only` over
each source on its own — including `geasglk.cc`, `quest5/aslxglk.cc` and
`geasglkterm.c`, which no harness here compiles at all — in a few seconds, and
is a prerequisite of `make check`.

## Spatterlight autosave (`autosave/run_autosave_tests.py`)

Everything above runs the engines through the in-repo harnesses and CheapGlk.
The autosave and autorestore code never gets there: it is compiled only into
the Spatterlight build of the terp (`#ifdef SPATTERLIGHT`,
`../geasglk-autosave.mm` and the `ASLXGLK-AUTOSAVE` blob in
`../quest5/aslxglk.cc`) and talks to `libglkimp`. This suite tests it the way
the app exercises it. `glkdrive.py` plays the app: it launches
`build/Debug/geas` with the library, answers the protocol, feeds scripted
input, and ends each session with the window-closed event (`EVTQUIT`), which is
the exit that keeps the autosave, so the next launch of the same game
autorestores.

```sh
make autosave                                   # builds the terp, runs all cases
python3 autosave/run_autosave_tests.py -v       # existing build; -v prints diffs
python3 autosave/run_autosave_tests.py q5-      # name filter
python3 autosave/run_autosave_tests.py --terp path/to/geas
```

The check is equivalence, not goldens: a script played in one session must
print, command for command, what it prints when the process is killed and
relaunched after chosen commands, with determinism on. Only the main text
window is compared, because the status line and the side pane are repainted at
boot on a relaunch. Every relaunch must also open and delete the same number
of windows (the boot windows are replaced by the restored ones) and print
nothing before its first input. The autosave directory is the app's
(`~/Library/Application Support/Spatterlight/Quest Files/Autosaves/`), under a
`geas-autosave-test-*` signature that is created and removed per case.

The two probe games in `autosave/` (`probe4.asl`, `probe5.aslx`) have one
command per piece of state a relaunch has to carry. The cases:

| | Quest 4 (`geasglk.cc`) | Quest 5 (`quest5/aslxglk.cc`) |
| --- | --- | --- |
| relaunch after every command | `q4-every-command` | `q5-every-command` |
| RNG position | `q4-rng` | `q5-rng` (the fallback stream and every per-expression stream, including those inside script bodies) |
| variables / attributes | `q4-variable` | `q5-attribute` |
| undo history | `q4-undo` | (no engine undo) |
| real-time timers | `q4-timer`, `q4-timer-midcycle` (XFAIL: a tick that did not fire is not re-saved) | `q5-timer`, `q5-timer-midcycle`, `q5-timeout` |
| prompts the game can be closed on | `q4-question`, `q4-menu` (no autosave; the relaunch resumes at the turn prompt) | `q5-get-input`, `q5-show-menu`, `q5-ask`, `q5-wait` (same) |
| hyperlinks | `q4-pane-links` (fold, Take, Drop through the pane) | `q5-inline-link`, `q5-link-menu` (an object link's verb menu stays autosaved and reopens on relaunch) |
| walkthroughs from `goldens/` | Bear Campsite, Mansion, Gathered in Darkness | Exit the Room, Bear's Epic Quest, ARC II |
| damaged container | `q4-corrupt` (discarded, fresh boot), `q4-bad-undo` (engine state restored, undo history ignored with a log line) | `q5-corrupt` |

Walkthrough cases are skipped when the game is absent from `games/`.
Restored transcripts are not re-sent as link runs, so a link clicked after a
relaunch is addressed by number (`link:PEER:N`) rather than by text; see the
`glkdrive.py` docstring for the script vocabulary (`tick:N`, `key:X`,
`click:TEXT`, `link:N`).

## Shared frontend helpers (`questglk_unit_tests.cc`)

Both Glk frontends (`geasglk.cc` and `quest5/aslxglk.cc`) draw on
`../questglk-common.inc`; this binary includes that file directly and links the
in-repo CheapGlk for the `glk_*` symbols its helpers reference. It belongs to
neither engine, so it stays here and is built at the root as
`./questglk_unit_tests`. Exit 0 is a pass.
