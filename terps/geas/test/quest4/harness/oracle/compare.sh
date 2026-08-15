#!/bin/bash
# Diff every corpus game's geas transcript against QuestViva's (see compare.py).
#
#   ./compare.sh                 # whole corpus, summary table + out/*.diff
#   ./compare.sh Magic Kingdom   # just these labels
#
# Labels, game files, scripts and per-game runner flags all come from
# ../run_walkthroughs.sh, so the two harnesses can never drift apart.
set -euo pipefail
export PATH="/opt/homebrew/bin:$PATH"
HERE="$(cd "$(dirname "$0")" && pwd)"
[ -f "$HERE/bin/Release/net10.0/qv4.dll" ] || "$HERE/build.sh"
exec python3 "$HERE/compare.py" "$@"
