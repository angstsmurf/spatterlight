#!/bin/bash
# Run the qvh oracle over every corpus game that has a "<Game> - walkthrough.txt"
# Welbourn walkthrough. Emits transcripts to out/ and a coverage summary.
export PATH="/opt/homebrew/bin:$PATH"
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
GAMES="$HOME/Downloads/Quest 5 games"
WALKS="$HOME/Downloads/Quest 5 walkthroughs"
OUT="$HERE/out"
mkdir -p "$OUT"
DLL="$HERE/bin/Release/net10.0/qvh.dll"

printf "%-52s %-6s %5s %5s %-9s\n" "GAME" "ASL" "STEPS" "EMITS" "STATE"
for q in "$GAMES"/*.quest; do
  base="$(basename "$q" .quest)"
  wt="$WALKS/$base - walkthrough.txt"
  [ -f "$wt" ] || continue
  cmd="$OUT/$base.cmd"
  python3 "$HERE/extract_welbourn.py" "$wt" > "$cmd"
  [ -s "$cmd" ] || { printf "%-52s %s\n" "$base" "(no commands extracted)"; continue; }
  dotnet "$DLL" "$q" "$cmd" > "$OUT/$base.out" 2> "$OUT/$base.err"
  diag="$(grep '^\[diag\] end:' "$OUT/$base.err")"
  ver="$(grep -o 'version=[^ ]*' "$OUT/$base.err" | head -1 | cut -d= -f2)"
  steps="$(echo "$diag" | grep -o 'steps=[0-9]*' | cut -d= -f2)"
  emits="$(echo "$diag" | grep -o 'emits=[0-9]*' | cut -d= -f2)"
  state="$(echo "$diag" | grep -o 'state=[A-Za-z]*' | cut -d= -f2)"
  printf "%-52s %-6s %5s %5s %-9s\n" "$base" "${ver:-?}" "${steps:-?}" "${emits:-?}" "${state:-?}"
done
