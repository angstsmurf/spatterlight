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
Glk frontends share, `../questglk-common.inc`) and `glkdrive.py`.

## Build

```sh
make            # build every in-repo harness, both engines
make syntax     # per-file syntax check of every engine source (see below)
make check      # syntax, + the unit tests, + the Quest 4 fixture games
make asan       # same under AddressSanitizer/UBSan -- see quest4/README.md
make clean

make gamescheck # verify both game corpora against their manifests
make gamesfetch # download whatever of them is still online
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

## Shared frontend helpers (`questglk_unit_tests.cc`)

Both Glk frontends (`geasglk.cc` and `quest5/aslxglk.cc`) draw on
`../questglk-common.inc`; this binary includes that file directly and links the
in-repo CheapGlk for the `glk_*` symbols its helpers reference. It belongs to
neither engine, so it stays here and is built at the root as
`./questglk_unit_tests`. Exit 0 is a pass.
