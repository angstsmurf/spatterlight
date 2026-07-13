#!/bin/sh
# Play each fixture game in ./fixtures against its command script and compare
# the transcript to the golden .expected file next to it.
#
#   ./run_fixtures.sh            check every fixture (PASS/FAIL table)
#   ./run_fixtures.sh --bless    (re)generate the .expected files
#
# Unlike run_walkthroughs.sh, everything here is in the repo: the fixtures are
# tiny hand-written .asl games that pin down engine behaviour the game corpus
# cannot reach (an exercised bug is usually a crash or a wrong string in a
# corner no shipped game happens to touch).  Each fixture's header comment says
# which behaviour it guards.
#
# The transcripts must be reproducible, so the seed is fixed -- and no fixture
# prints a raw random number, since rand() differs between C libraries.

set -u
here=$(cd "$(dirname "$0")" && pwd)
F="$here/fixtures"
RUN="$here/geas_walkthrough_runner"

bless=no
[ $# -gt 0 ] && [ "$1" = "--bless" ] && bless=yes

[ -x "$RUN" ] && [ "$RUN" -nt "$here/geas_walkthrough_runner.cc" ] || make -C "$here" >/dev/null || exit 1

export GEAS_SEED=1
pass=0; fail=0

for game in "$F"/*.asl; do
    label=$(basename "$game" .asl)
    script="$F/$label.cmd"
    [ -f "$script" ] || continue          # e.g. a library included by another
    expected="$F/$label.expected"
    got=$("$RUN" --echo --seed 1 "$game" "$script" 2>&1)
    if [ "$bless" = yes ]; then
        printf '%s\n' "$got" > "$expected"
        printf '%-16s BLESSED\n' "$label"
        continue
    fi
    if [ ! -f "$expected" ]; then
        printf '%-16s NO GOLDEN (run with --bless)\n' "$label"; fail=$((fail+1)); continue
    fi
    if printf '%s\n' "$got" | diff -u "$expected" - > /tmp/geas_fixture_diff.$$ 2>&1; then
        printf '%-16s PASS\n' "$label"; pass=$((pass+1))
    else
        printf '%-16s FAIL\n' "$label"; fail=$((fail+1))
        sed 's/^/    /' /tmp/geas_fixture_diff.$$
    fi
    rm -f /tmp/geas_fixture_diff.$$
done

[ "$bless" = yes ] && exit 0
echo "----"
echo "pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
