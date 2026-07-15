# TODO: Quest 5 support in Geas

## Status (2026-07-15)

- **Ground-truth oracle: built and frozen** (§7, milestone 6's diff half).
  `terps/geas/test/quest5-oracle/` drives real `.quest`/`.aslx` games headless
  through QuestViva (.NET port of the Quest 5 engine), RNG routed through
  `erkyrath_random()` (seed 1234), and captures normalised golden transcripts.
  17 walkthroughs driven — **15 Finished, 1 Running (ICM), 1 Wedged
  (Whitefield)**; the last two are genuine QuestViva-vs-Quest limitations, not
  drift. `check_golden.sh` replays all 17 `.cmd` scripts and diffs vs frozen
  `.out` (17/17), doubling as a determinism check. This is the reference the
  native engine will diff against, with no .NET dependency at replay time.
- **Native engine: milestones 1–2 landed.**
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
    now load too: `listextend` (flagged `Value::list_extend`, merge-on-read
    still TODO), `list` (heterogeneous `<value>` list), and delegate-typed
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
  - Still open for M3: routing `msg` through Core's `OutputText` (v540+) so the
    `{...}` text processor runs, the listextend merge-on-read in field
    resolution, game-local libraries bundled *inside* a `.quest` zip (only the
    Core dir resolves for packaged games today), change scripts, UndoLogger,
    `get input`/`show menu`/`ask`/`wait`, command-template patterns (`[look]`
    in a `pattern=`) and the `IsRegexMatch`/`GetMatchStrength`/`Populate`
    parser primitives, the game-boot driver (`InitInterface` → `StartGame` →
    `HandleCommand`), and multi-word (space-encoded) identifiers. Then
    `look`/`take`/`go` running as Core library code. Milestones 3 (rest)–5
    remain.

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
      null). Delegate values not yet a distinct runtime type.
- [x] **Inheritance**: `<inherit name="type"/>` chains resolved own → inherited
      types most-recent-first, recursively (`Interp::resolve_field`).
- [ ] **Change notification**: `changed<attr>` scripts fire on field writes.
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
      `finish`, `undo`/`start transaction` (no-op until UndoLogger), and the
      `JS.*` bridge. TODO — `create exit`, `rundelegate`, `get input`,
      `show menu`, `ask`, `wait`, `picture`, `play sound`/`stop sound`,
      `request`, `create timer`/`enable`/`disable`.
- [~] **Expression evaluator**: hand-written lexer + recursive-descent parser
      over `aslx::Value` (`aslx-runtime.cc`), compiled-expression cache per
      source string. DONE: `=`/`==` equality, `<>`/`!=`, `and or xor not`,
      `< > <= >=`, `+ - * / %` with int→double promotion, `+` string concat,
      ternary `?:`, dot member access (`box.capacity`), `[]` list/dict indexers,
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
      `SafeXML`), templates (`Template`/`DynamicTemplate`), and numbers
      (`CInt`/`CDbl`/`Abs`/`Max`/`Min`/`GetRandomInt`/`GetRandomDouble`). TODO:
      the remaining ~85 built-ins (`Eval`, DateTime, `IsRegexMatch`/regex,
      `FormatList` and the scope helpers), list/dict literals, and multi-word
      (space-encoded) identifiers.
- [ ] **Command parsing comes free**: it is implemented in `CoreParser.aslx`.
      Once elements + scripts + expressions + regex support
      (`RegexCache.cs`) work, `look`/`take`/disambiguation all run as
      library code. Needs a regex engine compatible with .NET named groups
      `(?<object1>.*)` — `std::regex` ECMAScript mode covers what Core uses,
      but verify.
- [x] **Determinism**: `GetRandomInt`/`GetRandomDouble` route through a
      xoshiro128**+SplitMix32 port of `erkyrath_random()` (`aslx::Rng`, seed
      1234, byte-identical to `terps/common_utils/randomness.c` and the oracle's
      `ErkyrathRandom.cs`). App wiring will point it at the real
      `erkyrath_random()` under `SPATTERLIGHT`; see `geas-runner.cc:1421`.

## 3. Input and timing model

Quest 5's engine thread blocks mid-script on `get input` / `show menu` /
`ask` / `wait` until the UI responds. Under single-threaded Glk, do what
geasglk already does for `get_string`/`make_choice` (`GeasRunner.hh`,
`geasglk.cc:63-64`): re-enter `glk_select` from inside script execution.
No second thread.

- [ ] `get input` → nested line event in the main buffer.
- [ ] `show menu` / `ask` → numbered-menu prompt (reuse `make_choice` UI).
- [ ] `wait` → keypress event.
- [ ] Timer elements + `SetTimeout`/`SetTimerScript` → `glk_request_timer_events`
      at prompt level (precedent: scarier a5 real-time events).

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
3. **Core.aslx boots** (loader half ✅ 2026-07-15): every corpus game plus a
   fresh default game loads its full Core element tree with 0 errors
   (`aslx-core/` bundle + inline `<include>` resolution; `listextend`/`list`/
   delegate-typed attributes handled). `test/aslx_loader_test.cc:test_coreboot`.
   Remaining half: `look`, `take`, `go` actually running (proves parser-in-ASLX,
   scopes, templates) — needs template substitution + more script commands.
4. **Interaction**: get input, menus, wait, timers, undo, save/restore.
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
