#!/bin/bash
# Build the qvh Quest 5 ground-truth oracle.
#
# Clones QuestViva ("Quest Viva", the cross-platform .NET port of the Quest 5
# engine) if needed, moves it to the pinned revision, patches it
# (questviva_clone.sh, shared with the Quest 4 oracle's build) and builds qvh
# against its src/Engine.
#
# QuestViva targets net10.0, so with the .NET 10 SDK the checkout needs no
# retargeting. Requires: .NET 10 SDK (`brew install dotnet`; this Mac has
# 10.0.301, arm64).
#
#   ./build.sh                       # clone/build into $ORACLE_HOME
#   ORACLE_HOME=/somewhere ./build.sh
set -euo pipefail
export PATH="/opt/homebrew/bin:$PATH"
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/questviva_clone.sh"

echo "[build] building qvh against $QV"
dotnet build -c Release "$HERE/qvh.csproj" -p:QuestVivaDir="$QV"
echo "[build] done: $HERE/bin/Release/net10.0/qvh.dll"
