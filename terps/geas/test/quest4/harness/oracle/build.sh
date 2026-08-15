#!/bin/bash
# Build the qv4 Quest 4 ground-truth oracle.
#
# QuestViva's src/Legacy IS the Quest 4 engine -- V4Game.cs is a line-by-line C#
# translation of Axe's VB6 Quest 4 -- so it makes a real oracle for geas, in the
# way FrankenDrift does for scarier.  The clone lives outside the repo and is
# shared with the Quest 5 oracle (../../../quest5/harness/oracle), whose
# patch_questviva.py is what routes both engines' RNG through the deterministic
# xoshiro128** stream geas draws from.  Requires the .NET 10 SDK.
#
#   ./build.sh
#   ORACLE_HOME=/somewhere ./build.sh
set -euo pipefail
export PATH="/opt/homebrew/bin:$PATH"
HERE="$(cd "$(dirname "$0")" && pwd)"
QV5="$HERE/../../../quest5/harness/oracle"
ORACLE_HOME="${ORACLE_HOME:-$HOME/questviva-oracle}"
QV="$ORACLE_HOME/questviva"

mkdir -p "$ORACLE_HOME"
if [ ! -d "$QV/.git" ]; then
  echo "[build] cloning QuestViva (branch main) into $QV"
  git clone --depth 1 --filter=blob:none https://github.com/textadventures/quest "$QV"
fi

python3 "$QV5/patch_questviva.py" "$QV" "$QV5"

echo "[build] building qv4 against $QV"
dotnet build -c Release "$HERE/qv4.csproj" -p:QuestVivaDir="$QV"
echo "[build] done: $HERE/bin/Release/net10.0/qv4"
