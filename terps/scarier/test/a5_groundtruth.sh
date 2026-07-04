#!/bin/sh
# ADRIFT 5 ground-truth diff: run the same command script through BOTH the
# Scarier a5run harness and the FrankenDrift engine (the reference VB.NET
# reimplementation of the official ADRIFT 5 Runner), and diff the transcripts.
#
# This is the validation backbone for the v5 engine.  FrankenDrift is a separate
# third-party checkout (github.com/awlck/frankendrift); we drive it through a
# tiny headless console frontend, FrankenDrift.Headless, that lives in that tree
# (FrankenDrift.Headless/Program.cs -- not part of upstream frankendrift).  It is
# version-controlled in our fork (github.com/angstsmurf/frankendrift, branch
# scarier-headless) so it survives a clobbered checkout -- restore with
# `git -C "$FD_ROOT" checkout scarier-headless` (or cherry-pick that commit) if it
# ever goes missing again.  Build it once with:
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

# SCARIER_TXT: a pre-captured Scarier transcript for this exact (game, script)
# pair -- the suite runner replays Scarier ONCE per game and shares the result
# between the golden compare and both RNG-mode diffs (Scarier's own output is
# RNG-mode-independent; only FrankenDrift's generator changes between modes).
if [ -n "${SCARIER_TXT:-}" ] && [ -s "${SCARIER_TXT}" ]; then
    cp "$SCARIER_TXT" "$TMP/scarier.txt"
else
    "$A5RUN" "$GAME" "$SCRIPT" 2>/dev/null > "$TMP/scarier.txt" || true
fi

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

# Number of real commands the script feeds FD (skip blank lines and #-comments,
# matching the headless runner's own filter).  A complete FD transcript echoes
# "> cmd" for each command it processed; the count is < this only when the game
# ended early (won/lost/neutral), which the banner/prompt check below allows.
script_cmds=$(grep -cE -v '^[[:space:]]*($|#)' "$SCRIPT" 2>/dev/null || echo 0)

# A transcript is COMPLETE iff dotnet exited 0 *and* either every command was
# echoed, or the game ended (a win/lose banner or the restart prompt is present).
# A short, banner-less transcript means dotnet was killed mid-run (OOM / signal
# under load) -- the classic cache-poisoning truncation.  Such output must never
# be cached, because the next run would silently diff against a stunted FD.
fd_is_complete() {  # $1 = transcript file, $2 = dotnet exit code
    [ "$2" = 0 ] || return 1
    [ -s "$1" ]  || return 1
    local echoed
    echoed=$(grep -cE '^> ' "$1" 2>/dev/null || echo 0)
    [ "$echoed" -ge "$script_cmds" ] && return 0
    grep -qE '\*\*\* You have (won|lost) \*\*\*|Would you like to .*(restart|restore)' "$1" \
        && return 0
    return 1
}

run_fd() {  # populate $TMP/frankendrift.txt; echo "ok"/"bad" via return code
    local rc
    FD_SEED="$FD_SEED" dotnet "$FD_DLL" "$GAME" "$SCRIPT" 2>/dev/null > "$TMP/frankendrift.txt"
    rc=$?
    fd_is_complete "$TMP/frankendrift.txt" "$rc"
}

if [ -z "${FD_NOCACHE:-}" ] && [ -s "$fd_cached" ]; then
    cp "$fd_cached" "$TMP/frankendrift.txt"
else
    # The truncation is load-transient, so retry a few times before giving up.
    fd_ok=
    for attempt in 1 2 3; do
        if run_fd; then fd_ok=1; break; fi
        echo "a5_groundtruth: FrankenDrift run looked truncated (attempt $attempt/3), retrying..." >&2
    done
    if [ -n "$fd_ok" ]; then
        mkdir -p "$FD_CACHE"
        cp "$TMP/frankendrift.txt" "$fd_cached"
    else
        # Don't poison the cache; warn loudly so the caller knows the diff is
        # against an incomplete FD transcript (hunk counts are not trustworthy).
        echo "a5_groundtruth: WARNING -- FrankenDrift transcript incomplete after 3 tries" >&2
        echo "  ($GAME: echoed $(grep -cE '^> ' "$TMP/frankendrift.txt" 2>/dev/null || echo 0)/$script_cmds commands, no end banner); NOT caching." >&2
    fi
fi

# Normalise trailing whitespace and collapse blank runs so the diff highlights
# real content divergence rather than spacing.
norm() { sed -e 's/[[:space:]]*$//' "$1" | cat -s; }
norm "$TMP/scarier.txt"      > "$TMP/s.txt"
norm "$TMP/frankendrift.txt" > "$TMP/f.txt"

echo "=== diff: < Scarier   > FrankenDrift (ground truth) ==="
diff "$TMP/s.txt" "$TMP/f.txt" || true
