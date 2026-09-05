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
#
# Upstream fixed this natively (NcalcExpressionEvaluator routes an IDictionary
# right operand through IDictionary.Contains(key)), so the patch is retired and
# this section only checks the fix is still there.
etext = (engine / "Expressions" / "NcalcExpressionEvaluator.cs").read_text()
if "right is IDictionary" in etext:
    print("[patch] upstream-native: NcalcExpressionEvaluator.cs -> dictionary `in` operator")
else:
    sys.exit("[patch] dictionary `in` fix missing from NcalcExpressionEvaluator.cs (upstream changed?)")

# 6. `wait` must suspend the TURN, not just the script. Legacy Quest 5 runs the
# game on its own thread and blocks it inside `wait` (DoInNewThreadAndWait), so
# the stack is preserved and Core's FinishTurn — hence every turnscript — runs
# AFTER the wait callback. QuestViva's async port fire-and-forgets the
# continuation (WaitScript.ExecuteAsync returns Task.CompletedTask), so
# RunProcedureAsync("HandleCommand") unwinds early and TryFinishTurnAsync races
# ahead of the callback. Any game whose `wait` callback moves the player then
# leaves a turnscript armed in the room it left sees that turnscript fire in a
# room the player has already quit.
#
# "The Legend of Robin Hood" is the case that proves it: the Sheriff's prize
# script strips your cloak and then does `wait { ... MoveObject(player,
# cloisters) }`, while the archery field's ArcheryContestStarts turnscript
# (armed at contest entry, never disabled) calls `finish` on any turn you stand
# there unhooded. Under the racing order it fires before the move and ends the
# game with the failure ending — the win is unreachable. Under the legacy order
# the player is already in the cloisters and the turnscript is out of scope.
#
# Upstream fixed this natively (Quest Viva #2177 + #2182): HandleCommand and
# the timer path set `_finishTurnDeferred` whenever `_pendingCallbackCount > 0`
# and EndPendingCallbackAsync runs the deferred FinishTurn once the count is
# back to zero. That gate is BROADER than the wait-only one this section used
# to impose (it also covers `get input`, `ask` and `show menu`), and legacy
# Quest 5.8.0 agrees with upstream: its TryFinishTurn skips FinishTurn while
# `m_callbacks.AnyOutstanding()` (any of the four), and every callback
# resolution goes through RunCallbackAndFinishTurn (WorldModel.cs 5.8.0:
# SendCommand, SetMenuResponse, SetQuestionResponse, FinishWait). So the patch
# is retired and this section only checks the upstream mechanism is present.
# (The native Geas engine still gates on the wait alone -- see the harness
# README for the goldens that diverge because of it.)
wtext = wm.read_text()
if "_finishTurnDeferred" in wtext and "_pendingCallbackCount > 0" in wtext:
    print("[patch] upstream-native: WorldModel.cs -> FinishTurn deferred across every suspension")
else:
    sys.exit("[patch] deferred-FinishTurn mechanism missing from WorldModel.cs (upstream changed?)")

# 7. changed<field> must fire only on a REAL value change. Quest 5 (branch v5)
# fired changed<field> from a subscriber to Fields.AttributeChanged, and
# Fields.Set raises that event ONLY when the value actually changed
# (changed = value == null ? oldValue != null : !value.Equals(oldValue)). The
# rewrite moved the dispatch into Element.SetFieldAsync and fires it
# unconditionally, so a same-value write recurses until the breaker wedges the
# session. L Too's Pool (enter -> enter_pool -> MoveObject(player, Pool)) and
# "Eight characters ..."'s three game boards (mutual SwitchOn in onswitchon)
# both wedge QuestViva but terminate on real Quest. Restore the guard at the
# new dispatch site. (The native Geas engine carries the equivalent guard;
# with this patch the oracle's Dracula/Iron John/The Acreage/Bony King/Xanadu
# transcripts become byte-identical to native.)
#
# Upstream restored the guard natively (Element.SetFieldAsync computes
# `changed` from oldValue before Fields.Set), so this only checks it is there.
eltext = (engine / "Element.cs").read_text()
if "!value.Equals(oldValue)" in eltext:
    print("[patch] upstream-native: Element.cs -> same-value changed<field> guard")
else:
    sys.exit("[patch] same-value changed<field> guard missing from Element.cs (upstream changed?)")

# 8. Quest 4 RNG. The V4 engine has its own generator (`V4Game._random`) and its
# own mapping, `Str(Int(Rnd * (b-a+1)) + a)` (V4Game.cs:6982-6988). Route it
# through ErkyrathRandomV4 -- xoshiro128** on the same seed as the Quest 5 half,
# with geas's modulo mapping -- so that a Quest 4 corpus game replayed against
# this oracle draws exactly what the native engine drew and any transcript
# divergence is an ENGINE divergence. The `Conversion.Str` wrapper is left in
# place on purpose: VB's Str() prefixes a space to non-negative numbers, and
# whether geas reproduces that is one of the things being checked.
legacy = qv / "src" / "Legacy"
shutil.copy2(oracle / "ErkyrathRandomV4.cs", legacy / "ErkyrathRandomV4.cs")

v4 = legacy / "V4Game.cs"
v4text = v4.read_text()
v4_edits = [
    ("    private readonly Random _random = new();",
     "    private readonly ErkyrathRandomV4 _random = ErkyrathRandomV4.FromEnv();"),
    ("""            return Conversion.Str(
                Conversion.Int(_random.NextDouble() *
                               (Conversions.ToDouble(parameters[2]) - Conversions.ToDouble(parameters[1]) + 1d)) +
                Conversions.ToDouble(parameters[1]));""",
     """            return Conversion.Str(_random.Rand(
                Conversions.ToDouble(parameters[1]), Conversions.ToDouble(parameters[2])));"""),
]
v4_already = "ErkyrathRandomV4.FromEnv()" in v4text
for old, new in v4_edits:
    if old in v4text:
        v4text = v4text.replace(old, new, 1)
    elif not v4_already:
        sys.exit(f"[patch] V4 anchor not found (upstream changed?):\n  {old}")
v4.write_text(v4text)
print(f"[patch] {'already patched' if v4_already else 'patched'}: "
      f"V4Game.cs -> ErkyrathRandomV4 (geas mapping, seed 1234 / QVH_SEED)")

# 9. VB's Chr/Asc must use code page 1252, not the host's. Strings.Chr maps
# 0-255 through the thread's ANSI code page: 1252 on Windows, which is what VB6
# Quest assumed, but 65001 (UTF-8) on macOS and Linux, where the lone byte 0xFD
# is not valid UTF-8 and Chr(253) comes back as U+FFFD. V4Game reads .cas files
# as 1252 and then searches them for Chr(253)/Chr(254) record delimiters, so
# off-Windows the delimiters are never found and EVERY CAS game dies in
# LoadCASFile ("Argument 'Length' must be greater or equal to zero" -- the
# InStr miss returns 0, so the following Mid length goes negative). That is 35
# of the 111 Quest 4 corpus games. DecryptString and the two QSG decoders share
# the bug without crashing, quietly mangling any byte above 0x7F. QvhChars
# pins both directions to 1252.
shutil.copy2(oracle / "QvhChars.cs", legacy / "QvhChars.cs")

for name in ("V4Game.cs", "V4Game.Part2.cs"):
    path = legacy / name
    text = path.read_text()
    if "QvhChars." in text:
        print(f"[patch] already patched: {name} -> QvhChars (cp1252 Chr/Asc)")
        continue
    if "Strings.Chr(" not in text and "Strings.Asc(" not in text:
        sys.exit(f"[patch] no Strings.Chr/Asc in {name} (upstream changed?)")
    text = text.replace("Strings.Chr(", "QvhChars.Chr(")
    text = text.replace("Strings.Asc(", "QvhChars.Asc(")
    path.write_text(text)
    print(f"[patch] patched: {name} -> QvhChars (cp1252 Chr/Asc)")

# 10. Begin() must arm the turn-suspension handshake like every other entry
# point. SendCommand/Tick/FinishWait/SetMenuResponse/SetQuestionResponse all do
# `_turnSuspendedTcs = new(); _ = <run game code>; await _turnSuspendedTcs.Task`,
# so they return when the turn PARKS. Begin() instead awaits DoBeginAsync
# straight through with _turnSuspendedTcs still null, so an opening that parks
# -- `wait`, `choose`, `ask`, or an `enter` reading the player's name -- never
# completes the Begin task and the driver deadlocks. Riddle Run ("The game will
# start as soon as you answer this question: What is your name?") and The Legend
# of Cyrn are the corpus cases. A real Player never notices, because its DoWait
# runs on the UI thread and can call FinishWait from inside the same Begin
# await; a headless driver cannot.
part2 = legacy / "V4Game.Part2.cs"
p2text = part2.read_text()
begin_anchor = """    public async Task Begin()
    {
        await DoBeginAsync();"""
begin_repl = """    public async Task Begin()
    {
        // qvh: park-aware Begin -- see patch_questviva.py section 10.
        _turnSuspendedTcs = new TaskCompletionSource();
        _ = QvhBeginAsync();
        await _turnSuspendedTcs.Task;
    }

    private async Task QvhBeginAsync()
    {
        try
        {
            await DoBeginAsync();"""
if "QvhBeginAsync" in p2text:
    print("[patch] already patched: V4Game.Part2.cs -> park-aware Begin()")
elif begin_anchor in p2text:
    p2text = p2text.replace(begin_anchor, begin_repl, 1)
    # Close the try/catch: the original body's closing brace now ends
    # QvhBeginAsync, so give it a catch that still releases the driver.
    tail_anchor = begin_repl + "\n    }\n"
    if tail_anchor not in p2text:
        sys.exit("[patch] Begin() body is not the single line it was")
    p2text = p2text.replace(tail_anchor, begin_repl + """
        }
        catch (Exception ex)
        {
            LogException(ex);
            SignalTurnSuspended();
        }
    }
""", 1)
    part2.write_text(p2text)
    print("[patch] patched: V4Game.Part2.cs -> park-aware Begin()")
else:
    sys.exit("[patch] Begin() anchor not found in V4Game.Part2.cs (upstream changed?)")

# 11. Array slot 0 must be a zeroed record, not null. ObjectType and RoomType
# were VB6 `Type` blocks, so `ReDim objs(1)` gave two real, blank elements and
# reading objs(0) -- the "no such object" id that GetObjectIdNoAlias and an
# unset `parentID` both come back as -- was harmless. QuestViva declares them
# classes and leaves index 0 unassigned, so the same read throws and the turn
# dies with "[An internal error occurred]": 12 of the corpus's 16 such lines,
# across six games. It also kills two loads outright, because
# GetPropertiesInType evaluates a type's `properties` line through GetParameter
# with the null context, whose CallingObjectId is 0 -- so any type library
# `properties` line containing #thisobject# reaches `_objs[0].ObjectName`
# before the first turn. That is the whole TLTclothing family (eleven garment
# types) and ESPER: The Secret of Drom Bennacht with it.
shutil.copy2(oracle / "QvhRecord.cs", legacy / "QvhRecord.cs")

part2 = legacy / "V4Game.Part2.cs"
p2text = part2.read_text()
zero_anchor = """        _numberObjs = 1;
        _objs = new ObjectType[2];
"""
zero_repl = """        _numberObjs = 1;
        _objs = new ObjectType[2];
        // qvh patch: VB6 slot 0 is a zeroed record, not null -- section 11.
        _objs[0] = QvhZeroedRecord<ObjectType>();
        _rooms = new RoomType[1];
        _rooms[0] = QvhZeroedRecord<RoomType>();
"""
if "QvhZeroedRecord<ObjectType>" in p2text:
    print("[patch] already patched: V4Game.Part2.cs -> zeroed _objs[0]/_rooms[0]")
elif zero_anchor in p2text:
    part2.write_text(p2text.replace(zero_anchor, zero_repl, 1))
    print("[patch] patched: V4Game.Part2.cs -> zeroed _objs[0]/_rooms[0]")
else:
    sys.exit("[patch] SetUpGameObject's _objs allocation not found (upstream changed?)")

# 12. `clone` must copy the record, not alias it. Same value-type slip, and a
# worse one because it corrupts the world model instead of throwing: the fresh
# ObjectType is discarded on the very next line, both slots end up pointing at
# the one object, and the two assignments that follow rename and move the
# ORIGINAL. Barbarian's greengrocer sells one vegetable per `clone <X; X%c%;
# Market>`, so the second sale renames the only Tomato to Tomato2 and the third
# finds none, falls into _objs[0] and takes the following `show` and `give`
# down with it.
v4 = legacy / "V4Game.cs"
v4text = v4.read_text()
clone_edits = [
    ("""        _objs[_numberObjs] = new ObjectType();
        _objs[_numberObjs] = _objs[id];""",
     """        // qvh patch: a VB6 record assignment copies -- section 12.
        _objs[_numberObjs] = QvhCopyRecord(_objs[id]);"""),
    ("""            _rooms[_numberRooms] = new RoomType();
            _rooms[_numberRooms] = _rooms[_objs[id].CorresRoomId];""",
     """            // qvh patch: a VB6 record assignment copies -- section 12.
            _rooms[_numberRooms] = QvhCopyRecord(_rooms[_objs[id].CorresRoomId]);"""),
]
if "QvhCopyRecord" in v4text:
    print("[patch] already patched: V4Game.cs -> clone copies the record")
else:
    for old, new in clone_edits:
        if old not in v4text:
            sys.exit("[patch] ExecClone's aliasing assignment not found (upstream changed?)")
        v4text = v4text.replace(old, new, 1)
    v4.write_text(v4text)
    print("[patch] patched: V4Game.cs -> clone copies the record")

# 13. `take` must notice the object is in a container. ExecTake declares
# `isInContainer = false` and never assigns it again, so the sixty lines
# guarded by it -- print "(first removing it from X)", run `remove <obj>`, and
# bail out if the removal failed -- are dead code. The `parentID` computed
# immediately above them is read by nothing else, and the block's own comment
# ("So, we want to take an object that's in a container or surface. So first we
# have to remove the object from that container") is a translated VB6 comment,
# so the original plainly set the flag where the parent was looked up and the
# line was dropped in translation. Restoring it makes SEVEN more corpus games
# byte-identical between geas and the oracle -- Blight of Elantria's
# thousand-line desync among them -- which is the evidence that geas has the
# behaviour right and this is QuestViva's defect, not geas's invention.
#
# NOTE: `FINDINGS.md`'s *Ground truth* section records the opposite from a
# hand-driven Quest 4.1.5 probe under Wine, but names the wrong engine as the
# one printing the parenthetical, so the sides were confused when it was
# written. Re-measure it before trusting either.
take_anchor = """            var parent = GetObjectProperty("parent", id, false, false);
            parentID = GetObjectIdNoAlias(parent);"""
take_repl = """            var parent = GetObjectProperty("parent", id, false, false);
            // qvh patch: the dropped line that makes the block below live -- section 13.
            if (!string.IsNullOrEmpty(parent)) isInContainer = true;
            parentID = GetObjectIdNoAlias(parent);"""
p2text = part2.read_text()
if "section 13" in p2text:
    print("[patch] already patched: V4Game.Part2.cs -> take sets isInContainer")
elif take_anchor in p2text:
    part2.write_text(p2text.replace(take_anchor, take_repl, 1))
    print("[patch] patched: V4Game.Part2.cs -> take sets isInContainer")
else:
    sys.exit("[patch] ExecTake's parent lookup not found (upstream changed?)")

# 14. Expose "the turn is parked on an `enter`" to the driver. This one adds no
# behaviour: `enter <var>` suspends the turn and waits for the next SendCommand
# to fill the variable (ExecuteEnterAsync -> _commandOverrideModeOn), which is
# a *mid-turn* input, not a turn of its own -- no `> ` echo, no afterturn. A
# headless driver that ticks the timers once per turn has to be able to tell
# that input apart from a command, and the flag saying so is private. Wizard is
# the corpus case: its library desk asks for a row number and a book letter, so
# its 15-turn portal timer ran three ticks ahead of geas's and the two engines
# walked into different worlds (see the quest4 oracle's FINDINGS.md).
enter_anchor = """    public Task SendCommand(string command)
    {
        return SendCommand(command, 0, null);
    }"""
enter_repl = """    public Task SendCommand(string command)
    {
        return SendCommand(command, 0, null);
    }

    // qvh: is the turn parked on an `enter`, waiting for a mid-turn input
    // rather than for the next command? -- see patch_questviva.py section 14.
    public bool QvhAwaitingEnter => _commandOverrideModeOn;"""
p2text = part2.read_text()
if "QvhAwaitingEnter" in p2text:
    print("[patch] already patched: V4Game.Part2.cs -> QvhAwaitingEnter")
elif enter_anchor in p2text:
    part2.write_text(p2text.replace(enter_anchor, enter_repl, 1))
    print("[patch] patched: V4Game.Part2.cs -> QvhAwaitingEnter")
else:
    sys.exit("[patch] SendCommand(string) not found (upstream changed?)")
