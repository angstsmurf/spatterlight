#!/bin/sh
# ADRIFT 5 ground-truth diff: run the same command script through BOTH the
# Scarier a5run harness and the FrankenDrift engine (the reference VB.NET
# reimplementation of the official ADRIFT 5 Runner), and diff the transcripts.
#
# This is the validation backbone for the v5 engine.  FrankenDrift is a separate
# third-party checkout (github.com/awlck/frankendrift); we drive it through a
# tiny headless console frontend, FrankenDrift.Headless, that lives in that tree
# (FrankenDrift.Headless/Program.cs -- not part of upstream frankendrift, and
# not committed here).  Build it once with:
#
#     dotnet build "$FD_ROOT/FrankenDrift.Headless/FrankenDrift.Headless.csproj" -c Release
#
# Usage:
#     test/a5_groundtruth.sh <game.blorb|game.taf> <script.txt>
#
# Env:
#     FD_ROOT   frankendrift checkout (default: ~/frankendrift)
#     FD_SEED   fixed RNG seed for FrankenDrift (default 1234, for reproducibility)
#
# IMPORTANT -- RNG caveat: FrankenDrift uses .NET System.Random while Scarier
# uses the shared erkyrath xoshiro128** (a5rand.cpp).  Even seeded identically
# the two sequences differ, so RAND-selected text (dream/epigraph variants,
# combat rolls, the Roller catch-all) will NOT match.  Diff the RAND-independent
# portions; treat RAND blocks as expected divergence.

set -eu

HERE=$(cd "$(dirname "$0")/.." && pwd)
FD_ROOT=${FD_ROOT:-$HOME/frankendrift}
FD_SEED=${FD_SEED:-1234}

if [ $# -lt 2 ]; then
    echo "usage: $0 <game.blorb|game.taf> <script.txt>" >&2
    exit 2
fi
GAME=$1
SCRIPT=$2

A5RUN="$HERE/test/a5run_dump"
FD_DLL=$(find "$FD_ROOT/FrankenDrift.Headless/bin" -name fd-headless.dll 2>/dev/null | head -1 || true)

if [ ! -x "$A5RUN" ]; then
    echo "build the Scarier harness first: make -f Makefile.headless a5run" >&2
    exit 1
fi
if [ -z "$FD_DLL" ]; then
    echo "FrankenDrift.Headless not built. Run:" >&2
    echo "  dotnet build \"$FD_ROOT/FrankenDrift.Headless/FrankenDrift.Headless.csproj\" -c Release" >&2
    exit 1
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

"$A5RUN" "$GAME" "$SCRIPT" 2>/dev/null > "$TMP/scarier.txt" || true
FD_SEED="$FD_SEED" dotnet "$FD_DLL" "$GAME" "$SCRIPT" 2>/dev/null > "$TMP/frankendrift.txt" || true

# Normalise trailing whitespace and collapse blank runs so the diff highlights
# real content divergence rather than spacing.
norm() { sed -e 's/[[:space:]]*$//' "$1" | cat -s; }
norm "$TMP/scarier.txt"      > "$TMP/s.txt"
norm "$TMP/frankendrift.txt" > "$TMP/f.txt"

echo "=== diff: < Scarier   > FrankenDrift (ground truth) ==="
diff "$TMP/s.txt" "$TMP/f.txt" || true
