# questviva_clone.sh -- what the two oracle builds share: ./build.sh (qvh, the
# Quest 5 oracle) and ../../../quest4/harness/oracle/build.sh (qv4, the Quest 4
# one). Sourced, not run.
#
# Both build against one QuestViva clone, which lives OUTSIDE the repo (like the
# FrankenDrift build the scarier a5 oracle uses) -- it is ~42 MB and not
# something to vendor. This gets that clone ready whichever build runs first:
# cloned if missing, moved to the pinned revision, harness patches applied.
#
#   . questviva_clone.sh           # sets ORACLE_HOME and QV (the clone)
#   ORACLE_HOME=/somewhere         # overrides where the clone lives
#   QV_REV=<sha>                   # overrides the revision
QV_HARNESS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ORACLE_HOME="${ORACLE_HOME:-$HOME/questviva-oracle}"
QV="$ORACLE_HOME/questviva"
# Pinned upstream revision: v6.0.0-rc.4 + 9 (2026-10-02). Since beta.57 this
# brings the shared per-game ExpressionOwner and SetRandomSeed hook (#2294),
# inline Ask/ShowMenu prompts for v600 games (#2288), the turn no longer ending
# when a blocking prompt resumes (#2413) and the nullable/LF refactors. Override
# with QV_REV=<sha> to test another revision; patch_questviva.py's anchors are
# checked against this one (and still accept beta.57's).
QV_REV="${QV_REV:-5ef2091b91cbedb8c7b50f0c7d765159b37428e0}"

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

# Route QuestViva's RNG -- both engines' -- through the deterministic
# ErkyrathRandom (the xoshiro128** stream Question draws from). Idempotent.
python3 "$QV_HARNESS/patch_questviva.py" "$QV" "$QV_HARNESS"
