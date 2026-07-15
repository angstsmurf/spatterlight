# TODO: Quest 5 support in Geas

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

- [ ] Zip reader for `.quest` packages (vendor `miniz` or reuse whatever the
      repo already links). The package holds `game.aslx` plus images/sounds;
      the core libraries are *not* packaged — the interpreter supplies them.
- [ ] XML parser for `.aslx` (system `libexpat` is fine). ASLX quirks to
      preserve:
      - root `<asl version="NNN">` (500–580 seen in the wild) — gates
        compatibility behaviour, like `asl-version` does in `set_game`
        (`geas-runner.cc:1462`);
      - attribute type system: `<attr name="x" type="int">3</attr>`,
        implicit string/boolean forms, `<value>` and CDATA;
      - script text as element bodies where newlines/indentation matter —
        do not whitespace-normalise;
      - nested `<object>` elements (containment by nesting);
      - `<include ref="English.aslx"/>` resolution: bundled library dir
        first, then game dir (recursion guard like `readfile.cc:824`).
- [ ] Bundle the non-editor core libraries from
      `v5:WorldModel/WorldModel/Core/` as terp resources (~260 KB total:
      `Core.aslx`, `CoreCommands`, `CoreParser`, `CoreOutput`, `CoreTypes`,
      `CoreScopes`, `CoreDescriptions`, `CoreFunctions`, `CoreTimers`,
      `CoreTurnScripts`, `CoreStatusAttributes`, `CoreGrid`, `CoreWearable`,
      `CoreCombat`, `CoreEffects`, `GamebookCore`, `Languages/*.aslx`,
      `Templates/`). Skip all `CoreEditor*.aslx`. One current version is
      enough — Core itself branches on the game's declared ASL version.
- [ ] Templates: `<template>`/`<dynamictemplate>` elements and `[Something]`
      substitution in strings (see `Templates.cs`).

## 2. WorldModel port (the new engine core)

Port of `v5:WorldModel/WorldModel/` (or `main:src/Engine/`, which is cleaner):

- [ ] **Element/Fields**: elements of type object, command, function,
      turnscript, timer, walkthrough, template, delegate, type, javascript…
      each with a typed field bag. Types needed: string, int, double, bool,
      object reference, script, stringlist/objectlist, stringdictionary/
      objectdictionary/scriptdictionary, delegate, null. Use a value variant
      (precedent: scarier's a5 value types).
- [ ] **Inheritance**: `<inherit name="type"/>` chains with Quest's field
      resolution order (own fields → inherited types, most recent first).
- [ ] **Change notification**: `changed<attr>` scripts fire on field writes.
- [ ] **UndoLogger**: per-turn transaction log of field writes / element
      creation-destruction; powers `undo` (maps onto the existing
      `LimitStack` idea, but log-based rather than snapshot-based).
- [ ] **Script commands** (~45; enumerate `v5:.../Scripts/`): `msg`, `if`,
      `else if`, `while`, `for`, `foreach`, `switch`, `=` assignment,
      `set`, `create`/`create exit`, `destroy`, function invocation
      (statement position), `invoke`, `rundelegate`, `do`, `get input`,
      `show menu`, `ask`/`Ask`, `wait`, `on ready`, `firsttime`, `picture`,
      `play sound`/`stop sound`, `JS.*`, `request`, `list add/remove`,
      `dictionary add/remove`, `undo`, `finish`, `return`, `error`,
      `create timer`/`enable`/`disable`. Registration table à la
      `ScriptFactory.cs`.
- [ ] **Expression evaluator** (the single biggest chunk): replaces FLEE.
      Hand-written lexer + recursive-descent/Pratt parser over a typed value
      variant (precedent: a5sexpr in scarier). Semantics to match:
      - `=` is equality in expressions, `<>` inequality; `and or not`;
        arithmetic with int→double promotion; `+` concatenates strings;
      - dot access walks object attributes (`player.parent.alias`);
        `game`, `player`, `this` resolve as elements;
      - function calls dispatch to ASLX `<function>` elements *and* the
        built-ins in `ExpressionOwner.cs` / `StringFunctions.cs` /
        `DateTimeFunctions.cs` (~150 functions: `GetBoolean`, `HasAttribute`,
        `ListContains`, `Split`, `TypeOf`, `GetRandomInt`, `Eval`, …);
      - list/dictionary indexers, `null` literal.
      Cache compiled expressions per source string (Quest does; also mirrors
      the `GeasBlock::ensure_cached` lesson — re-parsing per turn was the
      Quest-4 hotspot too).
- [ ] **Command parsing comes free**: it is implemented in `CoreParser.aslx`.
      Once elements + scripts + expressions + regex support
      (`RegexCache.cs`) work, `look`/`take`/disambiguation all run as
      library code. Needs a regex engine compatible with .NET named groups
      `(?<object1>.*)` — `std::regex` ECMAScript mode covers what Core uses,
      but verify.
- [ ] **Determinism**: route `GetRandomInt`/`GetRandomDouble` through
      `erkyrath_random()` under `SPATTERLIGHT` (same policy as every other
      terp; see `geas-runner.cc:1421`).

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

- [ ] Corpus: there are no Quest 5 games in `~/Desktop/Geas games` — pull
      from textadventures.co.uk and `if-archive/games/quest`.
- [ ] Ground truth: build Quest Viva's `src/Engine` headless on macOS
      (.NET 8) and diff transcripts against ours for identical command
      scripts — same recipe as `test/a5_groundtruth.sh` vs FrankenDrift.
- [ ] Many `.quest` games ship `<walkthrough>` elements; the engine's
      `Walkthrough.cs` shows the format — use them as free regression
      scripts.
- [ ] Extend `terps/geas/test/`: aslx fixtures with `.cmd`/`.expected`
      goldens under `run_fixtures.sh`, walkthrough runner rows for real
      games, `make check` stays the gate.

## 8. Milestones

1. **Loader**: unzip + XML → element tree; dump-elements debug mode.
2. **Script/expression core**: run a hand-written `.aslx` (msg/if/set/
   functions) with no Core library.
3. **Core.aslx boots**: a fresh Quest-editor default game loads; `look`,
   `take`, `go` work (proves parser-in-ASLX, scopes, templates).
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
