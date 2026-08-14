#!/bin/sh
# Drive a game with a solution file (one command per line) and print output.
#
#   sh play.sh <game.taf> <solution.txt> [extra cmd] [extra cmd] ...
#
# Appends `quit`/`y` automatically. Extra args are appended as commands after
# the solution file (handy for probing the next step without editing the file).
#
# Determinism: the harness `scare` binary is seeded (seed.c), so identical input
# always yields identical output/score. Re-run freely.
#
# Echoes each command after its '>' prompt (SCR_ECHO_INPUT), so a probe run has
# the same shape as the goldens run_v4_walkthroughs.sh records and can be
# diffed straight against one. Set SCR_ECHO_INPUT= (empty) to suppress it.
HERE="$(cd "$(dirname "$0")" && pwd)"
export SCR_ECHO_INPUT="${SCR_ECHO_INPUT-1}"
SCARE="$HERE/scare"
GAME="$1"; SOL="$2"; shift 2 2>/dev/null || true
{
  [ -f "$SOL" ] && cat "$SOL"
  for c in "$@"; do echo "$c"; done
  echo quit; echo y
} | "$SCARE" "$GAME" 2>&1
