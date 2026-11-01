#!/bin/sh
# run_save_resume.sh -- save late in each game, then finish it from that save in
# a new process, so the game starts on its first part and has to load the part
# the save came from (Crimson Crown disk 2, the Talisman desert).
#
#   ./run_save_resume.sh [games-dir]
#
# #savestate keeps the RNG position, so the rest of the script plays as it does
# in one sitting. Exit status is 0 only if every present game passes.
set -u

here=$(cd "$(dirname "$0")" && pwd)
G=${1:-"$here/games"}
HL="$here/../../comprehend_hl"
[ -x "$HL" ] || { echo "missing $HL (run: make -f Makefile.headless)" >&2; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0; skip=0

# play <label> <gameid> <game-file-relative-to-G> <script> <win-marker> [prompts]
# [prompts] counts the script lines answering prompts before the first turn. The
# second run answers them again, as a new process asks again.
play() {
    label=$1; gameid=$2; game="$G/$3"; script="$here/scripts/$4"; marker=$5; prompts=${6:-0}
    if [ ! -f "$game" ] || [ ! -f "$script" ]; then
        printf '%-16s SKIP (missing game or script)\n' "$label"
        skip=$((skip+1)); return
    fi
    grep -v '^[[:space:]]*#' "$script" | grep -v '^[[:space:]]*$' > "$tmp/all"
    n=$(wc -l < "$tmp/all"); k=$((n * 85 / 100))
    { head -n "$k" "$tmp/all"; echo "#savestate $tmp/$label.state"; } > "$tmp/first"
    { sed -n "1,${prompts}p" "$tmp/all"; echo "#loadstate $tmp/$label.state"; tail -n +"$((k + 1))" "$tmp/all"; } > "$tmp/second"
    COMPREHEND_SCRIPT="$tmp/first" "$HL" -g "$gameid" "$game" >/dev/null 2>&1
    out=$(COMPREHEND_SCRIPT="$tmp/second" "$HL" -g "$gameid" "$game" 2>&1)
    if printf '%s' "$out" | grep -qF "Load failed."; then
        printf '%-16s FAIL (load)\n' "$label"; fail=$((fail+1))
    elif printf '%s' "$out" | grep -qiF "$marker"; then
        printf '%-16s PASS\n' "$label"; pass=$((pass+1))
    else
        printf '%-16s FAIL (no win)\n' "$label"; fail=$((fail+1))
    fi
}

play CovetedMirror covetedmirror \
    "The Coveted Mirror 2.0 (woz-a-day collection)/The Coveted Mirror 2.0 side B (boot).woz" \
    covetedmirror.txt "Congratulations!!"
play OoTopos ootopos \
    "Oo-Topos (woz-a-day collection)/Oo-Topos side B.woz" \
    ootopos.txt "But your part has been done!"
play CrimsonCrown crimsoncrown \
    "The Crimson Crown (woz-a-day collection)/The Crimson Crown side A.woz" \
    crimsoncrown.txt "THE END."
play Talisman talisman \
    "Talisman- Challenging the Sands of Time (woz-a-day collection)/Talisman- Challenging the Sands of Time side 1 - Boot.woz" \
    talisman.txt "the Empire shalt flourish through eternity"
play Transylvania transylvania \
    "Transylvania 1985 (woz-a-day collection)/Transylvania 1985 side A.woz" \
    transylvania.txt "Well Done!!!" 2
play TransylvaniaV2 transylvaniav2 \
    "TransylvaniaPC/G0" \
    transylvania.txt "Well Done!!!" 2

echo "----"
echo "pass=$pass fail=$fail skip=$skip"
[ "$fail" -eq 0 ]
