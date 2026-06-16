#!/bin/sh
# Drive the headless Bocfel binary with a command transcript and (optionally)
# diff its output against an expected transcript.
#
# Usage:
#   ./run_transcript.sh STORY CMDS [EXPECTED]
#
#   STORY     a Z-machine story file (.z3/.z5/.z8/.zblorb ...)
#   CMDS      a file with one game command per line, fed on stdin
#   EXPECTED  optional reference output; if given, the actual output is diffed
#             against it and the script exits non-zero on any difference
#
# With no EXPECTED, the captured output is printed to stdout (handy for
# generating a reference file:  ./run_transcript.sh game.z5 cmds.txt > expected.txt).
#
# Examples:
#   ./run_transcript.sh game.z5 walkthrough.txt
#   ./run_transcript.sh game.z5 walkthrough.txt expected.txt && echo OK

set -eu

DIR=$(cd "$(dirname "$0")" && pwd)
BIN="$DIR/headless_obj/bocfel_hl"

if [ ! -x "$BIN" ]; then
    echo "error: $BIN not built. Run: make -f Makefile.headless" >&2
    exit 2
fi

if [ "$#" -lt 2 ]; then
    echo "usage: $0 STORY CMDS [EXPECTED]" >&2
    exit 2
fi

STORY=$1
CMDS=$2
EXPECTED=${3:-}

ACTUAL=$("$BIN" "$STORY" < "$CMDS" 2>&1)

if [ -z "$EXPECTED" ]; then
    printf '%s\n' "$ACTUAL"
    exit 0
fi

if printf '%s\n' "$ACTUAL" | diff -u "$EXPECTED" - ; then
    echo "PASS: $STORY"
else
    echo "FAIL: $STORY (output differs from $EXPECTED)" >&2
    exit 1
fi
