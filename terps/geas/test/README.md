# geas tests

Four layers, in increasing distance from a real game:

| what | needs | catches |
| --- | --- | --- |
| `make syntax` | nothing (in-repo) | code that only compiles as one translation unit |
| `run_fixtures.sh` | nothing (in-repo) | engine behaviour, against golden transcripts |
| `geas_unit_tests` | nothing (in-repo) | corrupt saves and parser edges no player can drive |
| `run_walkthroughs.sh` | the games (local; scripts in-repo) | regressions in 111 real games |

`make check` runs the first three — they are self-contained, so they are the ones
worth wiring into CI.

## Build

```sh
make            # builds ./geas_walkthrough_runner
make syntax     # per-file syntax check of every engine source (see below)
make check      # syntax, + ./geas_unit_tests, then runs both test suites
make asan       # same under AddressSanitizer/UBSan -- see the warning below
make clean
```

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

## Fixtures (`fixtures/`, `run_fixtures.sh`)

Small hand-written `.asl` games, each paired with a `.cmd` script and a golden
`.expected` transcript. They exist because **the game corpus cannot catch these
bugs**: a shipped game only walks the paths its author happened to walk, so a
crash or a wrong string in an unvisited corner leaves all 111 walkthroughs
byte-identical. Every fixture here was checked against a pre-fix engine and
either crashes it or produces different output; each file's header comment says
what it guards.

```sh
./run_fixtures.sh           # PASS/FAIL table
./run_fixtures.sh --bless   # regenerate the .expected files
```

The transcripts must be reproducible, so the seed is fixed and no fixture ever
prints a raw random number — `rand()` differs between C libraries. A fixture
that wants to check a random value asserts a *range* instead (see
`functions.asl`).

> **Watch out:** several of the bugs these guard are undefined behaviour, and a
> plain `-O2` build does not fault on them — it silently returns garbage. A
> fixture that passes at `-O2` on a knowingly-broken engine is not proof of
> anything; run `make asan` to see them. `source.asl` is the clearest example:
> it is a clean pass at `-O2` either way, and a heap-buffer-overflow under ASan
> before the fix.

### Manual fixtures (`fixtures/aslx/`, not run by any script)

A few things only exist on a graphical Glk host, so no in-repo harness can
assert on them — CheapGlk, which the smoke harness and `run_fixtures.sh` use,
has neither graphics nor hyperlinks. Those fixtures are driven **by hand in
Spatterlight**, and each carries a header comment listing the commands to type
and what to look for.

| fixture | checks |
|---|---|
| `framepicture.aslx` | the frame-picture band: opens, sizes itself to the picture without upscaling, and — via **both** the `SetPanelContents` and Quest 5.0 `RunScript` channels — closes again leaving no gap |

They are kept here rather than in a scratch directory because the paths they
cover are otherwise reachable only by finding a shipped game that happens to
use them: the picture-clear path went unverified through two commits for
exactly that reason.

## Unit tests (`geas_unit_tests.cc`)

For engine code a fixture cannot reach, because no *player input* reaches it: a
save file is untrusted binary input (every count in it is a loop bound, so a
corrupt one used to be an out-of-memory), and a record holding no elements
cannot be conjured from a game script. Run `./geas_unit_tests`; exit 0 is a
pass.

## Usage

```
geas_walkthrough_runner [options] <game-file> <command-script>
```

The command script is one input per line. Menu numbers and free-text answers
are read from the *same* stream as commands (via `make_choice`/`get_string`),
so a script reads exactly like what a player types. Two script styles work:

* a **raw** script — clean commands only; and
* a **prose walkthrough** — a header (everything up to a `Start:` line is
  skipped) with `  (parenthetical)` notes after commands (stripped).

### Options

| option | effect |
| --- | --- |
| `--win MARKER` | success marker; sets exit code (0 = seen) and `WON=` report |
| `--tick` | call `tick_timers()` once per turn, like the geasglk frontend does after each input — needed for games driven by timers |
| `--save-scum` | on a turn that kills the player without the win marker, reload the pre-turn state and retry (random fights) |
| `--fight "c1\|c2=MARKER"` | repeat the `\|`-separated commands (cycled), save-scumming, until `MARKER` appears; repeatable |
| `--echo` | echo the full transcript (otherwise only a summary + tail) |
| `--seed N` | RNG seed (or set `GEAS_SEED`) |
| `--max-reloads N` | per-turn save-scum reload cap (default 20000) |

Exit status is 0 when the win marker is seen (or, with no `--win`, when the
whole script ran), else 1.

## Examples

A simple game just needs a win marker:

```sh
./geas_walkthrough_runner --win "won the game" Magic.asl "Magic World - command script.txt"
```

World's End needs all three extras: timer ticking (its dynamite fuse is a
real-time timer whose explosion reveals an item the rest of the game needs),
and save-scum for its two random fights:

```sh
GEAS_SEED=1 ./geas_walkthrough_runner --tick --save-scum \
  --fight "use vial1 on cube|use vial2 on cube=The cube explodes" \
  --fight "use laser pistol on warlord=slumps to the ground dead" \
  --win "slumps to the ground dead" \
  "games/worldsend/world's end.asl" "walkthroughs/Worlds End - command script.txt"
```

## Running a whole collection

`run_walkthroughs.sh` plays a set of games against their scripts and prints a
PASS/FAIL table. The command scripts live in `./walkthroughs` and are committed,
so with the games in `./games` no arguments are needed:

```sh
./run_walkthroughs.sh
```

The games are copyrighted and stay local-only (`games/` is gitignored), so point
the script elsewhere if you keep them somewhere else. Three of the games are
played against a `<title> - walkthrough.txt` written by someone else; those are
not redistributed here, so they SKIP unless you pass a directory holding them:

```sh
./run_walkthroughs.sh "/path/to/Geas games" "/path/to/Geas walkthroughs"
```

A walkthrough is looked up in `./walkthroughs` first and in that directory
second, so a local copy never shadows a committed script.
