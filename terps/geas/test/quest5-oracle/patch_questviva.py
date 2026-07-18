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
# Some corpus games are legacy-tolerable FINITE error storms (The Acreage's
# Port-entry recursion, Whitefield's Inescapable Cage burst) that the breaker
# turns into fake endings. QVH_ERROR_LIMIT (or a #!errorlimit=N script
# directive) overrides the threshold per run; the default stays 20 so frozen
# behaviours are unchanged. (spondre's error storm, once blamed here, was
# really the broken dictionary `in` operator — see section 5.)
wm = engine / "WorldModel.cs"
wtext = wm.read_text()
w_anchor = "    private const int MaxScriptErrors = 20;"
w_repl = """    private static readonly int MaxScriptErrors =
        int.TryParse(Environment.GetEnvironmentVariable("QVH_ERROR_LIMIT"), out var qvhLimit)
            ? qvhLimit : 20; // qvh patch: legacy Quest had no breaker at all"""
if "QVH_ERROR_LIMIT" in wtext:
    print("[patch] already patched: WorldModel.cs -> QVH_ERROR_LIMIT breaker")
elif w_anchor in wtext:
    wtext = wtext.replace(w_anchor, w_repl, 1)
    wm.write_text(wtext)
    print("[patch] patched: WorldModel.cs -> QVH_ERROR_LIMIT breaker")
else:
    sys.exit("[patch] anchor not found in WorldModel.cs (upstream changed?)")

# NOT patched — early game.pov assignment (tried 2026-07-18, REJECTED). Core's
# StartGame only assigns game.pov = player AFTER InitInterface has run, so
# spondre's InitInterface-era UpdatePlayerUI hits a null pov ("Property
# 'longtermtopics' not found on ''" as the first error of every run). Legacy
# Quest errored identically there (FLEE resolver NullReferenceException), so
# the single init error is FAITHFUL; pre-assigning pov before InitInterface
# fixed it but changed grid-map init in Guttersnipe-Carnival + I Contain
# Multitudes (new CoreGrid coordinates errors -> golden drift). spondre's real
# blocker was section 5 below, which fixes it on its own.

# 5. `key in dictionary` must test KEY membership. Legacy FLEE's `in` operator
# checked dictionary keys, and game code relies on it — spondre's ResponseLib
# guards every topic insert with `if (topic in topics) { dictionary remove ... }`
# before `dictionary add`. NCalc's default In enumerates a dictionary as
# KeyValuePairs, so a string key never matches -> the remove is skipped and the
# add throws "Error adding key ... already been added" on EVERY topic pass
# (spondre's infinite THEOTHER storm). Intercept In/NotIn when the right operand
# is an IDictionary and use key lookup; lists keep NCalc's native behaviour.
ev = engine / "Expressions" / "NcalcExpressionEvaluator.cs"
etext = ev.read_text()
e_anchor = "        if (operatorName == null && !isEquality) return;"
e_repl = """        // qvh patch: FLEE's `in` checked dictionary KEYS; NCalc's In enumerates
        // KeyValuePairs so `key in dict` was always false (see patch_questviva.py section 5)
        if (args.BinaryExpression.Type is BinaryExpressionType.In or BinaryExpressionType.NotIn)
        {
            var inRight = await args.RightValueAsync();
            if (inRight is System.Collections.IDictionary inDict)
            {
                var inLeft = await args.LeftValueAsync();
                var contains = inLeft != null && inDict.Contains(inLeft);
                args.Result = args.BinaryExpression.Type == BinaryExpressionType.In ? contains : !contains;
            }
            return;
        }

        if (operatorName == null && !isEquality) return;"""
if "inDict" in etext:
    print("[patch] already patched: NcalcExpressionEvaluator.cs -> dictionary `in` operator")
elif e_anchor in etext:
    ev.write_text(etext.replace(e_anchor, e_repl, 1))
    print("[patch] patched: NcalcExpressionEvaluator.cs -> dictionary `in` operator")
else:
    sys.exit("[patch] `in` anchor not found in NcalcExpressionEvaluator.cs (upstream changed?)")
