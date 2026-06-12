#!/bin/sh
# run_walkthrough.sh -- play one Comprehend game from a command script and check
# for a win marker, the same idea as geas/test/geas_walkthrough_runner.
#
#   run_walkthrough.sh <gameid> <game-file> <script> <win-marker>
#
# <script> is one input per line (blank lines and #-comments ignored); this is
# the "raw" form geas commits.  It is fed to comprehend_hl via COMPREHEND_SCRIPT
# and the whole transcript is scanned for <win-marker>.  Exit status is 0 when
# the marker is seen, else 1 (so it slots straight into run_walkthroughs.sh / CI).
#
# The interpreter has no real-time clock here, so timed hazards (the Coveted
# Mirror hourglass, the Crimson Crown sand timer) never fire -- a script only has
# to be spatially correct, not fast.
set -u

here=$(cd "$(dirname "$0")" && pwd)
HL="$here/../../comprehend_hl"

gameid=${1:-}; game=${2:-}; script=${3:-}; marker=${4:-}
if [ -z "$gameid" ] || [ -z "$game" ] || [ -z "$script" ] || [ -z "$marker" ]; then
    echo "usage: $0 <gameid> <game-file> <script> <win-marker>" >&2
    exit 2
fi
[ -x "$HL" ] || { echo "missing $HL (run: make -f Makefile.headless)" >&2; exit 2; }
[ -f "$game" ]   || { echo "missing game file: $game" >&2; exit 2; }
[ -f "$script" ] || { echo "missing script: $script" >&2; exit 2; }

# Strip comments/blank lines into a clean command file.
cmds=$(mktemp); trap 'rm -f "$cmds"' EXIT
grep -v '^[[:space:]]*#' "$script" | grep -v '^[[:space:]]*$' > "$cmds"

out=$(COMPREHEND_SCRIPT="$cmds" "$HL" -g "$gameid" "$game" 2>&1)

if printf '%s' "$out" | grep -qiF "$marker"; then
    exit 0
else
    # On failure, surface the tail so a desync is easy to spot.
    printf '%s\n' "$out" | tail -25 >&2
    exit 1
fi
