#!/bin/sh
# census_lenient.sh -- what the lenient deviations wake that no walkthrough
# route exercises.
#
# Runs every ADRIFT 4 game in test/adrift4/games (or those matching FILTER)
# with SCR_CENSUS_LENIENT=1 (run_lenient_census() in scrun_dispatch.cpp) and
# prints its lines prefixed with the game:
#
#   <game>  woken task=...   a task no line reaches the Runner's way
#   <game>  cross task=...   a task that takes library-verb lines the Runner
#                            gives to the library
#   <game>  summary ...
#
# Cross hits are candidates (see run_lenient_census()); SCR_CENSUS_NOCROSS=1
# skips them.  The full run takes ~20 minutes on 8 cores.
#
# Usage: census_lenient.sh [FILTER]
# Byte-transparent, as trace_deviations.sh.

HERE=$(cd "$(dirname "$0")" && pwd)
GAMES=$HERE/../games
export LC_ALL=C SCR_SKIP_WAITKEY=1 SCR_CENSUS_LENIENT=1

ls "$GAMES" | grep -i '\.taf$' | grep -i -- "${1:-.}" \
  | tr '\n' '\0' \
  | xargs -0 -P "$(sysctl -n hw.ncpu 2>/dev/null || echo 4)" -I{} sh -c '
      printf "quit\ny\n" | "$1/scare" "$2/$3" 2>&1 >/dev/null \
        | grep -a "^CENSUS " | sed "s|^CENSUS |${3%.*}	|"' _ "$HERE" "$GAMES" {} \
  | sort
