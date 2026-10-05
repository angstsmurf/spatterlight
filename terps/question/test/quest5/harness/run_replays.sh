#!/bin/bash
# Corpus regression for the NATIVE aslx engine. For every frozen command script
# in ../goldens/<Game>.cmd, replay it through aslx_replay and diff the transcript
# against ../goldens/<Game>.txt.
#
# This is the mirror image of oracle/check_golden.sh, over exactly the same two
# files. There, a FAIL means the *oracle* drifted (a QuestViva upstream change, a
# .NET/RNG regression, a Program.cs edit) and the goldens may need re-freezing;
# here, a FAIL means *we* diverged from what real Quest prints, and the golden is
# the answer key. Run the oracle side first when both fail: if QuestViva itself
# moved, everything below is measuring against a stale baseline.
#
#   ./run_replays.sh                  # the whole corpus, in parallel
#   ./run_replays.sh acreage hobbit   # just the matching games
#
# Needs the corpus games, which are third-party and so untracked: prefers
# ../games if it has been set up (the way quest4/games works) and falls back to
# ~/Downloads/Quest 5 games, with GAMES= overriding both. Games with a golden
# but no game file are reported MISS, not FAIL -- but a run that checked nothing
# at all still exits non-zero, since a corpus-free machine must not read green.
#
# Env: GAMES=<dir>, JOBS=<n>, DIFF_LINES=<n> (0 to print no diff excerpt),
#      WORK=<dir> (keep the replayed transcripts; default is a temp dir).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/corpus_lib.sh"  # GAMES, GOLDEN, match_filter, corpus_check
REPLAY="$HERE/aslx_replay"
# The engine's Core library, which every game loads before its own .aslx.
export ASLX_CORE="$HERE/../../../quest5/aslx-core"

# The goldens are QuestViva's output under its default seed, which aslx_replay
# matches with its own default of 1234 -- an inherited ASLX_SEED would simply
# fail every game that draws a random number. ASLX_GRID_TRACE is worse than
# noisy: installing the grid hook switches the JS.* bridge onto its grid
# vocabulary, so it changes what is replayed. Neither belongs in a regression.
unset ASLX_SEED ASLX_GRID_TRACE ASLX_RESTORE

DIFF_LINES="${DIFF_LINES:-20}"

# aslx_replay links the engine objects, so a binary newer than aslx_replay.cc
# can still be older than the edit being tested; ask make every time (a no-op
# when nothing has changed). It is not part of `make check` --
# `check` is deliberately corpus-free -- so it may not have been built at all.
make -C "$HERE/../.." quest5/harness/aslx_replay >/dev/null || exit 2
[ -x "$REPLAY" ] || { echo "no $REPLAY" >&2; exit 2; }

keepwork=yes
if [ -n "${WORK:-}" ]; then
  mkdir -p "$WORK"
else
  keepwork=no
  WORK="$(mktemp -d "${TMPDIR:-/tmp}/aslx-replays.XXXXXX")"
  trap 'rm -rf "$WORK"' EXIT INT TERM
fi

# The whole corpus is a couple of minutes serially, so fan out across cores
# like the oracle's check does.
run_game() { "$REPLAY" "$1" "$2"; }
export REPLAY
SUFFIX=replay LABEL="native replay"
JOBS="${JOBS:-$(default_jobs)}"
FILTERS=("$@")
corpus_check
if [ "$fail" -gt 0 ]; then
  if [ "$keepwork" = yes ]; then echo "full transcripts in $WORK"
  else echo "re-run with WORK=<dir> to keep the full transcripts"; fi
fi

# Nothing replayed means no corpus, not a clean bill of health.
if [ "$((pass + fail))" -eq 0 ]; then
  echo "no games were replayed -- is the corpus present? (GAMES=$GAMES)" >&2
  exit 2
fi
[ "$fail" -eq 0 ]
