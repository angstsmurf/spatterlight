#!/bin/bash
# Corpus regression for the NATIVE aslx engine. For every frozen command script
# in ../goldens/<Game>.cmd, replay it through aslx_replay and diff the transcript
# against ../goldens/<Game>.txt.
#
# This is the mirror image of oracle/check_golden.sh, over exactly the same two
# files. There, a FAIL means the *oracle* drifted (a QuestViva upstream change, a
# .NET/RNG regression, a Program.cs edit) and the goldens may need re-freezing;
# here, a FAIL means *we* diverged from what real Quest prints, and the golden is
# the answer key. Run the oracle side first when both fail: if QuestViva itself
# moved, everything below is measuring against a stale baseline.
#
#   ./run_replays.sh                  # the whole corpus, in parallel
#   ./run_replays.sh acreage hobbit   # just the matching games
#
# Needs the corpus games, which are third-party and so untracked: prefers
# ../games if it has been set up (the way quest4/games works) and falls back to
# ~/Downloads/Quest 5 games, with GAMES= overriding both. Games with a golden
# but no game file are reported MISS, not FAIL -- but a run that checked nothing
# at all still exits non-zero, since a corpus-free machine must not read green.
#
# Env: GAMES=<dir>, JOBS=<n>, DIFF_LINES=<n> (0 to print no diff excerpt),
#      WORK=<dir> (keep the replayed transcripts; default is a temp dir).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${GAMES:-}" ] && [ -d "$HERE/../games" ]; then GAMES="$HERE/../games"; fi
GAMES="${GAMES:-$HOME/Downloads/Quest 5 games}"
GOLDEN="$HERE/../goldens"
REPLAY="$HERE/aslx_replay"
# The engine's Core library, which every game loads before its own .aslx.
export ASLX_CORE="$HERE/../../../quest5/aslx-core"

# The goldens are QuestViva's output under its default seed, which aslx_replay
# matches with its own default of 1234 -- an inherited ASLX_SEED would simply
# fail every game that draws a random number. ASLX_GRID_TRACE is worse than
# noisy: installing the grid hook switches the JS.* bridge onto its grid
# vocabulary, so it changes what is replayed. Neither belongs in a regression.
unset ASLX_SEED ASLX_GRID_TRACE ASLX_RESTORE

DIFF_LINES="${DIFF_LINES:-20}"

# aslx_replay unity-includes the engine sources, so a binary newer than
# aslx_replay.cc can still be older than the edit being tested; ask make every
# time (a no-op when nothing has changed). It is not part of `make check` --
# `check` is deliberately corpus-free -- so it may not have been built at all.
make -C "$HERE/../.." quest5/harness/aslx_replay >/dev/null || exit 2
[ -x "$REPLAY" ] || { echo "no $REPLAY" >&2; exit 2; }

# Optional args restrict the run to matching games (case-insensitive substring
# on the game name), so a single new golden can be re-checked without replaying
# the whole corpus.
FILTERS=("$@")
match_filter() {  # $1 = game name; true if no filters, or any filter matches
  [ "${#FILTERS[@]}" -eq 0 ] && return 0
  local g; g="$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')"
  local f
  for f in "${FILTERS[@]}"; do
    case "$g" in *"$(printf '%s' "$f" | tr '[:upper:]' '[:lower:]')"*) return 0;; esac
  done
  return 1
}

keepwork=yes
if [ -n "${WORK:-}" ]; then
  mkdir -p "$WORK"
else
  keepwork=no
  WORK="$(mktemp -d "${TMPDIR:-/tmp}/aslx-replays.XXXXXX")"
  trap 'rm -rf "$WORK"' EXIT INT TERM
fi

# Each game is an independent process, and the whole corpus is a couple of
# minutes serially, so fan out across cores like the oracle's check does.
JOBS="${JOBS:-$( (sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4) )}"
replay_one() {  # $1 = golden .cmd path; prints one "STATUS<TAB>game<TAB>detail" line
  local cmd="$1" game q gold got
  game="$(basename "$cmd" .cmd)"
  q="$GAMES/$game.quest"; gold="$GOLDEN/$game.txt"
  [ -f "$q" ]    || { printf 'MISS\t%s\tgame file missing\n' "$game"; return; }
  [ -f "$gold" ] || { printf 'MISS\t%s\tno golden .txt\n' "$game";    return; }
  got="$WORK/$game.replay.txt"
  "$REPLAY" "$q" "$cmd" > "$got" 2>/dev/null
  if diff -q "$gold" "$got" >/dev/null; then
    printf 'PASS\t%s\t\n' "$game"
  else
    printf 'FAIL\t%s\t%s diff lines\n' "$game" "$(diff "$gold" "$got" | grep -c '^[<>]')"
  fi
}
export -f replay_one; export GAMES GOLDEN WORK REPLAY

results="$(mktemp)"
sel_list=()
for cmd in "$GOLDEN"/*.cmd; do
  [ -e "$cmd" ] || { echo "no goldens in $GOLDEN" >&2; exit 2; }
  match_filter "$(basename "$cmd" .cmd)" || continue
  sel_list+=("$cmd")
done
sel=${#sel_list[@]}
if [ "$sel" -gt 0 ]; then
  printf '%s\0' "${sel_list[@]}" | xargs -0 -P "$JOBS" -I{} bash -c 'replay_one "$@"' _ {} \
    | sort -f -t$'\t' -k2 > "$results"
fi

pass=0; fail=0; miss=0
while IFS=$'\t' read -r status game detail; do
  case "$status" in
    PASS) printf "PASS  %-50s\n" "$game"; pass=$((pass+1));;
    MISS) printf "MISS  %-50s (%s)\n" "$game" "$detail"; miss=$((miss+1));;
    FAIL) printf "FAIL  %-50s (%s)\n" "$game" "$detail"; fail=$((fail+1))
          # The transcripts run to hundreds of KB, so show only the head of the
          # diff: golden on the left ("<"), what we printed on the right (">").
          if [ "$DIFF_LINES" -gt 0 ]; then
            diff "$GOLDEN/$game.txt" "$WORK/$game.replay.txt" \
              | head -n "$DIFF_LINES" | sed 's/^/      /'
          fi;;
  esac
done < "$results"
rm -f "$results"

total=$(ls "$GOLDEN"/*.cmd | wc -l | tr -d ' ')
echo "---"
[ "${#FILTERS[@]}" -gt 0 ] && echo "(filtered: $sel of $total goldens; ${JOBS}-way parallel)"
echo "native replay: $pass passed, $fail failed, $miss missing"
if [ "$fail" -gt 0 ]; then
  if [ "$keepwork" = yes ]; then echo "full transcripts in $WORK"
  else echo "re-run with WORK=<dir> to keep the full transcripts"; fi
fi

# Nothing replayed means no corpus, not a clean bill of health.
if [ "$((pass + fail))" -eq 0 ]; then
  echo "no games were replayed -- is the corpus present? (GAMES=$GAMES)" >&2
  exit 2
fi
[ "$fail" -eq 0 ]
