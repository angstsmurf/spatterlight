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

printf "%-52s %-6s %6s %6s %4s %-9s\n" "GAME" "ASL" "STEPS" "EMITS" "ERR" "STATE"
fin=0; run=0; wedge=0; skip=0
while IFS=$'\t' read -r game wt mode preamble; do
  case "$game" in ''|\#*) continue;; esac
  if [ "$mode" = "hints" ]; then
    printf "%-52s %-6s %6s %6s %-9s\n" "$game" "-" "-" "-" "hints"
    skip=$((skip+1)); continue
  fi
  q="$GAMES/$game.quest"
  src="$WALKS/$wt"
  if [ ! -f "$q" ];   then printf "%-52s  (game file missing)\n" "$game"; continue; fi
  # wt="-" marks an override-only row: no source walkthrough exists anywhere
  # (the overrides/ script was derived from the game source), so there is no
  # file to existence-check — the override itself is required instead.
  if [ "$wt" = "-" ]; then
    if [ ! -f "$HERE/overrides/$game.cmd" ]; then printf "%-52s  (override missing for wt=- row)\n" "$game"; continue; fi
  elif [ ! -f "$src" ]; then printf "%-52s  (walkthrough missing: %s)\n" "$game" "$wt"; continue; fi
  cmd="$OUT/$game.cmd"
  # A curated override wins outright: overrides/<game>.cmd is a hand-authored
  # winning script (verbatim, incl. any title-screen preamble baked in) for a game
  # whose raw walkthrough does NOT reach the ending as-is — a walkthrough typo, a
  # branch that must be chosen, a nav error vs the release map, or an RNG-/timer-
  # specific solve. Each override carries a "#"-comment header documenting exactly
  # how it deviates from the raw walkthrough. When present it REPLACES both the
  # preamble and the extractor. See overrides/README.md.
  if [ -f "$HERE/overrides/$game.cmd" ]; then
    cp "$HERE/overrides/$game.cmd" "$cmd"
  else
  # Optional 4th column: ";"-separated commands prepended before the walkthrough
  # (e.g. "new game" to click past a title-screen menu the walkthrough assumes
  # you already dismissed — without it every later command hits the menu and
  # fails). Kept out of extract_walkthrough.py because it is game-, not
  # format-specific.
  : > "$cmd"
  if [ -n "${preamble:-}" ]; then
    printf '%s\n' "$preamble" | tr ';' '\n' | sed 's/^ *//; s/ *$//' >> "$cmd"
  fi
  python3 "$HERE/extract_walkthrough.py" --mode "$mode" "$src" >> "$cmd"
  fi
  [ -s "$cmd" ] || { printf "%-52s  (no commands extracted)\n" "$game"; continue; }
  dotnet "$DLL" "$q" "$cmd" > "$OUT/$game.out" 2> "$OUT/$game.err"
  diag="$(grep '^\[diag\] end:' "$OUT/$game.err")"
  ver="$(grep -o 'version=[^ ]*' "$OUT/$game.err" | head -1 | cut -d= -f2)"
  steps="$(echo "$diag" | grep -o 'steps=[0-9]*' | cut -d= -f2)"
  emits="$(echo "$diag" | grep -o 'emits=[0-9]*' | cut -d= -f2)"
  errs="$(echo "$diag" | grep -o 'errors=[0-9]*' | cut -d= -f2)"
  state="$(echo "$diag" | grep -o 'state=[A-Za-z]*' | cut -d= -f2)"
  printf "%-52s %-6s %6s %6s %4s %-9s\n" "$game" "${ver:-?}" "${steps:-?}" "${emits:-?}" "${errs:-?}" "${state:-?}"
  case "$state" in
    Finished) fin=$((fin+1));;
    Wedged)   wedge=$((wedge+1));;
    *)        run=$((run+1));;
  esac
done < "$HERE/corpus.tsv"
echo "---"
echo "driven: $((fin+run+wedge))  (Finished: $fin, Running: $run, Wedged: $wedge)   hints-only skipped: $skip"
