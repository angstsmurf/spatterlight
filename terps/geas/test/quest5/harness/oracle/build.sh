#!/bin/bash
# Build the qvh Quest 5 ground-truth oracle.
#
# Clones QuestViva ("Quest Viva", the cross-platform .NET port of the Quest 5
# engine) if needed and builds qvh against its src/Engine. The clone lives
# OUTSIDE the repo (like the FrankenDrift build the scarier a5 oracle uses) — it
# is ~42 MB and not something to vendor.
#
# QuestViva targets net10.0, so with the .NET 10 SDK the checkout stays pristine
# — no retargeting. Requires: .NET 10 SDK (`brew install dotnet`; this Mac has
# 10.0.301, arm64).
#
#   ./build.sh                       # clone/build into $ORACLE_HOME
#   ORACLE_HOME=/somewhere ./build.sh
set -euo pipefail
export PATH="/opt/homebrew/bin:$PATH"
HERE="$(cd "$(dirname "$0")" && pwd)"
ORACLE_HOME="${ORACLE_HOME:-$HOME/questviva-oracle}"
QV="$ORACLE_HOME/questviva"
# Pinned upstream revision: v6.0.0-beta.57 (2026-09-04), the first release with
# the string-concat regression fix (#2188) on top of the on-ready/FinishTurn
# rework (#2177, #2182). Override with QV_REV=<sha> to test another revision;
# patch_questviva.py's anchors are checked against this one.
QV_REV="${QV_REV:-1b129e7a916c01235c4508a5e45d0a1db06f482f}"

mkdir -p "$ORACLE_HOME"
if [ ! -d "$QV/.git" ]; then
  echo "[build] cloning QuestViva into $QV"
  git clone --depth 1 --filter=blob:none https://github.com/textadventures/quest "$QV"
fi
if [ "$(git -C "$QV" rev-parse HEAD)" != "$QV_REV" ]; then
  # Drop the previous revision's patches (they are re-applied below) and move
  # the shallow clone to the pinned commit.
  echo "[build] moving QuestViva clone to $QV_REV"
  git -C "$QV" checkout -q -- . && git -C "$QV" clean -fdq
  git -C "$QV" fetch -q --depth 1 origin "$QV_REV"
  git -C "$QV" checkout -q -f "$QV_REV"
fi

# Route QuestViva's RNG through the deterministic ErkyrathRandom (matches the
# future native Geas engine's erkyrath_random stream). Idempotent.
python3 "$HERE/patch_questviva.py" "$QV" "$HERE"

echo "[build] building qvh against $QV"
dotnet build -c Release "$HERE/qvh.csproj" -p:QuestVivaDir="$QV"
echo "[build] done: $HERE/bin/Release/net10.0/qvh.dll"
