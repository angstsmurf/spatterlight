#!/bin/sh
# trace_deviations.sh -- which deliberate deviations decide something on the
# ADRIFT 4 walkthrough routes.
#
# Runs every run_v4_walkthroughs.sh row (or those matching FILTER) with
# SCR_TRACE_DEVIATIONS=1, keeps each row's stderr, and prints a count per
# deviation id followed by every event, one per line:
#
#   <game>  <id>  [<input line>]  <detail>
#
# The ids are the SCR_DEVIATION() calls in the engine (grep for them); each
# fires only where the deviation changed what the Runner would have done, or,
# for lenient_task, where a task ran on a line no task matches strictly.  The
# [input line] is the last typed line, so for events raised on a library
# rebuild or before the line is split it can be the previous one; the detail
# names the line the event is about.
#
# Usage: trace_deviations.sh [-k DIR] [FILTER]
#   -k DIR  keep the per-row stderr files in DIR (default: a temp dir, removed)
#
# Byte-transparent: game text is shown as the engine prints it (cp1251 games
# stay cp1251; pipe through `iconv -f cp1251 -t utf-8` to read them).

HERE=$(cd "$(dirname "$0")" && pwd)
KEEP=
if [ "$1" = "-k" ]; then KEEP=$2; shift 2; fi

if [ -n "$KEEP" ]; then
  DIR=$KEEP
  mkdir -p "$DIR"
  rm -f "$DIR"/*.err
else
  DIR=$(mktemp -d "${TMPDIR:-/tmp}/trace_deviations.XXXXXX")
  trap 'rm -rf "$DIR"' EXIT
fi

V4WT_STDERR_DIR=$DIR SCR_TRACE_DEVIATIONS=1 \
  "$HERE/run_v4_walkthroughs.sh" "$@" >/dev/null 2>&1

export LC_ALL=C
events=$(cd "$DIR" && grep -a '^DEV ' ./*.err 2>/dev/null \
         | sed -e 's|^\./||' -e 's/\.[Tt][Aa][Ff]\.[^:]*\.err:DEV /	/')

echo "== deviations: events, rows"
printf '%s\n' "$events" | awk -F'\t' 'NF > 1 {
    n[$2]++
    if (!(($2, $1) in seen)) { seen[$2, $1] = 1; rows[$2]++ }
  }
  END { for (id in n) printf "%-24s %5d %4d\n", id, n[id], rows[id] }' \
  | sort -k2,2nr

echo
echo "== events"
printf '%s\n' "$events" | awk -F'\t' 'NF > 1' | sort -t'	' -k2,2 -k1,1
