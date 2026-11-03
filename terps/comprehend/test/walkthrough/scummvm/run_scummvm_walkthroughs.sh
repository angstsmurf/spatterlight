#!/bin/sh
# run_scummvm_walkthroughs.sh -- play the committed Comprehend walkthrough
# scripts (../scripts) against the ScummVM Glk Comprehend port and print a
# PASS/FAIL table with an AddressSanitizer error count per game.
#
#   ./run_scummvm_walkthroughs.sh <scummvm-checkout> [games-dir] [work-dir]
#
# <scummvm-checkout> is a git checkout of the port (the angstsmurf/scummvm
# branch spatterlight-comprehend-port). It is not modified: a detached
# worktree of its HEAD is made in <work-dir>/wt, hooks.patch is applied there,
# and it is built with the null backend and ASan. An existing build in
# <work-dir>/wt is reused; delete it to rebuild. games-dir defaults to
# ../games, work-dir to ./work (gitignored).
#
# hooks.patch makes the engine read its commands from $COMPREHEND_SCRIPT,
# echo them, copy the bottom-window text to stdout, answer every key prompt
# with Return and quit at the end of the script.
#
# Environment: SEED (default 1234; the scripts are tuned to win at it),
# JOBS (default 5; more ASan runs at once slow the slow-draw games past the
# 900 s per-run alarm).
set -u

here=$(cd "$(dirname "$0")" && pwd)
if [ $# -lt 1 ]; then
    echo "usage: $0 <scummvm-checkout> [games-dir] [work-dir]" >&2; exit 2
fi
SRC=$(cd "$1" && pwd) || exit 2
G=$(cd "${2:-$here/../games}" && pwd) || exit 2
mkdir -p "${3:-$here/work}" || exit 2
W=$(cd "${3:-$here/work}" && pwd)
SEED=${SEED:-1234}
JOBS=${JOBS:-5}
export SEED W G here

# Child mode: run one table row, "label|gameid|dir|script|marker".
if [ "${RUN_ONE:-}" ]; then
    IFS='|' read -r label id dir script marker <<ROW
$RUN_ONE
ROW
    R="$W/run/$label"; rm -rf "$R"; mkdir -p "$R/save"
    if [ ! -d "$G/$dir" ]; then
        printf '%-16s SKIP (missing %s)\n' "$label" "$dir"; exit 0
    fi
    printf '[scummvm]\nenable_unsupported_game_warning=false\n' > "$R/rc"
    grep -v '^[[:space:]]*#' "$here/../scripts/$script" |
        grep -v '^[[:space:]]*$' > "$R/cmds"
    cd "$W/wt" || exit 1
    COMPREHEND_SCRIPT="$R/cmds" perl -e 'alarm 900; exec @ARGV' \
        ./scummvm --random-seed="$SEED" -c "$R/rc" --savepath="$R/save" \
        -p "$G/$dir" "$id" > "$R/out" 2>&1
    asan=$(grep -c "ERROR: AddressSanitizer" "$R/out")
    if grep -qF "$marker" "$R/out"; then r=PASS; else r=FAIL; fi
    printf '%-16s %s asan=%s\n' "$label" "$r" "$asan"
    exit 0
fi

# Build.
if [ ! -x "$W/wt/scummvm" ]; then
    if [ ! -d "$W/wt" ]; then
        git -C "$SRC" worktree add --quiet --detach "$W/wt" HEAD || exit 1
        git -C "$W/wt" apply "$here/hooks.patch" || exit 1
    fi
    echo "building ScummVM in $W/wt (log in $W/build.log)..."
    ( cd "$W/wt" &&
      ./configure --backend=null --disable-all-engines --enable-engine=glk \
          --disable-detection-full --enable-debug --enable-asan &&
      make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" ) > "$W/build.log" 2>&1 ||
        { echo "build failed, see $W/build.log" >&2; exit 1; }
fi

# label|gameid|game folder under games-dir|script|win marker
# The markers are the ones run_walkthroughs.sh uses.
cat > "$W/table" <<'TABLE'
CovetedMirror|covetedmirror|The Coveted Mirror 2.0 (woz-a-day collection)|covetedmirror.txt|Congratulations!!
OoTopos|ootopos|Oo-Topos (woz-a-day collection)|ootopos.txt|But your part has been done!
CrimsonCrown|crimsoncrown|The Crimson Crown (woz-a-day collection)|crimsoncrown.txt|THE END.
Talisman|talisman|Talisman- Challenging the Sands of Time (woz-a-day collection)|talisman.txt|the Empire shalt flourish through eternity
Transylvania|transylvania|Transylvania 1985 (woz-a-day collection)|transylvania.txt|Well Done!!!
TransylvaniaV2|transylvaniav2|TransylvaniaPC|transylvania.txt|Well Done!!!
OoTopos-DOS|ootopos|oo-topos|ootopos.txt|But your part has been done!
CrimsonCrown-DOS|crimsoncrown|crimson-crown|crimsoncrown.txt|THE END.
Talisman-DOS|talisman|talisman-challenging-the-sands-of-time|talisman.txt|the Empire shalt flourish through eternity
Transylvania-DOS|transylvania|transylvania|transylvania.txt|Well Done!!!
TABLE

echo "seed $SEED, $JOBS at a time; transcripts in $W/run/<label>/out"
export SRC
n=$(wc -l < "$W/table")
seq 1 "$n" | xargs -P "$JOBS" -n1 sh -c \
    'RUN_ONE=$(sed -n "${1}p" "$W/table") "$0" "$SRC" "$G" "$W" > "$W/result.$1"' \
    "$here/run_scummvm_walkthroughs.sh"

# Print in table order; exit 0 only if nothing failed or tripped ASan.
status=0
for i in $(seq 1 "$n"); do
    cat "$W/result.$i"
    grep -q 'FAIL\|asan=[1-9]' "$W/result.$i" && status=1
    rm -f "$W/result.$i"
done
exit $status
