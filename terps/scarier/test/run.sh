#!/bin/bash
# usage: A5WORK=<dir> run.sh [n_last_prompts]
#   Replay $A5WORK/wt.txt through a5run_dump against TheFortressOfFear and print
#   the tail from the Nth-from-last prompt.  $A5WORK holds the scratch wt.txt /
#   out.txt (defaults to the current dir); pair with route.py / annotate.py,
#   which read map.tsv / out.txt from the same dir.  Blind-walkthrough
#   derivation tooling — see TODO_a5_walkthrough_wiring.md (⭐ TheFortressOfFear).
cd /Users/administrator/spatterlight/terps/scarier
S=${A5WORK:-$(pwd)}
N=${1:-12}
./test/a5run_dump test/adrift5-games/TheFortressOfFear.blorb "$S/wt.txt" > "$S/out.txt" 2>&1
# print from the Nth-from-last prompt line
total=$(grep -c '^> ' "$S/out.txt")
start=$((total - N + 1)); [ $start -lt 1 ] && start=1
awk -v s=$start '/^> /{c++} c>=s' "$S/out.txt"
