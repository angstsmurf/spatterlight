# TODO: Quest 5 support in Geas

## Status (2026-07-16)

- **Ground-truth oracle: built and frozen** (§7, milestone 6's diff half).
  `terps/geas/test/quest5-oracle/` drives real `.quest`/`.aslx` games headless
  through QuestViva (.NET port of the Quest 5 engine), RNG routed through
  `erkyrath_random()` (seed 1234), and captures normalised golden transcripts.
  17 walkthroughs driven — **15 Finished, 1 Running (ICM), 1 Wedged
  (Whitefield)**; the last two are genuine QuestViva-vs-Quest limitations, not
  drift. `check_golden.sh` replays all 17 `.cmd` scripts and diffs vs frozen
  `.out` (17/17), doubling as a determinism check. This is the reference the
  native engine will diff against, with no .NET dependency at replay time.
- **Native engine: milestones 1–2 landed; milestone 3 mostly done; milestone
  4's input model landed** — Core boots, real player commands
  (`look`/`take`/`examine`/`open`/`go`) run end-to-end through Core's parser
  with zero script errors, and the prompt layer (`get input`/`show menu`/
  `ask`/`wait` + disambiguation menus) works through the new `send_command`
  host API (see the M3/M4 entries below).
  - **M1 loader** (`aslx.hh`/`aslx.cc`): `.quest` (in-memory zip over zlib) or
    raw `.aslx` (libexpat SAX) → typed element tree (containment, `<inherit>`
    chains, `<implied>`/`<template>` registration, full attribute-type set,
    simplepattern→regex). Loads all **76** corpus games, 0 failures.
  - **M2 script/expression core** (`aslx-runtime.hh`/`aslx-runtime.cc` +
    `-parse.inc`/`-builtins.inc`): a hand-written lexer + recursive-descent
    expression parser over the typed `Value`, and a script interpreter for
    `msg`, `if`/`else if`/`else`, `while`, `for`, `foreach`, `=` assignment
    (var and `obj.attr`), function calls, `return`, comments. Field access
    resolves inheritance (own → inherited types, most-recent-first). ~40
    built-ins (attribute/type, list, string, number) with `GetRandomInt`/
    `GetRandomDouble` routed through a deterministic erkyrath_random() port.
    Tested by `test/aslx_runtime_test.cc` (`make check`, clean under ASan/UBSan).
  - **M3 loader half landed** (Core bundling + `<include>` resolution).
    The non-editor Core libraries are bundled under `terps/geas/aslx-core/`
    (root `Core*.aslx`/`GamebookCore.aslx` + `Languages/*.aslx`), and the
    loader resolves `<include ref="..">` inline in source order — game-adjacent
    dir, then Core dir, then Core/Languages — matching QuestViva's
    `GetLibraryStream()`. `Editor*`/`CoreEditor*` refs are skipped (not
    bundled). Recursion is deduped/guarded. Three Core-driven attribute types
    now load too: `listextend` (flagged `Value::list_extend`; merge-on-read
    landed 2026-07-16 -- see the typed-lists entry), `list` (heterogeneous `<value>` list), and delegate-typed
    attributes (an attr whose `type=` names a `<delegate>` loads its body as a
    delegate-implementation Script). **All 76 corpus games load with 0 errors**;
    a fresh game boots the full Core tree (~330 functions / ~56 types). Wired as
    `test/aslx_loader_test.cc:test_coreboot()` (fixture `coreboot.aslx`),
    `make check` green, clean under ASan/UBSan.
  - **M3 script layer widened** (2026-07-15): the big batch of Core-critical
    script commands and built-ins landed, all unit-tested and clean under
    ASan/UBSan; the 76-game 0-error load invariant is preserved.
    - `[Something]` template substitution now resolves at **load time** (like
      QuestViva's `GameLoader.GetTemplate`): `[name]` in script bodies and
      string attributes is replaced with the registered template's text
      (later-registered wins, unmatched refs left verbatim so regex classes
      like `[^\w]` survive). `Template()`/`DynamicTemplate()` built-ins cover
      the runtime calls. `aslx.cc:Loader::replace_templates`.
    - New script commands: `switch`/`case`/`default`, `firsttime`/`otherwise`
      (per-instance run flag), `do (obj,"script"[,params])`, `invoke`,
      `create`/`create`-with-type, `destroy`, `set(obj,attr,val)`,
      `list add`/`list remove`, `dictionary add`/`dictionary remove`,
      `on ready` (deferred FIFO queue + `drain_on_ready()`), `error`, `finish`
      (sets `World::finished`), `undo`/`start transaction` (accepted, no-op
      until the UndoLogger lands), and the `JS.*` bridge (`JS.addText` → output
      sink, other `JS.*` ignored). List/dictionary mutators resolve their
      target as an lvalue (local var or obj.attr, copy-on-write for inherited).
    - New built-ins: `Template`/`DynamicTemplate`, dictionary
      constructors/`DictionaryItem`/`DictionaryCount`/`DictionaryContains` and
      the typed `*DictionaryItem`, `StringListItem`/`ObjectListItem`,
      `AllObjects`/`AllCommands`, `GetDirectChildren`/`GetAllChildObjects`,
      `Contains`, `GetAttributeNames`, 2-arg `TypeOf(obj,attr)`, `CapFirst`,
      `SafeXML`. (`JSSafe` is Core ASLX, not an engine primitive — it comes
      free once these run.)
    - Tests: `test/aslx_runtime_test.cc:test_script_commands` +
      `:test_new_builtins` (fixture `runtime.aslx` gained a template, a
      dynamictemplate, and an object script).
  - **M3 parser primitives: regex landed** (2026-07-15): `IsRegexMatch`,
    `GetMatchStrength`, `Populate` (each in the 2-arg and 3-arg/cacheID forms)
    port `Utility.IsRegexMatch/GetMatchStrength/Populate` + `RegexCache.cs`. The
    core wrinkle: Quest patterns are .NET regexes with named groups
    (`^look at (?<object1>.*)$`) and `std::regex` (ECMAScript) has no named-group
    syntax, so `rewrite_dotnet_regex` (aslx-runtime.cc) strips `(?<name>`/
    `(?'name'` to plain `(` and records the group name by capture index, leaving
    non-capturing/lookaround groups untouched. Repeated names across an
    alternation (`take (?<object>.*)|get (?<object>.*)`) are coalesced to one
    logical group with .NET's last-successful-capture value (`named_groups`).
    Matching is `regex_search` (search, not full-match, mirroring .NET `Match`)
    with `icase`; the `RegexCache` keys on cacheID alone (pattern ignored on a
    hit), so the no-cacheID forms compile fresh. Wired as
    `test/aslx_runtime_test.cc:test_regex_primitives`; `make check` green, clean
    under ASan/UBSan; the 76-game 0-error load invariant is preserved. This
    proves `std::regex` ECMAScript covers what CoreParser's simplepattern regexes
    need (§2 "Command parsing comes free" caveat resolved for the common case).
  - **M3 boot pipeline: the game boots and renders through Core** (2026-07-15):
    a fresh game plus the full Core library now runs `InitInterface` +
    `StartGame` + the `on ready` queue with **zero script errors**, and the
    opening title + room description come out through Core's own `{...}` text
    processor. Regression: `test/aslx_runtime_test.cc:test_coreboot_runs`
    (`make check` green, clean under ASan/UBSan; 76-game 0-error load preserved).
    Landed to get there, all traced empirically by running Core and fixing what
    it hit:
    - **`msg` → `OutputText`** (v540+): `msg` now calls Core's `OutputText`
      (which runs `ProcessText` then `JS.addText`) when it exists, so the entire
      `{...}` text processor -- itself ASLX in `CoreOutput`/`CoreTypes` -- runs.
      Unlocked by three primitives: `Eval(expr[,scope])`, `foreach` over a
      `scriptdictionary` (the `textprocessorcommands` dispatch table), and the
      pre-existing `ScriptDictionaryItem`/`invoke`-with-params.
    - **`not` precedence bug fixed** (critical): `not` bound tighter than `=`, so
      `not obj = null` -- which is *everywhere* in Core -- parsed as
      `(not obj) = null`. Moved `not`/`!` to their own level between `and/or` and
      equality (`parse_not`), matching QuestViva's notOperator; `-`/`~` stay
      tight. This alone was silently breaking most Core conditionals.
    - **Backslash string escapes**: `obscure_strings`, `remove_comments`, and the
      expression lexer now honour `\"` `\'` `\\` (Parlot semantics). Before, a
      body like `Replace(s,"\"","")` swallowed the rest of the function.
    - **Implicit default types**: every element gets `defaultobject`/
      `defaultgame`/`defaultexit`/`defaultcommand`/`defaultturnscript` prepended
      (QuestViva `ObjectFactory` + `DefaultTypeNames`); this is what gives the
      game object its `showtitle`/`textprocessorcommands` defaults.
    - **`parent` as a field**: containment is now exposed as a `parent`
      ObjectRef field (not just the load-time pointer), and
      `GetDirectChildren`/`GetAllChildObjects`/`Contains` derive from it, so a
      runtime `MoveObject` (which sets `obj.parent`) stays correct.
    - **CoreEditor.aslx bundled**: it is an "editor" library but QuestViva loads
      it at runtime (defines `editor_player` + runtime types every editor-made
      game inherits); its 18 UI sub-includes stay unbundled/skipped.
    - New builtins: `HasScript`/`HasDouble`/`HasDelegateImplementation`,
      `AllExits`/`AllTurnScripts`, `GetTimer`, `GetUIOption` (empty headless),
      `ToInt`/`ToDouble`/`ToString`, `Eval`. `request` accepted as a no-op (its
      first arg is a bare enum identifier, so it must not evaluate its args).
  - **M3 command driving: real commands run end-to-end** (2026-07-16): a booted
    game now takes player input through Core's `HandleCommand` → `ScopeCommands`
    → `IsRegexMatch`/`GetMatchStrength` → `ResolveName`/`GetScope` → verb script
    chain, with **zero script errors**. `look`, `take`/`inventory`, `examine`,
    `open`, and `go`-through-an-exit (into a second room, with the object list
    and the `{exit:}` link rendered) all work. Regression:
    `test/aslx_runtime_test.cc:test_command_driving` (fixture `command.aslx`);
    `make check` green, clean under ASan/UBSan; 50-package corpus still loads
    0-error. Landed, each traced by running a command and fixing what Core hit:
    - **Command patterns resolve three ways** (`aslx.cc`, matching QuestViva's
      CommandLoader): a `pattern=` attribute is template-substituted then stored
      RAW as its regex (`[look]` → `^look$|^l$`; `^restart$` verbatim) with no
      simplepattern conversion; a `template=` attribute names a verbtemplate
      whose `;`-joined text goes through `ConvertVerbSimplePattern` (lazy
      `(?<object>.*?)`, plus `displayverb`); a nested `<pattern>` keeps the
      existing greedy simplepattern conversion. `template_text()` unifies the
      verb/plain template lookup (verbtemplates `"; "`-joined, `AddVerbTemplate`).
    - **`name` exposed as a field** on every element (QuestViva
      `FieldDefinitions.Name`). It was missing, so `cmd.name` -- the RegexCache
      cacheID in `HandleSingleCommand` -- was empty and every command collapsed
      onto one cache entry (the first pattern won for all input). This also fixed
      the empty `{object:}`/room description, which key off `.name`/`.alias`.
    - **Lists and dictionaries are now REFERENCE types** (`aslx.hh`): their
      backing lives behind a `shared_ptr`, so copying a Value (function-arg
      binding, `y = x`, reading a field) shares it and `list add`/`dictionary
      add` through any alias is visible everywhere -- exactly Quest's
      QuestList/QuestDictionary. This is what makes `CompareNames`,
      `ResolveNameFromList`, and every Core helper that builds a caller-supplied
      list work. Builtins that derive a *new* collection (`ListExclude`,
      `ListCombine`, `ObjectListSort`) call `detach()` first.
    - **Dictionary values are typed per-entry** (`Value`, not a flat string):
      Quest dicts hold boxed values and a single dict can mix objects, strings
      and booleans (CoreParser's `currentcommandresolvedelements` holds the
      resolved objects alongside a boolean `multiple`). `do`/`invoke`/`Eval`
      param binding now hands each resolved object to the verb script as an
      object ref, not the string of its name -- the fix that let `open` pass its
      object to `TryOpenClose`.
    - **QuestList operators** `+`/`-`/`*` (`QuestList<T>` overloads): list+list
      merge, list+elem append, elem+list prepend, list-elem remove-first,
      list*list union -- handled before scalar/string arithmetic. `GetScope`'s
      `ListCombine(...) - game.pov` was silently coercing the list to numeric 0.
    - **`GetObject` resolves exits** (they are `ElementType.Object` in QuestViva,
      just `ObjectType.Exit`), so `ProcessTextCommand_Exit` renders `{exit:}`.
    - New builtins: `IsGameRunning`, `ObjectListSort`/`ObjectListSortDescending`,
      `StringListSort`/`StringListSortDescending`.
  - **M3 real-game blockers cleared: 49/50 corpus games boot with 0 errors**
    (2026-07-16, second batch — the whole corpus through `InitInterface` +
    `StartGame` + `on ready`, up from 2-of-5 sampled the day before). Landed,
    each verified against QuestViva source and unit-tested
    (`test/aslx_runtime_test.cc:test_realgame_constructs`):
    - **Multi-word (space-encoded) identifiers** (`Utility.EncodeIdentifierSpaces`
      port): outside string literals, adjacent word-char words join into one
      identifier unless either is an expression keyword (`and or xor not if in`,
      case-sensitive). We join with `'\x01'` and the lexer folds it back to a
      space inside a single Ident token — no decode pass. Covers Dracula's
      `GetBoolean(OUTSIDE INN, "MORNING")` and Magi's `game.Next text` (member
      names with spaces work too).
    - **`in` / `not in`** at the relational precedence level (QuestNCalc
      grammar): list membership, dictionary-key lookup, substring.
    - **`x => { script }` script-literal assignment** (SetScriptScript; `=>`
      checked before `=`, like SetScriptConstructor). This was Everyman's
      `unexpected token '>'`.
    - **Trailing script-block argument**: `Proc (args) { script }` (and
      `Proc { script }`) passes the block as one extra script-literal argument
      bound to the function's last parameter (FunctionCallScript.paramFunction).
      This is how ShowMenu callbacks and the classic inlined-CoreTimers
      `SetTimerScript (timer) { ... }` pattern work.
    - **`this` binding**: `do (obj, "action" [, params])` and `rundelegate` run
      their script with `this` = the object (WorldModel.RunScriptAsync's
      thisElement) — fixes every `changedparent`-style script Core invokes via
      `do`. `invoke` deliberately does NOT bind this (matches InvokeScript).
    - **`rundelegate` + `RunDelegateFunction`**: shared implementation; the
      delegate's `paramnames` come from the `<delegate>` element named by the
      implementation Script's declared_type; returns the script's return value.
    - **`create timer` / `create turnscript`** (create_object grew an elem_type
      param; turnscripts get defaultturnscript, timers have no default type) and
      **`GetUniqueElementName`** (trailing-digits-stripped numbering).
      **`GetObject` resolves turnscripts and the game element** too (they are
      ElementType.Object in QuestViva) — dynamic-turnscript code relies on it.
    - **`True`/`FALSE`/`Null` are case-insensitive** (NCalc `Terms.Text(...,
      true)`; null via the evaluator's parameter check).
    - **NCalc `if(cond, a, b)`** (lazy branches) **and `cast(x, int)`** (the
      type arg is a bare identifier, special-cased before evaluation like
      `_evaluatingCastType`).
    - **`GetFileURL`** returns the filename headless (real URL mapping is
      presentation-milestone work).
    - **Media commands no-op with a one-time `world.warnings` entry** (new
      vector, separate from `errors` so the 0-error invariants stay strict):
      `picture`, `play sound`, `stop sound`, `insert`. `wait { ... }` parses and
      (until the §3 input model) runs its callback immediately.
    - **QuestViva error semantics**: runtime errors (unknown variable/function,
      foreach over non-list, bad index, the `error` command...) now THROW;
      `run_script` is the per-script-body boundary (RunScriptAsync's catch) that
      logs to `world.errors` AND prints "Error running script: ..." once to the
      player. After 20 errors the session is declared wedged and finished
      (MaxScriptErrors/scriptErrorsFatal); a 200-deep script recursion cap
      matches MaxScriptExecutionDepth. Error messages carry the innermost
      executing function ("... (in UpdatePlayerUI)") via a call-frame stack.
    - Corpus smoke driver: scratch `boot_smoke.cc` (loads + boots every game in
      `~/Downloads/Quest 5 games`, reports load/boot errors) — worth wiring into
      test/ as a permanent gate once a corpus path convention is settled.
    - The one remaining boot failure, **spondre**, is two things: (a) QuestViva
      itself errors on its boot (`game.pov.longtermtopics` with pov unset —
      verified against the oracle, same abort point), and (b) **lists holding
      only strings**: spondre builds lists of *dictionaries* and indexes them
      (`match["score"]`), which needs QuestList-style typed lists (see below).
  - **M3 typed lists landed** (2026-07-16): list entries are now full `Value`s
    behind the shared backing (QuestList<object>), completing the dict-values
    change. Loader materialises typed entries (`<value type="int">` etc. in a
    `type="list"` attr; String entries for stringlists, ObjectRef for
    objectlists); `list add` stores the boxed value verbatim (a list can hold
    dictionaries — spondre's `match["score"]`); indexing/`ListItem`/foreach
    hand back the typed entry; `in`/`ListContains`/`list remove`/the QuestList
    operators compare via `values_equal`, with collections comparing by
    REFERENCE identity (shared-backing pointer), matching .NET. Verified
    against QuestViva source while porting: `list remove` takes the FIRST
    occurrence only (QuestList.Remove → List<T>.Remove; ours removed all),
    `ListExclude` filters ALL occurrences and also accepts a list to exclude
    (both fixed). Container TypeOf stays "stringlist"/"objectlist" — QuestViva
    has a distinct "list" name for QuestList<object>, still TODO if a game
    branches on it. Tests: `test/aslx_runtime_test.cc:test_typed_lists` +
    typed-`<value>` loader checks (fixture `hello.aslx`); `make check` green,
    clean under ASan/UBSan. **spondre now aborts at QuestViva's own abort
    point** (foreach over `game.pov.longtermtopics` with pov unset — the
    genuine game bug the oracle hits), with the rest of the corpus still
    49/50-boot-0-error and 0 load failures.
  - **M3 listextend merge-on-read landed** (2026-07-16, with typed lists):
    `resolve_field` now collects the whole field chain -- the base value is the
    first NON-extend field, and every `listextend` field anywhere in the chain
    is an extension. Reads merge base-entries-first then extensions
    least-derived to most-derived (QuestViva Fields.GetMergedResult /
    QuestList.MergeLists accumulate parent-first), rebuilt on every read into a
    stable per-(element,attr) slot. Before this, a listextend field silently
    SHADOWED the inherited base. Unit-tested in `test_new_builtins` (fixture
    `runtime.aslx`: container base + openable extension + own extension);
    corpus boot output byte-identical (displayverbs feed UI verb menus).
  - **M4 input model landed (engine side)** (2026-07-16): the four prompt
    script commands (`get input`, `show menu`, `ask`, real pending `wait`) plus
    the host-facing entry points, ported from QuestViva's fire-and-forget
    pending-callback model rather than the nested-glk_select plan (§3 has the
    details). Highlights:
    - `Interp::send_command()` ports `HandleCommandAsyncInternal`: a pending
      `get input` consumes the line (command override, `result` param bound),
      otherwise pre-v520 echo + Core `HandleCommand` + pre-v580 `FinishTurn`.
    - Prompts are fire-and-forget (QuestViva ExecuteAsync): the callback is
      registered, the enclosing script keeps running, the host resolves later
      via `set_menu_response(key|null)` / `set_question_response(bool)` /
      `finish_wait()`. `pending_menu()` (MenuData: caption + ordered
      key→display options + allow_cancel), `pending_question()` (caption; the
      engine does NOT print it — ShowQuestion is host UI), `pending_wait()`,
      `command_override()` tell the host what the prompt is waiting for.
      A new same-kind prompt cancels the old one (BeginPrompt TrySetCanceled).
    - **`on ready` semantics corrected to WorldModel.AddOnReady**: it runs
      IMMEDIATELY (inline) unless a prompt callback is outstanding
      (`_pendingCallbackCount`), in which case it queues; resolving the prompt
      flushes the queue FIFO (EndPendingCallbackAsync). Our old queue-always +
      manual-drain model ran callbacks too late / menus'd have drained early.
    - `msg` caption/echo printing factored into `print_via_core`
      (WorldModel.PrintAsync: OutputText on v540+, failures reported and
      swallowed); menu responses echo " - <display text>".
    - **Core disambiguation needed NO engine prompt**: Core's `ShowMenu` is
      pure ASLX (numbered options via `msg`, callback in `game.menucallback`,
      answer routed by `HandleCommand` → `HandleMenuTextResponse`), so two
      same-alias objects + `take hat` + `1` works end-to-end through
      send_command. Only missing primitive was `IsInt` (+ `IsDouble`), added.
    - Tests: `test/aslx_runtime_test.cc:test_input_model` (fixture
      `command.aslx` gained two same-alias hats + type/pick/query/nap
      commands); `make check` green, ASan/UBSan clean; corpus still
      49/50-boot-0-error, boot output byte-identical except The Acreage, whose
      `wait`-gated intro sections now emerge in the correct authored order.
  - Still open for M3/M4: timers (`SetTimeout`/`SetTimerScript`/`enable`/
    `disable`, a Tick entry point — see §3), `request (Wait)`/`request (Pause)`
    for pre-v540 games (truly blocking mid-script in QuestViva, needs a
    blocking host hook), change (`changed<attr>`) scripts, game-local
    libraries bundled *inside* a `.quest` zip, the UndoLogger, and (maybe)
    member-access-on-null throwing like QuestViva ("Property 'x' not found on
    ''") instead of yielding null. Milestone 5 remains.

## 0. Scope and reality check

Quest 5 shares nothing with Quest 4 except lineage. The game format is XML
(`.aslx`, or `.quest` = a zip containing `game.aslx` plus resources), the world
model is a typed attribute/element system, scripts are a different language,
and expressions are .NET-flavoured (originally evaluated by FLEE). Almost all
game *logic* (parser, verbs, scopes, turn handling) is not in the engine at
all — it lives in `Core.aslx` and friends, libraries written in ASLX itself
and shipped with the player.

Consequence: Quest 5 support cannot be grafted onto the existing ASL text
interpreter (`read_geas_file` → `preprocess`/`decompile` → `GeasFile`). It is
a **second engine sharing the Glk frontend**, exactly the shape of Scarier
(SCARE v4 + new a5 engine behind the `os_glk` seam). Keep the existing engine
for `.asl`/`.cas`, dispatch `.aslx`/`.quest` to the new one.

Reference source (MIT licensed, so Core libraries can be bundled):
<https://github.com/textadventures/quest>

- branch `v5` — classic Quest 5: `WorldModel/` (engine), `WorldModel/WorldModel/Core/`
  (the ASLX core libraries), `Legacy/` (Quest 1–4 converter), `IASL/` +
  `Player/` (UI seam).
- branch `main` — "Quest Viva", a cross-platform .NET port. `src/Engine/` is
  the same WorldModel; FLEE has been replaced by an NCalc-based evaluator
  (`src/Engine/Expressions/NcalcExpressionEvaluator.cs`, ~22 KB + a 25 KB
  parser). Two takeaways: the FLEE expression semantics are re-implementable
  on a small hand-written parser, and Viva runs headless on macOS — perfect
  for a ground-truth diff harness.

## 1. Loading: zip + XML + includes

- [x] Zip reader for `.quest` packages — a small self-contained in-memory
      reader over system zlib (`aslx.cc:extract_game_aslx`), pulls `game.aslx`
      out of the package. (Didn't reuse the scott MiniZip helper — it's coupled
      to scott internals.) Other package resources are left for the presentation
      milestone.
- [x] XML parser for `.aslx` via `libexpat` SAX (`aslx.cc:load_aslx_buffer`).
      ASLX quirks handled (BOM, CDATA, script bodies kept verbatim, nested
      containment, attribute type system). `<include>` refs are now resolved
      inline (see the Core-bundling item below). ASLX quirks to
      preserve:
      - root `<asl version="NNN">` (500–580 seen in the wild) — gates
        compatibility behaviour, like `asl-version` does in `set_game`
        (`geas-runner.cc:1462`);
      - attribute type system: `<attr name="x" type="int">3</attr>`,
        implicit string/boolean forms, `<value>` and CDATA;
      - script text as element bodies where newlines/indentation matter —
        do not whitespace-normalise;
      - nested `<object>` elements (containment by nesting);
      - [x] `<include ref="English.aslx"/>` resolution: game dir first, then
        bundled Core dir, then Core/Languages (QuestViva's `GetLibraryStream()`
        order — note game-adjacent wins, unlike the v5 desktop player). Resolved
        inline during the SAX parse so template precedence matches Quest's
        source-order processing; dedup + depth guard against cycles
        (`aslx.cc:Loader::resolve_include`).
- [x] Bundle the non-editor core libraries as terp resources under
      `terps/geas/aslx-core/` (`Core.aslx`, `CoreCommands`, `CoreParser`,
      `CoreOutput`, `CoreTypes`, `CoreScopes`, `CoreDescriptions`,
      `CoreFunctions`, `CoreTimers`, `CoreTurnScripts`, `CoreStatusAttributes`,
      `CoreGrid`, `CoreWearable`, `CoreCombat`, `CoreEffects`, `GamebookCore`,
      `Languages/*.aslx`). All `CoreEditor*.aslx`/`Editor*.aslx` skipped (they
      are referenced but simply not bundled, so the loader treats a missing
      `*editor*` ref as a no-op). `.template` files are an editor artifact — not
      bundled. Copied verbatim from the QuestViva checkout (MIT); refresh recipe
      in `aslx-core/README.txt`. One version is enough — Core branches on the
      game's declared `<asl version=>`. The core dir is passed to `load_file` or
      taken from `$ASLX_CORE`; app wiring will point it at the resource bundle.
- [x] Templates: `<template>`/`<dynamictemplate>`/`<verbtemplate>` elements are
      parsed and registered (`World::templates` etc.). `[Something]`
      substitution now runs at load time on script bodies and string attributes
      (`aslx.cc:Loader::replace_templates`, mirroring
      `GameLoader.GetTemplate`): later-registered templates win, unmatched
      references are left verbatim (so regex classes survive). Not applied to
      `simplepattern` attributes — a command template like `pattern="[look]"`
      resolves to a raw regex and must skip simplepattern conversion; that path
      belongs to the parser milestone. `Template()`/`DynamicTemplate()`
      built-ins handle the runtime calls. See `Templates.cs`.

## 2. WorldModel port (the new engine core)

Port of `v5:WorldModel/WorldModel/` (or `main:src/Engine/`, which is cleaner):

- [x] **Element/Fields**: elements of type object, command, function,
      turnscript, timer, walkthrough, template, delegate, type, javascript…
      each with a typed field bag (`aslx::Value` variant: string, int, double,
      bool, object ref, script, string/object list, string/object/script dict,
      null). The element id is exposed as a `name` field (QuestViva's
      `FieldDefinitions.Name` — Core keys `cmd.name`/`obj.name` off it, notably
      the RegexCache cacheID). **Lists and dictionaries are reference types**:
      their storage sits behind a `shared_ptr` so copies (function args, `y = x`,
      field reads) alias one backing and `list add`/`dictionary add` propagate,
      matching QuestList/QuestDictionary; derived-list builtins `detach()` first.
      **List entries and dictionary values are full Values** (per-entry typing),
      so one collection can mix objects, strings, numbers and dictionaries
      (CoreParser's resolvedelements; spondre's list-of-dicts). Collections
      compare by reference identity on the shared backing (.NET semantics).
      Delegate values not yet a distinct runtime type.
- [x] **Inheritance**: `<inherit name="type"/>` chains resolved own → inherited
      types most-recent-first, recursively (`Interp::resolve_field`). Each
      element also gets an implicit default type (`defaultobject`/`defaultgame`/
      …) prepended at load, and containment is exposed as a `parent` ObjectRef
      field (children derived from it, so runtime `MoveObject` is honoured).
- [ ] **Change notification**: `changed<attr>` scripts fire on field writes.
      (Not yet — the boot path doesn't need it, but turn handling will.)
- [ ] **UndoLogger**: per-turn transaction log of field writes / element
      creation-destruction; powers `undo` (maps onto the existing
      `LimitStack` idea, but log-based rather than snapshot-based).
- [~] **Script commands** (~45; enumerate `v5:.../Scripts/`): DONE —
      `msg`, `if`/`else if`/`else`, `while`, `for`, `foreach`, `=` assignment
      (var and `obj.attr`), function invocation (statement position), `return`,
      comments (`ScriptFactory.CreateScript` statement splitter ported),
      `switch`/`case`/`default`, `firsttime`/`otherwise`, `do`, `invoke`,
      `create` (+ type), `destroy`, `set` (3-arg field form), `list add`/
      `list remove`, `dictionary add`/`dictionary remove`, `on ready`, `error`,
      `finish`, `undo`/`start transaction` (no-op until UndoLogger), `request`
      (no-op — headless UI request; its enum arg is left unevaluated), and the
      `JS.*` bridge. `msg` routes through Core's `OutputText` (v540+) so the
      `{...}` text processor runs. Also done (2026-07-16): `rundelegate`,
      `create timer`/`create turnscript`, `x => {script}` assignment, trailing
      `{script}` call argument, `this` binding in `do`/`rundelegate`,
      `picture`/`play sound`/`stop sound`/`insert` (headless no-op + warning),
      `wait` (runs callback immediately pending §3), `error` (throws, QuestViva
      semantics). Also done (2026-07-16): `get input`, `show menu`, `ask`,
      real pending `wait` (the §3 input model — fire-and-forget callbacks
      resolved by the host). TODO — `create exit`, `enable`/`disable` timer.
- [~] **Expression evaluator**: hand-written lexer + recursive-descent parser
      over `aslx::Value` (`aslx-runtime.cc`), compiled-expression cache per
      source string. DONE: `=`/`==` equality, `<>`/`!=`, `and or xor not`,
      `< > <= >=`, `+ - * / %` with int→double promotion, `+` string concat,
      the `QuestList<T>` operator overloads (`list+list` merge, `list±elem`
      append/remove-first, `elem+list` prepend, `list*list` union — Core's
      `GetScope` does `list - game.pov`), ternary `?:`, dot member access
      (`box.capacity`), `[]` list/dict indexers,
      `null`/`true`/`false` literals, dispatch to ASLX `<function>` elements and
      ~65 built-ins: type/attribute (`GetBoolean`/`GetInt`/`GetString`/
      `HasAttribute`/`TypeOf` 1- & 2-arg/`DoesInherit`), world model
      (`AllObjects`/`AllCommands`/`GetDirectChildren`/`GetAllChildObjects`/
      `Contains`/`GetAttributeNames`), lists (`NewList`/`ListCount`/`ListItem`/
      `StringListItem`/`ObjectListItem`/`ListContains`/`ListExclude`/
      `ListCombine`), dictionaries (`NewDictionary`/`NewStringDictionary`/
      `NewObjectDictionary`/`NewScriptDictionary`/`DictionaryCount`/
      `DictionaryContains`/`DictionaryItem`/`*DictionaryItem`), strings (`Split`/
      `Join`/`Left`/`Right`/`Mid`/`Instr`/`Replace`/`LCase`/`UCase`/`CapFirst`/
      `SafeXML`), templates (`Template`/`DynamicTemplate`), the regex parser
      primitives (`IsRegexMatch`/`GetMatchStrength`/`Populate`, 2- & 3-arg),
      `Eval(expr[,scope])`, the has-family (`HasScript`/`HasString`/`HasBoolean`/
      `HasInt`/`HasDouble`/`HasObject`/`HasDelegateImplementation`), world-model
      helpers (`AllExits`/`AllTurnScripts`/`GetTimer`/`GetUIOption`), the `To*`
      converters (`ToInt`/`ToDouble`/`ToString`), and numbers
      (`CInt`/`CDbl`/`Abs`/`Max`/`Min`/`GetRandomInt`/`GetRandomDouble`).
      **Operator precedence fixed**: `not` binds looser than `=`/comparison
      (own `parse_not` level), so `not obj = null` is `not (obj = null)` — Core
      depends on this everywhere. Also done (2026-07-16): multi-word
      (space-encoded) identifiers, `in`/`not in`, case-insensitive
      `True`/`False`/`Null`, NCalc `if()`/`cast()`, `GetFileURL`/
      `GetUniqueElementName`/`RunDelegateFunction`; typed (Value-holding)
      lists with reference-identity equality (2026-07-16). TODO: the remaining
      built-ins (DateTime, `FormatList` and the scope helpers) and list/dict
      literals.
- [x] **Command parsing comes free**: implemented in `CoreParser.aslx`, and it
      now runs — `HandleCommand` → `ScopeCommands` → `IsRegexMatch`/
      `GetMatchStrength` → `ResolveName`/`GetScope`/`ResolveNameFromList`/
      `CompareNames` → verb script all execute as library code. `look`/`take`/
      `examine`/`open`/`go` work end-to-end (0 errors). The four engine gaps that
      blocked it: command patterns from `pattern="[tmpl]"`/`template="verb"`
      (not just nested `<pattern>`); the `name` field (RegexCache cacheID);
      reference-type lists/dicts (caller-built match lists); per-entry dict value
      typing (object params to verb scripts). Disambiguation menus work too
      (2026-07-16, `test_input_model`): Core's ShowMenu stores
      `game.menucallback` and the numbered answer is routed by `HandleCommand`
      — all ASLX, no engine prompt involved.
- [x] **Determinism**: `GetRandomInt`/`GetRandomDouble` route through a
      xoshiro128**+SplitMix32 port of `erkyrath_random()` (`aslx::Rng`, seed
      1234, byte-identical to `terps/common_utils/randomness.c` and the oracle's
      `ErkyrathRandom.cs`). App wiring will point it at the real
      `erkyrath_random()` under `SPATTERLIGHT`; see `geas-runner.cc:1421`.

## 3. Input and timing model

Classic Quest 5's engine thread blocked mid-script on `get input` /
`show menu` / `ask` / `wait`; QuestViva (the oracle) instead made them
**fire-and-forget**: the script command registers a callback, the rest of the
enclosing script keeps running, and the next player input resolves it
(`_commandOverride` / `_menuTcs` / `_questionTcs` / `_waitTcs`), with
`_pendingCallbackCount` deferring `on ready` until the prompt resolves. We
ported that model exactly (2026-07-16) — it needs **no nested glk_select and
no second thread**: the Glk prompt loop just asks the engine what it is
waiting for and routes the next line/keypress.

- [x] `get input` → command override: the next `send_command` line binds
      `result` and runs the callback instead of the parser.
- [x] `show menu` / `ask` → `pending_menu()` (MenuData) / `pending_question()`
      exposed to the host; resolved by `set_menu_response(key|null)` /
      `set_question_response(bool)`. Core's own ShowMenu/disambiguation is
      pure ASLX via `game.menucallback` and needs none of this — numbered
      answers arrive as ordinary commands.
- [x] `wait` → `pending_wait()` + `finish_wait()` (host keypress; headless
      harnesses auto-advance like the oracle's AutoAdvance).
- [ ] Glk wiring in geasglk: render pending menus/questions as numbered
      prompts (reuse `make_choice` UI), `wait` as a keypress event.
- [ ] Timer elements + `SetTimeout`/`SetTimerScript` → an engine Tick(secs)
      entry (TimerRunner port) + `glk_request_timer_events` at prompt level
      (precedent: scarier a5 real-time events; the oracle's DrainTimers shows
      the deterministic-headless variant).
- [ ] `request (Wait)` / `request (Pause, ms)` (pre-v540/v550 games only):
      genuinely blocking mid-script even in QuestViva (`DoWaitAsync` is
      awaited) — needs a blocking host hook; currently a no-op.

## 4. Output mapping (HTML/JS → Glk)

Quest 5 emits HTML through the IASL `PrintText` interface and drives a JS UI.

- [ ] HTML-subset → Glk styles converter: `<b><i><u>`, `<br/>`, `<font>`/
      colour best-effort, alignment ignored gracefully. (Core's output is a
      constrained subset; don't write a browser.)
- [ ] Object/command links (`<a class="cmdlink" ...>`) → Glk hyperlinks.
- [ ] `picture` → `glk_image_draw` in the buffer (resources from the zip).
- [ ] `play sound` → Glk sound channels.
- [ ] Panes: compass/inventory/"places and objects"/status attributes →
      reuse the existing sidebar machinery (`objwin`, `bannerwin` in
      `geasglk.cc:83-87`); the data arrives as UI update requests
      (`RequestScript` / `UpdateList` calls).
- [ ] `JS.*` calls: implement the handful Core itself uses (`JS.eval` from
      user games gets a one-time warning and is ignored). Games built around
      custom JavaScript UIs are explicitly out of scope — detect and warn.

## 5. App/frontend integration

- [ ] Dispatch in `glk_main` (`geasglk.cc:364`): sniff the file — `PK\x03\x04`
      zip containing `game.aslx`, or XML whose root is `<asl` → Quest 5
      runner; else the existing `read_geas_file` path. Keep `GeasInterface`
      as the shared host seam if practical.
- [ ] New sources into the geas group of `Spatterlight.xcodeproj` *and* the
      standalone `test/Makefile`.
- [ ] Babel: new `babel/quest5.c` treaty module beside `quest4.c` — claim
      zips whose directory contains `game.aslx` and raw `<asl version=`
      XML. Unlike quest4 it can return real metadata: IFID from the game's
      `gameid` GUID attribute, plus `gamename`/`author` (and cover art from
      the package). Drop `NO_METADATA`/`NO_COVER`.
- [ ] `Info.plist`: register `quest` and `aslx` extensions/UTIs (only `asl`
      + `public.quest` exist today, lines ~668/682/1738/1744).
- [ ] Save/undo: v1 ships our own snapshot format via the existing
      `save_state`/`load_state` seam (`geas-state.cc` pattern) so autosave
      and Spatterlight integration behave like every other terp. Native
      `.quest-save` compatibility (Quest serialises the whole world back to
      ASLX, `GameSaver.cs`) is a later, optional milestone — the SCARE
      `.tas` work proved two-way save compat is doable.

## 6. Legacy dispatch note

Quest 5's own player runs Quest 1–4 games by *converting* them (`Legacy/`).
We do the opposite: `.asl`/`.cas` keep going to the existing native engine,
which is more faithful than the converter. `QCGF001` (Quest 1 compiled)
stays unsupported, as in `quest4.c`.

## 7. Testing

- [x] Corpus: 49 games + 102 walkthroughs pulled into `~/Downloads`
      (textadventures.co.uk + if-archive). See the `quest5-corpus` memo.
- [x] Ground truth: Quest Viva's `src/Engine` builds headless on macOS
      (.NET 10, `terps/geas/test/quest5-oracle/build.sh` clones + builds
      `~/questviva-oracle`). `qvh` driver emits normalised transcripts;
      RNG wired to `erkyrath_random()`. Same recipe as `test/a5_groundtruth.sh`.
- [x] Walkthrough regression scripts: `.quest` games' shipped walkthroughs
      (+ Welbourn corpus) extracted by `extract_walkthrough.py`, driven by
      `run_corpus.sh`; 17 wired (15 win). Frozen as `golden/<Game>.cmd`+`.out`.
- [ ] Extend `terps/geas/test/`: aslx fixtures with `.cmd`/`.expected`
      goldens under `run_fixtures.sh`, walkthrough runner rows for real
      games, `make check` stays the gate. (Oracle goldens exist; the native
      engine still needs its own fixture harness — starts with milestone 1.)

## 8. Milestones

1. **Loader** ✅ (2026-07-15): unzip + XML → element tree; `dump()` debug mode.
   `aslx.hh`/`aslx.cc` + `test/aslx_loader_test.cc`; loads all 76 corpus games.
2. **Script/expression core** ✅ (2026-07-15): runs a hand-written `.aslx`
   (msg/if/while/for/foreach/`=`/functions/return) with no Core library.
   `aslx-runtime.*` + `test/aslx_runtime_test.cc`. Expression evaluator, script
   interpreter, inheritance-aware field resolution, ~40 built-ins, deterministic
   RNG. Remaining script commands / built-ins tracked in §2.
3. **Core.aslx boots + drives commands** (loader ✅ 2026-07-15; boot ✅
   2026-07-15; command driving ✅ 2026-07-16): every corpus game plus a fresh
   default game loads its full Core tree with 0 errors, `InitInterface`+
   `StartGame`+`on ready` execute with 0 errors, and real player commands
   (`look`/`take`/`inventory`/`examine`/`open`/`go`) now run end-to-end through
   Core's `HandleCommand` → scope → resolve → verb-script chain with 0 errors.
   `test/aslx_loader_test.cc:test_coreboot` (load) +
   `test/aslx_runtime_test.cc:test_coreboot_runs` (boot) +
   `:test_command_driving` (commands) + `:test_input_model` (disambiguation).
   Remaining: change scripts.
4. **Interaction** (input model ✅ 2026-07-16): get input, menus, ask, wait
   all land engine-side with host entry points (`send_command` +
   `set_menu_response`/`set_question_response`/`finish_wait`,
   `test_input_model`). Remaining: timers/Tick, undo, save/restore, and the
   geasglk prompt-loop wiring.
5. **Presentation**: HTML→styles, hyperlinks, panes, pictures, sound.
6. **Integration**: babel module, Info.plist, Xcode wiring, corpus
   regression + ground-truth harness green.

Size honesty: milestones 2–3 alone are on the order of the whole existing
runner (`geas-runner.cc` is 5.5 k lines) — the engine primitives are small
individually but numerous, and fidelity lives in the details of the
expression evaluator and field-resolution order. The payoff is that once
those are right, the entire game-logic layer (Core.aslx) is not our code.

Rejected alternatives, for the record: transpiling ASLX to ASL 4 (semantic
gap is unbridgeable — typed attributes, delegates, closures over script
objects); embedding Mono/.NET in Spatterlight (unshippable); reusing QuestJS
(it *converts* games and is not behaviour-faithful).
