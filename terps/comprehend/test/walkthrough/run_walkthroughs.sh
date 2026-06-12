#!/bin/sh
# run_walkthroughs.sh -- play the committed Comprehend command scripts against
# their games and print a PASS/FAIL table, modelled on
# geas/test/run_walkthroughs.sh.
#
#   ./run_walkthroughs.sh [games-dir]
#
# The games are not in this repo (copyrighted). Point the script at a directory
# holding the per-game data; it defaults to "~/Downloads/comprehend games".
# Scripts live in ./scripts and are the "raw" form: one input per line.
#
# Each entry names a win marker -- a line the interpreter only prints once the
# script has driven the game to that point -- exactly as the geas runner's
# --win does. Exit status is 0 only if every present game passes.
#
# NOTE on coverage: the Coveted Mirror entry is a timeout-free *smoke* prefix
# (its hourglass ticks per turn even headless, so a full run must bribe the
# jailer on a strict cadence). Fleshing the other games out to full-completion
# scripts is follow-up work; add entries here as those scripts are written.
set -u

here=$(cd "$(dirname "$0")" && pwd)
G=${1:-"$HOME/Downloads/comprehend games"}
RUN="$here/run_walkthrough.sh"
HL="$here/../../comprehend_hl"

# Build the headless interpreter if it is missing or stale.
if [ ! -x "$HL" ]; then
    echo "building comprehend_hl..."
    make -C "$here/../.." -f Makefile.headless >/dev/null || exit 1
fi

pass=0; fail=0; skip=0

# play <label> <gameid> <game-file-relative-to-G> <script> <win-marker>
play() {
    label=$1; gameid=$2; game="$G/$3"; script="$here/scripts/$4"; marker=$5
    if [ ! -f "$game" ] || [ ! -f "$script" ]; then
        printf '%-16s SKIP (missing game or script)\n' "$label"
        skip=$((skip+1)); return
    fi
    if "$RUN" "$gameid" "$game" "$script" "$marker" >/dev/null 2>&1; then
        printf '%-16s PASS\n' "$label"; pass=$((pass+1))
    else
        printf '%-16s FAIL\n' "$label"; fail=$((fail+1))
    fi
}

play CovetedMirror covetedmirror \
    "The Coveted Mirror 2.0 (woz-a-day collection)/The Coveted Mirror 2.0 side B (boot).woz" \
    covetedmirror.txt \
    "My horoscope was horrible."

echo "----"
echo "pass=$pass fail=$fail skip=$skip"
[ "$fail" -eq 0 ]
