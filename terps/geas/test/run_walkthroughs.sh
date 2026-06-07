#!/bin/sh
# Play a collection of Quest games against their walkthroughs and print a
# PASS/FAIL table.  The games and walkthroughs are not part of this repo; pass
# the two directories on the command line.
#
#   ./run_walkthroughs.sh "<games dir>" "<walkthroughs dir>"
#
# Builds the runner if needed.  Uses a fixed RNG seed for reproducibility;
# World's End's two random fights are won by --save-scum regardless of seed.

set -u
here=$(cd "$(dirname "$0")" && pwd)
G=${1:-}
W=${2:-}
if [ -z "$G" ] || [ -z "$W" ]; then
    echo "usage: $0 \"<games dir>\" \"<walkthroughs dir>\"" >&2
    exit 2
fi

RUN="$here/geas_walkthrough_runner"
[ -x "$RUN" ] && [ "$RUN" -nt "$here/geas_walkthrough_runner.cc" ] || make -C "$here" >/dev/null || exit 1

export GEAS_SEED=1
pass=0; fail=0

# play  <label>  <game>  <walkthrough>  <win-marker>  [extra runner args...]
play() {
    label=$1; game=$2; wt=$3; marker=$4; shift 4
    if [ ! -f "$G/$game" ] || [ ! -f "$W/$wt" ]; then
        printf '%-22s SKIP (missing files)\n' "$label"; return
    fi
    if "$RUN" "$@" --win "$marker" "$G/$game" "$W/$wt" >/dev/null 2>&1; then
        printf '%-22s PASS\n' "$label"; pass=$((pass+1))
    else
        printf '%-22s FAIL\n' "$label"; fail=$((fail+1))
    fi
}

play Adventure          "adventure.cas"               "Adventure - walkthrough.txt"                              "Credits room"
play Assassination      "attempted_assassination.asl" "Attempted Assassination - walkthrough.txt"                "YOU WIN"
play BrokenMirror       "BMTSFD.asl"                  "Broken Mirror - The Screaming Fountain - walkthrough.txt" "completed the rather short demo"
play FadeToWhite        "White.asl"                   "Fade to White - walkthrough.txt"                          "END OF DEMO"
play Koww               "KOWW1.ASL"                   "Koww the Magician - walkthrough.txt"                      "over the chasm"
play MagicWorld         "Magic.asl"                   "Magic World - walkthrough.txt"                            "won the game"
play SirLoin            "sirloin.cas"                 "Sir Loin and the coming of age - walkthrough.txt"         "To be continued"
play Space              "space.asl"                   "Space - The Final Fuck Up - walkthrough.txt"              "pint or two"
play Uranus             "uranus.asl"                  "Uranus or Bust - walkthrough.txt"                         "BOOOOOM"
play GatheredInDarkness "Gatheredindarkness.cas"      "Gathered in Darkness - walkthrough.txt"                   "Congrats"
play Mansion            "mansion.asl"                 "The Mansion - walkthrough.txt"                            "completed the game"
play WorldsEnd          "worldsend/world's end.asl"   "Worlds End - command script (raw).txt"                    "slumps to the ground dead" \
    --tick --save-scum \
    --fight "use vial1 on cube|use vial2 on cube=The cube explodes" \
    --fight "use laser pistol on warlord=slumps to the ground dead"

echo "----"
echo "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
