#!/bin/bash
# Run the qvh oracle over every corpus game listed in corpus.tsv, using the
# extractor mode recorded there. Emits transcripts + extracted scripts to out/
# and prints a coverage table. `hints` rows are reported as skipped (no linear
# command script exists for them).
export PATH="/opt/homebrew/bin:$PATH"
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
GAMES="${GAMES:-$HOME/Downloads/Quest 5 games}"
WALKS="${WALKS:-$HOME/Downloads/Quest 5 walkthroughs}"
OUT="$HERE/out"
mkdir -p "$OUT"
DLL="$HERE/bin/Release/net10.0/qvh.dll"

printf "%-52s %-6s %6s %6s %-9s\n" "GAME" "ASL" "STEPS" "EMITS" "STATE"
fin=0; run=0; skip=0
while IFS=$'\t' read -r game wt mode; do
  case "$game" in ''|\#*) continue;; esac
  if [ "$mode" = "hints" ]; then
    printf "%-52s %-6s %6s %6s %-9s\n" "$game" "-" "-" "-" "hints"
    skip=$((skip+1)); continue
  fi
  q="$GAMES/$game.quest"
  src="$WALKS/$wt"
  if [ ! -f "$q" ];   then printf "%-52s  (game file missing)\n" "$game"; continue; fi
  if [ ! -f "$src" ]; then printf "%-52s  (walkthrough missing: %s)\n" "$game" "$wt"; continue; fi
  cmd="$OUT/$game.cmd"
  python3 "$HERE/extract_walkthrough.py" --mode "$mode" "$src" > "$cmd"
  [ -s "$cmd" ] || { printf "%-52s  (no commands extracted)\n" "$game"; continue; }
  dotnet "$DLL" "$q" "$cmd" > "$OUT/$game.out" 2> "$OUT/$game.err"
  diag="$(grep '^\[diag\] end:' "$OUT/$game.err")"
  ver="$(grep -o 'version=[^ ]*' "$OUT/$game.err" | head -1 | cut -d= -f2)"
  steps="$(echo "$diag" | grep -o 'steps=[0-9]*' | cut -d= -f2)"
  emits="$(echo "$diag" | grep -o 'emits=[0-9]*' | cut -d= -f2)"
  state="$(echo "$diag" | grep -o 'state=[A-Za-z]*' | cut -d= -f2)"
  printf "%-52s %-6s %6s %6s %-9s\n" "$game" "${ver:-?}" "${steps:-?}" "${emits:-?}" "${state:-?}"
  [ "$state" = "Finished" ] && fin=$((fin+1)) || run=$((run+1))
done < "$HERE/corpus.tsv"
echo "---"
echo "driven: $((fin+run))  (Finished: $fin, Running: $run)   hints-only skipped: $skip"
