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
HERE="$(cd "$(dirname "$0")" && pwd)"
SCARE="$HERE/scare"
GAME="$1"; SOL="$2"; shift 2 2>/dev/null || true
{
  [ -f "$SOL" ] && cat "$SOL"
  for c in "$@"; do echo "$c"; done
  echo quit; echo y
} | "$SCARE" "$GAME" 2>&1
