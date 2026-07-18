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

# 3. Lazy dynamictemplate expressions. QuestViva parses every dynamictemplate's
# expression EAGERLY at load (Templates.cs AddDynamicTemplate -> new
# Expression<string>), so a game shipping one malformed template fails to
# initialise at all. Legacy Quest (~5.0, 2011) did not do this — e.g.
# "Nearco II (Remake)" ships `"No puedes tirar de eso"."` (stray tail) in its
# DefaultPull dynamictemplate and was released and played, so the parse must
# be deferred to first *use* to be faithful (the native Geas engine also
# compiles lazily). On parse failure we register QvhLazyExpression, which
# re-attempts the compile when the template actually fires.
lazy_cls = engine / "Functions" / "QvhLazyExpression.cs"
lazy_cls.write_text('''using QuestViva.Engine.Scripts;

namespace QuestViva.Engine.Functions;

// qvh patch: a dynamictemplate expression that failed to parse at load.
// Legacy Quest deferred that parse to first use; do the same, so the error
// (if ever reached) surfaces as a runtime script error, not a load failure.
public class QvhLazyExpression(string expression, ScriptContext scriptContext) : IFunction<string>
{
    private IFunction<string>? _inner;

    public Task<string> ExecuteAsync(Context c)
    {
        _inner ??= new Expression<string>(expression, scriptContext);
        return _inner.ExecuteAsync(c);
    }

    public string Save() => expression;

    public IFunction<string> Clone() => new QvhLazyExpression(expression, scriptContext);
}
''')

templates = engine / "Templates.cs"
ttext = templates.read_text()
anchor = """            template.Fields[FieldDefinitions.Function] =
                new Expression<string>(expression, new ScriptContext(m_worldModel));"""
lazy = """            try
            {
                template.Fields[FieldDefinitions.Function] =
                    new Expression<string>(expression, new ScriptContext(m_worldModel));
            }
            catch (Exception)
            {
                // qvh patch: legacy-Quest lazy dynamictemplate parse (see patch_questviva.py)
                template.Fields[FieldDefinitions.Function] =
                    new QuestViva.Engine.Functions.QvhLazyExpression(expression, new ScriptContext(m_worldModel));
            }"""
if "QvhLazyExpression(expression" in ttext:
    print("[patch] already patched: Templates.cs -> lazy dynamictemplate parse")
elif anchor in ttext:
    templates.write_text(ttext.replace(anchor, lazy, 1))
    print("[patch] patched: Templates.cs -> lazy dynamictemplate parse")
else:
    sys.exit("[patch] anchor not found in Templates.cs (upstream changed?)")

# 4. Configurable script-error breaker limit. QuestViva's 20-script-error
# circuit breaker ("session unrecoverably wedged") has NO counterpart in
# legacy Quest, which simply printed every script error and carried on.
# Some corpus games are legacy-tolerable error storms: spondre's ResponseLib
# re-adds duplicate suggestion keys (~18 throws per topic pass) and would be
# Wedged instantly under the breaker despite being fully playable in real
# Quest. QVH_ERROR_LIMIT overrides the threshold per run (default stays 20,
# so Whitefield's and Eight characters' frozen behaviours are unchanged).
wm = engine / "WorldModel.cs"
wtext = wm.read_text()
w_anchor = "    private const int MaxScriptErrors = 20;"
w_repl = """    private static readonly int MaxScriptErrors =
        int.TryParse(Environment.GetEnvironmentVariable("QVH_ERROR_LIMIT"), out var qvhLimit)
            ? qvhLimit : 20; // qvh patch: legacy Quest had no breaker at all"""
if "QVH_ERROR_LIMIT" in wtext:
    print("[patch] already patched: WorldModel.cs -> QVH_ERROR_LIMIT breaker")
elif w_anchor in wtext:
    wm.write_text(wtext.replace(w_anchor, w_repl, 1))
    print("[patch] patched: WorldModel.cs -> QVH_ERROR_LIMIT breaker")
else:
    sys.exit("[patch] anchor not found in WorldModel.cs (upstream changed?)")
