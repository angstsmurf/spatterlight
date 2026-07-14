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

mkdir -p "$ORACLE_HOME"
if [ ! -d "$QV/.git" ]; then
  echo "[build] cloning QuestViva (branch main) into $QV"
  git clone --depth 1 --filter=blob:none https://github.com/textadventures/quest "$QV"
fi

# Route QuestViva's RNG through the deterministic ErkyrathRandom (matches the
# future native Geas engine's erkyrath_random stream). Idempotent.
python3 "$HERE/patch_questviva.py" "$QV" "$HERE"

echo "[build] building qvh against $QV"
dotnet build -c Release "$HERE/qvh.csproj" -p:QuestVivaDir="$QV"
echo "[build] done: $HERE/bin/Release/net10.0/qvh.dll"
