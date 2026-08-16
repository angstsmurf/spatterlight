# Quest 4 (`.asl`/`.cas`) tests

The geas engine's own test tree. Three layers, in increasing distance from a
real game:

| what | needs | catches |
| --- | --- | --- |
| `make syntax` (at `../`) | nothing (in-repo) | code that only compiles as one translation unit |
| `harness/run_fixtures.sh` | nothing (in-repo) | engine behaviour, against golden transcripts |
| `harness/geas_unit_tests` | nothing (in-repo) | corrupt saves and parser edges no player can drive |
| `harness/run_walkthroughs.sh` | the games (local; scripts in-repo) | regressions in 111 real games, against golden transcripts |

`make check` (from `../`) runs everything but the last — those are
self-contained, so they are the ones worth wiring into CI.

## Layout

- `fixtures/` — the hand-written `.asl` games, their `.cmd` scripts and their
  `.expected` transcripts. Ours, and committed.
- `goldens/` — two committed files per corpus game: the
  `<title> - command script.txt` walkthrough we derived ourselves, and the
  `<title> - transcript.txt` the engine produces when it replays it. Four of
  the corpus games are pornographic and their transcripts are nothing but that
  prose, so those four are untracked (`../.gitignore`) and regenerated locally
  with `run_walkthroughs.sh --bless`; their command scripts are committed like
  all the others.
- `games/` — the `.asl`/`.cas` corpus. **Untracked** (`../.gitignore`):
  third-party game data is never committed.
- `games.manifest.tsv` — the committed sha256 pin for every file in `games/`,
  with its source URL where one exists. This is what makes the corpus
  reproducible without redistributing it; see `../GAMES.md` and
  `../fetch_games.sh`.
- `harness/` — the runner, the unit tests and the two shell drivers. Binaries
  build here and are gitignored.

About half the corpus is compiled `.cas` rather than readable `.asl`, and you
cannot grep a compiled game. `../../uncas.pl` decompiles one back to source
(`perl ../../uncas.pl game.cas [game.asl]`, or the other way round with an
`.asl` first argument to compile), which is how you find the `items <…>` line
or the `command <…>` a transcript is tripping over. It shares its keyword table
with `readfile.cc`, so the two decode `.cas` identically — a keyword missing
from one is missing from the other, and both were wrong the same way before the
table was completed from the real Quest's `quest.dat`.

`harness/casdump.cc` (`make quest4/harness/casdump` from `../`, then
`casdump game.cas`) is the other view of the same thing: it runs the engine's
own loader and prints every parsed GeasBlock — `!include`s merged, `!addto`
folded in, tag lines already rewritten to `properties` — which is exactly the
text the runtime's lookups walk. Read a game as its author wrote it with
uncas.pl; read what geas *believes* it says with casdump; a disagreement
between the two is a loader bug.

## Fixtures (`fixtures/`, `harness/run_fixtures.sh`)

Small hand-written `.asl` games, each paired with a `.cmd` script and a golden
`.expected` transcript. They exist because **the game corpus cannot catch these
bugs**: a shipped game only walks the paths its author happened to walk, so a
crash or a wrong string in an unvisited corner leaves all 111 walkthroughs
byte-identical. Every fixture here was checked against a pre-fix engine and
either crashes it or produces different output; each file's header comment says
what it guards.

```sh
harness/run_fixtures.sh           # PASS/FAIL table
harness/run_fixtures.sh --bless   # regenerate the .expected files
```

The transcripts must be reproducible, so the seed is fixed, and no fixture
prints a raw random number: one asserting a random value checks a *range*
instead (see `fixtures/functions.asl`). That convention predates the engine
pinning its generator — which now makes a raw draw reproducible too — and is
kept because a range says what the fixture is actually testing.

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
harness/run_walkthroughs.sh              # win marker + transcript diff
harness/run_walkthroughs.sh --bless      # re-record the transcripts
harness/run_walkthroughs.sh --win-only   # markers only, no diffing
```

Each game is checked twice over: the win marker has to appear, **and** the whole
replay has to match `goldens/<title> - transcript.txt` byte for byte. The marker
on its own is a weak test — an engine change that garbles every room description
still reaches the ending, so it still passes. Changing one word of the
`badcommand` message, for instance, leaves all 111 win markers intact and shows
up as four failed transcripts.

A failure prints the head of the diff and how many lines moved; the transcripts
themselves run from 1 KB to 280 KB, 3.6 MB in total. They are *our* engine's
output, not an oracle's, so they freeze current behaviour, bugs included: a
deliberate fix is expected to move them, and `--bless` re-records. What the
diff is for is the change nobody intended — and its size is itself the signal,
since a fix to one message should not move sixty games.

Two things make the transcripts stable enough to diff. The seed is fixed
(`GEAS_SEED=1`), and `$rand(a;b)$` draws from `erkyrath_random()`
(`common_utils/randomness.c`) rather than the C library's `rand()`, because the
seeded draws decide the outcome of 14 of these games and `rand()` is not the
same function on two C libraries. Seeded, `erkyrath_random()` is xoshiro128** —
a fixed algorithm with the same output everywhere — and it is the generator the
Spatterlight build already used, so the headless runner and the app now make
the same draws from the same seed. The harness links `randomness.c` as an
object rather than reimplementing it, which is what keeps the two from drifting
apart.

Switching to it moved the draw sequence, so it re-derived eight of the
walkthroughs: Schoolgirl Jan Ken Pon, Forward and Back, Easy Money, Blight of
Elantria, Shipwrecked, Barbarian, MagicSword Part 1 and Kingdom. Each command
script's header says what its fights and rolls now turn on.

The games are copyrighted and stay local-only (`games/` is gitignored), so point
the script elsewhere if you keep them somewhere else:

```sh
harness/run_walkthroughs.sh "/path/to/Geas games"
```

Both halves of every pair in `goldens/` are ours. Each command script was
derived here by reading the game — its header says what the puzzles turn on and
why the odd-looking moves are there — and the transcript beside it is our
engine's own output. Nothing in `goldens/` reproduces a walkthrough written by
someone else, which is what lets these be committed when the games cannot be.
