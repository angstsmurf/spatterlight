#!/bin/sh
# run_walkthroughs.sh -- play the committed Comprehend command scripts against
# their games and print a PASS/FAIL table, modelled on
# geas/test/run_walkthroughs.sh.
#
#   ./run_walkthroughs.sh [games-dir]
#
# The games are copyrighted, so they are kept local-only in ./games (gitignored,
# see .gitignore); the games dir defaults there but can be overridden with an
# argument. Scripts live in ./scripts and are the "raw" form: one input per line.
#
# Each entry names a win marker -- a line the interpreter only prints once the
# script has driven the game to that point -- exactly as the geas runner's
# --win does. Exit status is 0 only if every present game passes.
#
# NOTE on coverage: the Coveted Mirror entry is a FULL playthrough to the win
# ("Congratulations!!"), riding the hourglass bribe cadence the whole way. The
# other games are smoke prefixes; fleshing them out to full-completion scripts
# is follow-up work.
set -u

here=$(cd "$(dirname "$0")" && pwd)
G=${1:-"$here/games"}
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
    "Congratulations!!"

play OoTopos ootopos \
    "Oo-Topos (woz-a-day collection)/Oo-Topos side B.woz" \
    ootopos.txt \
    "Pushing the green button causes the panel to swing open."

play CrimsonCrown crimsoncrown \
    "The Crimson Crown (woz-a-day collection)/The Crimson Crown side A.woz" \
    crimsoncrown.txt \
    "On a stalagmite rests a pulsating crystal ball."

play Talisman talisman \
    "Talisman- Challenging the Sands of Time (woz-a-day collection)/Talisman- Challenging the Sands of Time side 1 - Boot.woz" \
    talisman.txt \
    "A horrific figure of a demon is embedded within the room's southern wall"

play Transylvania transylvania \
    "Transylvania 1985 (woz-a-day collection)/Transylvania 1985 side A.woz" \
    transylvania.txt \
    "Sabrina dies at dawn!"

echo "----"
echo "pass=$pass fail=$fail skip=$skip"
[ "$fail" -eq 0 ]
