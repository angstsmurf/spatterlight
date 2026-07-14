#!/usr/bin/env python3
"""Patch a pristine QuestViva checkout to use the deterministic ErkyrathRandom
in place of .NET's Random, so the oracle's RNG matches the future native Geas
engine. Idempotent — safe to re-run. Called by build.sh after cloning.

Usage: patch_questviva.py <questviva-dir> <oracle-dir>
"""
import shutil, sys
from pathlib import Path

qv, oracle = Path(sys.argv[1]), Path(sys.argv[2])
engine = qv / "src" / "Engine"

# 1. Drop ErkyrathRandom.cs into the Engine assembly (idempotent copy).
shutil.copy2(oracle / "ErkyrathRandom.cs", engine / "ErkyrathRandom.cs")

# 2. Rewrite the three RNG lines in ExpressionOwner.cs.
owner = engine / "Functions" / "ExpressionOwner.cs"
text = owner.read_text()

edits = [
    ("    private readonly Random _random = new();",
     "    private readonly ErkyrathRandom _random = ErkyrathRandom.FromEnv();"),
    ("        return _random.Next(min, max + 1);",
     "        return _random.NextInclusive(min, max);"),
    ("        return _random.NextDouble();",
     "        return _random.NextDouble();"),  # signature-compatible, kept as-is
]

already = "ErkyrathRandom.FromEnv()" in text
for old, new in edits:
    if old == new:
        continue
    if old in text:
        text = text.replace(old, new, 1)
    elif not already:
        sys.exit(f"[patch] anchor not found (upstream changed?):\n  {old}")

owner.write_text(text)
print(f"[patch] {'already patched' if already else 'patched'}: "
      f"ExpressionOwner.cs -> ErkyrathRandom (seed 1234 / QVH_SEED)")
