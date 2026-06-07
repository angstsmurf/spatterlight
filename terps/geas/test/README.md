# geas headless walkthrough runner

`geas_walkthrough_runner` drives the geas core directly (no Glk) so a Quest
game can be played from a command script and checked for completion in a
script/CI. It unity-includes the engine sources from `../`, the same approach
as `SpatterlightTests/GeasRegressionTests.mm`.

## Build

```sh
make            # builds ./geas_walkthrough_runner
make clean
```

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
./geas_walkthrough_runner --win "won the game" Magic.asl "Magic World - walkthrough.txt"
```

World's End needs all three extras: timer ticking (its dynamite fuse is a
real-time timer whose explosion reveals an item the rest of the game needs),
and save-scum for its two random fights:

```sh
GEAS_SEED=1 ./geas_walkthrough_runner --tick --save-scum \
  --fight "use vial1 on cube|use vial2 on cube=The cube explodes" \
  --fight "use laser pistol on warlord=slumps to the ground dead" \
  --win "slumps to the ground dead" \
  "worldsend/world's end.asl" "Worlds End - command script (raw).txt"
```

## Running a whole collection

`run_walkthroughs.sh` plays a set of games against their scripts and prints a
PASS/FAIL table. The games and walkthroughs are not in this repo; point the
script at your own copies:

```sh
./run_walkthroughs.sh "/path/to/Geas games" "/path/to/Geas walkthroughs"
```
