#!/bin/sh
# strings_oracle.sh -- every Coveted Mirror string, ours against the original's.
#
#   strings_oracle.sh [side-B.woz]
#
# COMPREHEND_STRDUMP makes the headless interpreter write every decoded string
# of both tables and exit; this diffs that against fixtures/original_strings.txt,
# the same strings decoded by the original interpreter running in MAME
# (captured with strings_oracle.lua; local-only, it is the game's text).
#
# Compared: S1 0-154 and S2 0-744. S2 745-767 are stale slots at the end of the
# offset table of bank ML: they point into the middle of other strings or past
# the data, nothing in the game references them (no PRINT or random-message
# instruction, room or item), and the two decoders make different junk of them.
# Exit status 0 when every compared string is identical (or the local-only
# fixture or game is absent, which prints SKIP).
set -u

here=$(cd "$(dirname "$0")" && pwd)
HL="$here/../../comprehend_hl"
GOLD="$here/fixtures/original_strings.txt"
game=${1:-"$here/../walkthrough/games/The Coveted Mirror 2.0 (woz-a-day collection)/The Coveted Mirror 2.0 side B (boot).woz"}

[ -x "$HL" ]   || { echo "missing $HL (run: make -f Makefile.headless)" >&2; exit 2; }
# The fixture and the game are local-only; without them this is a skip, not a
# failure, so `make check` can run it everywhere.
[ -f "$GOLD" ] || { echo "strings oracle: SKIP (no $GOLD; capture it with strings_oracle.lua)"; exit 0; }
[ -f "$game" ] || { echo "strings oracle: SKIP (no game file $game)"; exit 0; }

ours=$(mktemp); trap 'rm -f "$ours"' EXIT
COMPREHEND_STRDUMP="$ours" "$HL" -g covetedmirror "$game" >/dev/null 2>&1

python3 -I - "$ours" "$GOLD" <<'EOF'
import sys

def load(path):
    d = {}
    for line in open(path, encoding="latin-1"):
        table, index, text = line.rstrip("\n").split(" ", 2)
        d[(table, int(index))] = text
    return d

ours, gold = load(sys.argv[1]), load(sys.argv[2])
bad = n = 0
for key in sorted(gold):
    if key[0] == "S2" and key[1] > 744:
        continue
    n += 1
    if ours.get(key) != gold[key]:
        bad += 1
        print("MISMATCH %s %d\n  original: %r\n  ours:     %r" % (key[0], key[1], gold[key], ours.get(key)))
print("%d strings compared, %d mismatches" % (n, bad))
sys.exit(1 if bad or n != 900 else 0)
EOF
