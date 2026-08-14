#!/bin/sh
# Bounded version of play.sh, for probing a game whose route is not yet known.
#
#   sh safeplay.sh <game.taf> <solution.txt> [extra cmd] [extra cmd] ...
#
# Same contract as play.sh, but capped: 12s of CPU and 4MB of output. A wrong
# turn in an event-heavy game (circus is the standard example) can spin the
# interpreter until it has eaten all the machine's RAM, which is far more
# annoying to recover from than a truncated transcript. Use play.sh once the
# route is settled; the regression runner applies its own `ulimit -t 30`.
#
# Echoes commands after the '>' prompt like play.sh does; SCR_ECHO_INPUT=
# (empty) suppresses it.
HERE="$(cd "$(dirname "$0")" && pwd)"
export SCR_ECHO_INPUT="${SCR_ECHO_INPUT-1}"
SCARE="$HERE/scare"
GAME="$1"; SOL="$2"; shift 2 2>/dev/null || true
( ulimit -t 12
  {
    [ -f "$SOL" ] && cat "$SOL"
    for c in "$@"; do echo "$c"; done
    echo quit; echo y
  } | "$SCARE" "$GAME" 2>&1 ) | head -c 4000000
