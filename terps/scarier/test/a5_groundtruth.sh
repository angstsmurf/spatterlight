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
#     FD_CACHE  directory for cached FrankenDrift transcripts
#               (default: test/.fd_cache).  The FD engine is deterministic given
#               (game, script, FD_SEED, FD_RNG, FD build), so its transcript is
#               cached under a content hash of exactly those inputs and reused on
#               the next run -- this is what makes a repeat `make a5walkthroughs`
#               fast (no dotnet startup per game).  Set FD_NOCACHE=1 to bypass
#               (always re-run dotnet; still refreshes the cache entry).
#     FD_RNG    set to "xoshiro" to make FrankenDrift draw from a generator
#               byte-identical to Scarier's erkyrath xoshiro128** under the same
#               seed (see FrankenDrift.Headless/Program.cs XoshiroRandom).  This
#               aligns RAND-selected text so the diff becomes a full every-line
#               conformance check.  Inherited by the dotnet child, so:
#                   FD_RNG=xoshiro test/a5_groundtruth.sh <game> <script>
#
# RNG caveat (FD_RNG unset / default): FrankenDrift uses .NET System.Random while
# Scarier uses xoshiro128** (a5rand.cpp); even seeded identically the sequences
# differ, so RAND-selected text (dream/epigraph variants, combat rolls, the
# Roller catch-all) will NOT match -- diff the RAND-independent portions.  Set
# FD_RNG=xoshiro to remove this caveat (any residual RAND divergence then points
# at a real draw-order/count difference, i.e. a Scarier bug).

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

# FrankenDrift is deterministic given (game, script, FD_SEED, FD_RNG, FD build),
# so cache its transcript under a content hash of exactly those inputs and reuse
# it next run -- a dotnet launch per game is the slow part of the suite.  The
# build is captured by hashing the engine assemblies (fd-headless.dll plus the
# FrankenDrift.Adrift/Glue dlls beside it), so a rebuilt engine invalidates the
# cache automatically.
FD_CACHE=${FD_CACHE:-$HERE/test/.fd_cache}
FD_BINDIR=$(dirname "$FD_DLL")
fd_key=$(
    {
        shasum "$GAME" "$SCRIPT" "$FD_DLL" \
               "$FD_BINDIR/FrankenDrift.Adrift.dll" \
               "$FD_BINDIR/FrankenDrift.Glue.dll" 2>/dev/null
        echo "seed=$FD_SEED rng=${FD_RNG:-default}"
    } | shasum | cut -d' ' -f1
)
fd_cached="$FD_CACHE/$fd_key.txt"

if [ -z "${FD_NOCACHE:-}" ] && [ -s "$fd_cached" ]; then
    cp "$fd_cached" "$TMP/frankendrift.txt"
else
    FD_SEED="$FD_SEED" dotnet "$FD_DLL" "$GAME" "$SCRIPT" 2>/dev/null > "$TMP/frankendrift.txt" || true
    # Only cache a non-empty transcript (an empty file means dotnet failed; don't
    # poison the cache with it).
    if [ -s "$TMP/frankendrift.txt" ]; then
        mkdir -p "$FD_CACHE"
        cp "$TMP/frankendrift.txt" "$fd_cached"
    fi
fi

# Normalise trailing whitespace and collapse blank runs so the diff highlights
# real content divergence rather than spacing.
norm() { sed -e 's/[[:space:]]*$//' "$1" | cat -s; }
norm "$TMP/scarier.txt"      > "$TMP/s.txt"
norm "$TMP/frankendrift.txt" > "$TMP/f.txt"

echo "=== diff: < Scarier   > FrankenDrift (ground truth) ==="
diff "$TMP/s.txt" "$TMP/f.txt" || true
