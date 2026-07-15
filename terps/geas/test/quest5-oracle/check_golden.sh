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
# ~/Downloads/Quest 5 games). Exit status is non-zero if any game diverges.
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
[ "$fail" -eq 0 ]
