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

pass=0; fail=0; miss=0
for cmd in "$GOLDEN"/*.cmd; do
  [ -e "$cmd" ] || { echo "no goldens in $GOLDEN — run ./update_golden.sh" >&2; exit 2; }
  game="$(basename "$cmd" .cmd)"
  q="$GAMES/$game.quest"
  gold="$GOLDEN/$game.out"
  if [ ! -f "$q" ];    then printf "MISS  %-50s (game file missing)\n" "$game"; miss=$((miss+1)); continue; fi
  if [ ! -f "$gold" ]; then printf "MISS  %-50s (no golden .out)\n" "$game";    miss=$((miss+1)); continue; fi
  got="$WORK/$game.golden-check.out"
  dotnet "$DLL" "$q" "$cmd" > "$got" 2>/dev/null
  if diff -q "$gold" "$got" >/dev/null; then
    printf "PASS  %-50s\n" "$game"
    pass=$((pass+1))
  else
    printf "FAIL  %-50s (%s)\n" "$game" "$(diff "$gold" "$got" | grep -c '^[<>]') diff lines"
    fail=$((fail+1))
  fi
done
echo "---"
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
