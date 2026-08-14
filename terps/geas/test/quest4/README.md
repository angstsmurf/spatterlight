# Quest 4 (`.asl`/`.cas`) tests

The geas engine's own test tree. Three layers, in increasing distance from a
real game:

| what | needs | catches |
| --- | --- | --- |
| `make syntax` (at `../`) | nothing (in-repo) | code that only compiles as one translation unit |
| `harness/run_fixtures.sh` | nothing (in-repo) | engine behaviour, against golden transcripts |
| `harness/geas_unit_tests` | nothing (in-repo) | corrupt saves and parser edges no player can drive |
| `harness/run_walkthroughs.sh` | the games (local; scripts in-repo) | regressions in 108 real games |

`make check` (from `../`) runs everything but the last — those are
self-contained, so they are the ones worth wiring into CI.

## Layout

- `fixtures/` — the hand-written `.asl` games, their `.cmd` scripts and their
  `.expected` transcripts. Ours, and committed.
- `goldens/` — the `<title> - command script.txt` walkthroughs we derived
  ourselves, one per corpus game. Committed; the transcripts are not frozen,
  since each game is checked by its win marker instead.
- `games/` — the `.asl`/`.cas` corpus. **Untracked** (`../.gitignore`):
  third-party game data is never committed.
- `harness/` — the runner, the unit tests and the two shell drivers. Binaries
  build here and are gitignored.

## Fixtures (`fixtures/`, `harness/run_fixtures.sh`)

Small hand-written `.asl` games, each paired with a `.cmd` script and a golden
`.expected` transcript. They exist because **the game corpus cannot catch these
bugs**: a shipped game only walks the paths its author happened to walk, so a
crash or a wrong string in an unvisited corner leaves all 108 walkthroughs
byte-identical. Every fixture here was checked against a pre-fix engine and
either crashes it or produces different output; each file's header comment says
what it guards.

```sh
harness/run_fixtures.sh           # PASS/FAIL table
harness/run_fixtures.sh --bless   # regenerate the .expected files
```

The transcripts must be reproducible, so the seed is fixed and no fixture ever
prints a raw random number — `rand()` differs between C libraries. A fixture
that wants to check a random value asserts a *range* instead (see
`fixtures/functions.asl`).

> **Watch out:** several of the bugs these guard are undefined behaviour, and a
> plain `-O2` build does not fault on them — it silently returns garbage. A
> fixture that passes at `-O2` on a knowingly-broken engine is not proof of
> anything; run `make asan` to see them. `fixtures/source.asl` is the clearest
> example: it is a clean pass at `-O2` either way, and a heap-buffer-overflow
> under ASan before the fix.
>
> The reverse also happens once: `scriptdepth` FAILs under `make asan` and only
> there. Its `.expected` was recorded against the production `kMaxScriptDepth`
> of 500, and ASan builds deliberately lower the cap to 100 (`geas-impl.hh`),
> because ASan's redzones inflate `run_script`'s frame enough that 500 nested
> frames overflow the sanitizer's stack before the guard can trip. The recursion
> is abandoned earlier, which is the whole point of the cap — a known, expected
> diff, not a regression.

## Unit tests (`harness/geas_unit_tests.cc`)

For engine code a fixture cannot reach, because no *player input* reaches it: a
save file is untrusted binary input (every count in it is a loop bound, so a
corrupt one used to be an out-of-memory), and a record holding no elements
cannot be conjured from a game script. Run `cd harness && ./geas_unit_tests`;
exit 0 is a pass.

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

Run these from `harness/`. A simple game just needs a win marker:

```sh
./geas_walkthrough_runner --win "won the game" \
  ../games/Magic.asl "../goldens/Magic World - command script.txt"
```

World's End needs all three extras: timer ticking (its dynamite fuse is a
real-time timer whose explosion reveals an item the rest of the game needs),
and save-scum for its two random fights:

```sh
GEAS_SEED=1 ./geas_walkthrough_runner --tick --save-scum \
  --fight "use vial1 on cube|use vial2 on cube=The cube explodes" \
  --fight "use laser pistol on warlord=slumps to the ground dead" \
  --win "slumps to the ground dead" \
  "../games/worldsend/world's end.asl" "../goldens/Worlds End - command script.txt"
```

## Running the whole collection

`harness/run_walkthroughs.sh` plays every game against its script and prints a
PASS/FAIL table. The command scripts live in `goldens/` and are committed, so
with the games in `games/` no arguments are needed:

```sh
harness/run_walkthroughs.sh
```

The games are copyrighted and stay local-only (`games/` is gitignored), so point
the script elsewhere if you keep them somewhere else. Three of the games are
played against a `<title> - walkthrough.txt` written by someone else; those are
not redistributed here, so they SKIP unless you pass a directory holding them:

```sh
harness/run_walkthroughs.sh "/path/to/Geas games" "/path/to/Geas walkthroughs"
```

A walkthrough is looked up in `goldens/` first and in that directory second, so
a local copy never shadows a committed script.
