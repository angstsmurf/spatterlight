#!/bin/sh
# probe_lenient.sh -- where the lenient deviations change an answer at points
# along the walkthrough routes.
#
# For every run_v4_walkthroughs.sh row (or those matching FILTER): take the
# lines the census's cross half says a task might take from the library
# (census_lenient.sh, SCR_CENSUS_CROSS_LINES=1), then play the solution with
# SCR_PROBE_LINES (os_ansi.cpp), which tries each of those lines before every
# solution line, once as Scarier plays it and once with no leniency at all,
# and reports the lines whose answers differ.  Unlike the census this plays
# the lines for real, in the state the route has reached, so library-first,
# restrictions and what is in scope all have their say.
#
# Each line that differs is printed once per row, with the answers at the
# first point it differs, each cut to its first four lines:
#
#   == <solution>  [<line>]  <n> points, first at solution line <k>
#   L  <lenient answer>
#   S  <strict (Runner-way) answer>
#   D  <DEV trace of the lenient run>
#
# Usage: probe_lenient.sh [-e N] [-k DIR] [FILTER]
#   -e N    probe only every Nth solution line (default 1)
#   -k DIR  keep the per-row census lines and stderr in DIR
#
# Byte-transparent, as trace_deviations.sh.

HERE=$(cd "$(dirname "$0")" && pwd)
GAMES=$HERE/../games
GOLDENS=$HERE/../goldens
EVERY=1
KEEP=
while :; do
  case "$1" in
    -e) EVERY=$2; shift 2 ;;
    -k) KEEP=$2; shift 2 ;;
    *) break ;;
  esac
done
export LC_ALL=C

if [ -n "$KEEP" ]; then
  DIR=$KEEP
  mkdir -p "$DIR"
else
  DIR=$(mktemp -d "${TMPDIR:-/tmp}/probe_lenient.XXXXXX")
  trap 'rm -rf "$DIR"' EXIT
fi

# The rows are the map_rows heredoc: solution|taf|marker|env.
sed -n '/^map_rows() { cat <<.EOF.$/,/^EOF$/p' "$HERE/run_v4_walkthroughs.sh" \
  | sed '1d;$d' | grep -a '|' | grep -a -v '^#' | grep -a -i -- "${1:-.}" \
  | tr '\n' '\0' \
  | xargs -0 -P "$(sysctl -n hw.ncpu 2>/dev/null || echo 4)" -I{} sh -c '
      row=$1 HERE=$2 GAMES=$3 GOLDENS=$4 DIR=$5 EVERY=$6
      sol=${row%%|*}; rest=${row#*|}; taf=${rest%%|*}
      envs=$(printf "%s\n" "$row" | cut -d"|" -f4)
      [ -f "$GAMES/$taf" ] && [ -f "$GOLDENS/$sol" ] || exit 0
      lines=$DIR/$sol.lines
      printf "quit\ny\n" \
        | SCR_SKIP_WAITKEY=1 SCR_CENSUS_LENIENT=1 SCR_CENSUS_CROSS_LINES=1 \
          "$HERE/scare" "$GAMES/$taf" 2>&1 >/dev/null \
        | grep -a "^CENSUS crossline " | sed "s/^[^\"]*\"//; s/\"\$//" \
        | sort -u > "$lines"
      [ -s "$lines" ] || exit 0
      { cat "$GOLDENS/$sol"; echo quit; echo y; } \
        | env SCR_ECHO_INPUT=1 SCR_RNG=xoshiro $envs SCR_TRACE_DEVIATIONS=1 \
              SCR_PROBE_LINES="$lines" SCR_PROBE_EVERY="$EVERY" TMPDIR="$DIR" \
              "$HERE/scare" "$GAMES/$taf" 2>"$DIR/$sol.err" >/dev/null
    ' _ {} "$HERE" "$GAMES" "$GOLDENS" "$DIR" "$EVERY"

# One report per row and line, with the answers at the first point it differs.
for err in "$DIR"/*.err; do
  [ -f "$err" ] || continue
  sol=$(basename "$err" .err)
  grep -a '^PROBE	\|^[LSD]	' "$err" | awk -F'\t' -v sol="$sol" '
    function cut(tag, text, count) {
      return text (count > 4 ? tag "  ...(" count - 4 " more lines)\n" : "")
    }
    function flush() {
      if (line == "") return
      if (!(line in n)) {
        order[++keys] = line; first[line] = at
        text[line] = cut("L", part["L"], cnt["L"]) cut("S", part["S"], cnt["S"]) part["D"]
      }
      n[line]++
    }
    $1 == "PROBE" {
      flush(); at = $2; line = $3
      part["L"] = part["S"] = part["D"] = ""; cnt["L"] = cnt["S"] = 0
      next
    }
    ($1 == "D" || ++cnt[$1] <= 4) { part[$1] = part[$1] $1 "  " substr($0, 3) "\n" }
    END {
      flush()
      for (i = 1; i <= keys; i++) {
        k = order[i]
        printf "== %s  [%s]  %d points, first at solution line %d\n%s\n", \
               sol, k, n[k], first[k], text[k]
      }
    }'
done
