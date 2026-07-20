#!/bin/bash
# Golden regression for the qvh oracle. For every driven corpus game, replay the
# FROZEN command script committed under golden/<Game>.cmd through QuestViva and
# diff the normalised transcript against the committed golden/<Game>.out.
#
# This deliberately drives from the committed .cmd (not extract_walkthrough.py),
# so a FAIL means the oracle's behaviour drifted — a QuestViva upstream change, a
# .NET/RNG regression, or a Program.cs edit — NOT a walkthrough-extraction tweak.
# (To refresh the goldens after an intended change, run ./update_golden.sh.)
#
# Requires the built oracle (./build.sh) and the corpus games (default
# ~/Downloads/Quest 5 games). Exit status is non-zero if any game diverges, or
# if corpus.tsv and golden/ disagree about which games are covered (see the
# coverage cross-check at the bottom).
export PATH="/opt/homebrew/bin:$PATH"
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
GAMES="${GAMES:-$HOME/Downloads/Quest 5 games}"
GOLDEN="$HERE/golden"
DLL="$HERE/bin/Release/net10.0/qvh.dll"
WORK="$HERE/out"
mkdir -p "$WORK"

if [ ! -f "$DLL" ]; then
  echo "oracle not built: $DLL missing — run ./build.sh first" >&2
  exit 2
fi

# Optional args restrict the run to matching games (case-insensitive substring on
# the game name), so you can re-check a single new golden without replaying all
# ~68 games:  ./check_golden.sh "lost cavern"   (the coverage cross-check below
# always runs over the full corpus regardless of the filter).
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

# The per-game replay+diff is embarrassingly parallel (each is an independent
# `dotnet` invocation), and dotnet's JIT startup dominates each short game, so we
# fan out across cores instead of looping serially (~68 cold starts -> minutes).
JOBS="${GOLDEN_JOBS:-$( (sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4) )}"
check_one() {  # $1 = golden .cmd path; prints one "STATUS<TAB>game<TAB>detail" line
  local cmd="$1" game q gold got
  game="$(basename "$cmd" .cmd)"
  q="$GAMES/$game.quest"; gold="$GOLDEN/$game.out"
  [ -f "$q" ]    || { printf 'MISS\t%s\tgame file missing\n' "$game"; return; }
  [ -f "$gold" ] || { printf 'MISS\t%s\tno golden .out\n' "$game";    return; }
  got="$WORK/$game.golden-check.out"
  dotnet "$DLL" "$q" "$cmd" > "$got" 2>/dev/null
  if diff -q "$gold" "$got" >/dev/null; then
    printf 'PASS\t%s\t\n' "$game"
  else
    printf 'FAIL\t%s\t%s diff lines\n' "$game" "$(diff "$gold" "$got" | grep -c '^[<>]')"
  fi
}
export -f check_one; export GAMES GOLDEN WORK DLL

results="$(mktemp)"; trap 'rm -f "$results"' EXIT
sel_list=()
for cmd in "$GOLDEN"/*.cmd; do
  [ -e "$cmd" ] || { echo "no goldens in $GOLDEN — run ./update_golden.sh" >&2; exit 2; }
  match_filter "$(basename "$cmd" .cmd)" || continue
  sel_list+=("$cmd")
done
sel=${#sel_list[@]}
if [ "$sel" -gt 0 ]; then
  printf '%s\0' "${sel_list[@]}" | xargs -0 -P "$JOBS" -I{} bash -c 'check_one "$@"' _ {} | sort -f -t$'\t' -k2 > "$results"
fi

pass=0; fail=0; miss=0
while IFS=$'\t' read -r status game detail; do
  case "$status" in
    PASS) printf "PASS  %-50s\n" "$game"; pass=$((pass+1));;
    FAIL) printf "FAIL  %-50s (%s)\n" "$game" "$detail"; fail=$((fail+1));;
    MISS) printf "MISS  %-50s (%s)\n" "$game" "$detail"; miss=$((miss+1));;
  esac
done < "$results"
echo "---"
[ "${#FILTERS[@]}" -gt 0 ] && echo "(filtered: $sel of $(ls "$GOLDEN"/*.cmd | wc -l | tr -d ' ') goldens; ${JOBS}-way parallel)"
echo "golden check: $pass passed, $fail failed, $miss missing"

# Coverage cross-check. The loop above iterates golden/*.cmd, so it only ever
# visits games that ALREADY have a golden: a corpus row that is driven but was
# never frozen into golden/ is not regression-checked and nothing complains.
# (run_corpus.sh's counterpart guard catches the other half — a corpus row that
# cannot be driven at all.) Compare the two directions explicitly.
rows="$(mktemp)"; trap 'rm -f "$rows"' EXIT
while IFS=$'\t' read -r game wt mode preamble; do
  case "$game" in ''|\#*) continue;; esac
  [ "$mode" = "hints" ] || printf '%s\n' "$game"
done < "$HERE/corpus.tsv" > "$rows"

ungolden=0; orphan=0
while IFS= read -r game; do
  if [ ! -f "$GOLDEN/$game.cmd" ] || [ ! -f "$GOLDEN/$game.out" ]; then
    printf "NOGOLD  %-48s (driven corpus row, but no frozen golden)\n" "$game"
    ungolden=$((ungolden+1))
  fi
done < "$rows"

for cmd in "$GOLDEN"/*.cmd; do
  [ -e "$cmd" ] || continue
  game="$(basename "$cmd" .cmd)"
  if ! grep -Fxq "$game" "$rows"; then
    printf "ORPHAN  %-48s (golden with no corpus.tsv row — stale?)\n" "$game"
    orphan=$((orphan+1))
  fi
done

echo "coverage: $(wc -l < "$rows" | tr -d ' ') driven corpus rows, $ungolden without a golden, $orphan orphaned golden(s)"
[ "$fail" -eq 0 ] && [ "$ungolden" -eq 0 ] && [ "$orphan" -eq 0 ]
