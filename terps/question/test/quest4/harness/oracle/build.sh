#!/bin/bash
# Build the qv4 Quest 4 ground-truth oracle.
#
# QuestViva's src/Legacy IS the Quest 4 engine -- V4Game.cs is a line-by-line C#
# translation of Axe's VB6 Quest 4 -- so it makes a real oracle for Question, in the
# way FrankenDrift does for scarier.  The clone lives outside the repo and is
# shared with the Quest 5 oracle (../../../quest5/harness/oracle), whose
# questviva_clone.sh gets it to the pinned revision and whose patch_questviva.py
# is what routes both engines' RNG through the deterministic xoshiro128** stream
# Question draws from.  Requires the .NET 10 SDK.
#
#   ./build.sh
#   ORACLE_HOME=/somewhere ./build.sh
set -euo pipefail
export PATH="/opt/homebrew/bin:$PATH"
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../../../quest5/harness/oracle/questviva_clone.sh"

echo "[build] building qv4 against $QV"
dotnet build -c Release "$HERE/qv4.csproj" -p:QuestVivaDir="$QV"
echo "[build] done: $HERE/bin/Release/net10.0/qv4"
