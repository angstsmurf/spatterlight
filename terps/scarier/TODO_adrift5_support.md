# TODO — ADRIFT 5 support in Scarier

Add support for **ADRIFT 5.0** games to the Scarier interpreter, validated against
the IFComp 2018 entry **Six Silver Bullets**:

```
.../IFComp2018/Games/Six Silver Bullets/six silver bullets/Six Silver Bullets (v 1.0).blorb
(== bullets.blorb, 376 934 bytes)
```

Concepts and algorithms are borrowed from the VB.NET reimplementation of the
official ADRIFT 5 Runner, **frankendrift** (`/Users/administrator/frankendrift`,
engine in `FrankenDrift.Adrift/*.vb`). Blorb/IFF container handling is borrowed
from **Bocfel** (`terps/bocfel/iff.cpp`, `blorb.cpp`).

> ⚠️ **Scope reality check.** ADRIFT 5 is *not* an incremental change to the
> 3.8/3.9/4.0 reader. It is a different file format (zlib-compressed XML instead
> of a binary property tree) driving a different, property-centric, string-key
> object model. frankendrift's engine alone is ~25 000 lines of VB.NET. This is a
> **second engine living next to the existing one**, selected at load time. Plan
> it in phases; do not try to graft v5 onto `sctafpar`/`scgamest`.

---

## 1. Architecture decision: a parallel v5 path

Keep the entire existing v3.8/3.9/4.0 pipeline (`sctaffil` → `sctafpar` →
`scgamest`/`sctasks`/`screstrs`/`scrunner` …) **untouched**. Add a sibling v5
pipeline and branch on the detected version.

```
sctaffil (load + container + version detect)
   ├─ v3.8/3.9/4.0 ─> sctafpar  ─> scgamest (binary property tree)  [existing]
   └─ v5.0          ─> a5parse  ─> a5model  ─> a5run                 [NEW]
```

Proposed new files (names indicative; keep the `sc`/`a5` prefix convention and
C-like C++ style established by the rename):

| New file | Responsibility | Borrow from |
|---|---|---|
| `a5blorb.cpp/.h` | IFF/Blorb container: find `Exec`(`ADRI`) + `Pict`/`Snd`/`Data` | Bocfel `iff.cpp`, `blorb.cpp`, `io.h` |
| `a5deobf.cpp` | v5 de-obfuscation (1024-byte XOR pad) + zlib inflate → XML | frankendrift `FileIO.ObfuscateByteArray`/`Decompress`; reuse our `zlib` |
| `a5xml.cpp/.h` | minimal XML reader (DOM or pull) for .NET-`XmlTextWriter` output | (new — small) |
| `a5model.cpp/.h` | v5 object model structs + hashtable-by-key access | frankendrift `cls*.vb` |
| `a5load.cpp` | walk the XML tree → populate `a5model` | frankendrift `FileIO.Load500` + `Load*` helpers |
| `a5text.cpp` | text engine: ALRs, variables, `%functions%`, perspective/pronouns | frankendrift `Global.vb`, `clsALR`, `clsUserFunction`, `_FrankenDrift_Pronouns` |
| `a5expr.cpp/.h` | OO property-expression engine (`%key%.Prop.Func(args)`) | frankendrift `Global.ReplaceOO`/`ReplaceOOProperty` |
| `a5parse.cpp` | command → general-reference resolution + task matching | frankendrift `clsUserSession` parser, `clsTask` |
| `a5run.cpp` | turn loop, restriction eval, action execution, events, characters | frankendrift `clsUserSession`, `clsEvent`, `clsCharacter`, `clsTask` |
| `a5state.cpp` | v5 save/restore (XML game-state) | frankendrift `FileIO.SaveState`/`LoadState`, `clsState` |

`scinterf.cpp` (the public `scr_game_from_*` / `scr_interpret_game` API) gains a
v5 branch but keeps the same opaque `scr_game` handle so the os layer
(`os_glk.cpp`, `os_ansi.cpp`) and Spatterlight need **no** changes for text.

---

## 2. Load pipeline — concrete recipe

Verified against `bullets.blorb` (`FORM … IFRS`, resource index `Exec` → chunk
type `ADRI` at file offset 0x24; chunk magic `3c 42 3f c9 6a 87 c2 cf 92 45 3e 61
30 30 30 30 …` = the ADRIFT signature with the v5 marker).

1. **Container** (Bocfel): if the file is `FORM`/`IFRS`, parse the resource index,
   take the `Exec` resource (its chunk type must be `ADRI`). A bare `.taf` skips
   this step. Also remember `Pict`/`Snd`/`Data` chunks for Phase 5.
2. **Version detect**: read the 12-byte signature. Existing 14-byte v4/v3.9/v3.8
   signatures in `sctaffil.cpp` end `… 39 fa`. **v5** instead has bytes 12–15 ==
   `"0000"` (`30 30 30 30`). So: signature[8]…, then `data[12..15]=="0000"` ⇒ v5.
   (frankendrift `FileIO.LoadFile` lines ~725–770.)
3. **De-obfuscate**: XOR the payload with a fixed **1024-byte key**, repeating
   (`key[(i-offset) mod 1024]`). The full table is `iRandomKey` in
   `FileIO.ObfuscateByteArray` (FileIO.vb:1066) — copy it verbatim, **or** verify
   it equals the first 1024 bytes of our existing VB-PRNG stream in `sctaffil.cpp`
   (`PRNG_*`, `taf_random_state`) and generate it. Obfuscation is conditional:
   frankendrift's heuristic is "deobfuscate unless Babel metadata exists and does
   *not* mention `compilerversion`". Implement the same toggle.
4. **Offsets** (frankendrift FileIO.vb:753–756): for a blorb with `data[12..15]==
   "0000"`, the zlib region is `offset = 16, length = total − 30` (16-byte header
   + 14-byte trailer). Otherwise `offset = 12, length = total − 26`. The bare-taf
   path uses `offset = 16, length = total − 30` (FileIO.vb:712).
5. **Inflate**: zlib-decompress the region (we already link zlib;
   `taf_decompress` in `sctaffil.cpp` shows the inflate setup). Result is UTF-8
   XML.
6. **Parse** the XML into `a5model` (Phase 1+).

**Phase-0 acceptance test:** dump the decompressed XML for `bullets.blorb` to
stdout and diff its structure against frankendrift loading the same file. No model
needed yet — this proves container + cipher + inflate.

---

## 3. The v5 object model (Phase 1)

Top-level container = the Adventure, keyed collections (frankendrift
`clsAdventure.vb:6–23`). Everything is addressed by **string key** ("Location1",
"Object3", "Task12", "Property5" …), not by index.

Port these as C structs + a key→record lookup (hash map or sorted array):

- **Adventure**: dictionaries of Locations, Objects, Tasks, Events, Characters,
  Groups, Variables, ALRs, Hints, UDFs, Synonyms; property *definitions* split
  into All/Object/Character/Location property tables; `listKnownWords`; player
  character key; intro/ending text; start location/task priority.
- **Property** (`clsProperty.vb`, 304 lines) — **the core of v5**. Properties are
  typed (boolean / integer / text / selection-from-list / object-ref / location-
  group / state-list …) and attached to Locations, Objects, Characters. Most of
  the world state lives in property values. Get this right first.
- **Location** (`clsLocation.vb`): descriptions (list of `SingleDescription` with
  `DisplayWhen` + restrictions), directional exits (each exit is a restricted
  move), properties.
- **Object** (`clsObject.vb`, 1080 lines): static vs dynamic, openable/lockable/
  container/surface/wearable/edible/sittable, locations ("at location" / "inside"
  / "on" / "held by" / "worn by" / "part of"), descriptions, properties.
- **Character** (`clsCharacter.vb`, 1875 lines): player + NPCs, walks, topics/
  conversation, gender/pronouns, properties, "seen" state.
- **Task** (`clsTask.vb`, 1528 lines): the verb engine — see §4.
- **Event** (`clsEvent.vb`): timed/triggered state machines with sub-events.
- **Variable** (`clsVariable.vb`): named integer/text (incl. arrays); `Score`,
  `MaxScore`, `Turns`, etc. are variables.
- **ALR** (`clsALR.vb`): "Adventure Language Resource" — output text substitutions.
- **UserFunction** (`clsUserFunction.vb`): `%FunctionName[args]%` text functions.
- **Group** (`clsGroup.vb`): named sets of objects/locations/characters used by
  restrictions/actions.
- **Synonym** (`clsSynonym.vb`), **Hint** (`clsHint.vb`), **Map** (`clsMap.vb`,
  layout only — likely ignorable for a text port).

`StronglyTypedCollections.vb` (1165 lines) is just the typed dictionaries/array
lists — in C these become a couple of small generic containers; don't port 1:1.

---

## 4. Tasks, restrictions, actions (Phases 3–4)

Tasks are the heart of ADRIFT (`clsTask.vb`):

- **TaskType**: General (player verbs), Specific (overrides for a specific noun),
  System (auto/triggered). Plus priority, `LowPriority`,
  `ContinueToExecuteLowerPriority`, `Scored`, `PreventOverriding`.
- **Commands**: author patterns like `take/get/pick up {the} %object%` with
  optional `{...}`, alternation `[a/b]`, and **general references**
  (`%object%`, `%character%`, `%direction%`, `%number%`, `%text%`). frankendrift
  compiles each to a .NET `Regex` (`clsTask.RegExs`). **Decision needed (risk):**
  in C either (a) translate patterns to POSIX ERE (`<regex.h>`) or (b) write a
  small bespoke matcher. The matcher must also produce the bound references and
  feed disambiguation. This is the single biggest parser sub-project.
- **Restrictions** (`clsRestriction`, clsTask.vb:506+): typed
  (Location/Object/Task/Character/Item/Variable/…), `Must`/`MustNot`, up to two
  keys + an operator/enum (e.g. object Exist/Visible/seen/held/at-location;
  task complete/not-complete; variable comparisons). A task fires only if all
  restrictions pass; otherwise its `FailMessage` may show. Implement as a typed
  eval switch over the model — mirror frankendrift's enums exactly.
- **Actions** (`clsAction`): move object/character, set property, set variable,
  execute/unset task, end game, display text, change score, etc.
- **Reference resolution / disambiguation**: map the matched `%object%` text to a
  concrete key using scope (visible/reachable), known words, synonyms and groups;
  prompt "Which do you mean…" when ambiguous (`_FrankenDrift_NewReference.vb`).

Turn loop (frankendrift `clsUserSession`): read input → expand ALR/abbreviations →
match against general tasks by priority → resolve refs → check restrictions → run
actions → run system tasks/events → advance characters/walks → tick timers →
flush output. `qTasksToRun`/`lstTasks` show the ordering model.

---

## 5. Text engine (Phase 2, deepened in 4)

ADRIFT text is full of embedded directives evaluated at display time:

- **Variables**: `%varname%`, array access.
- **Functions**: built-ins (`%PCase[]%`, `%CharacterName[]%`, `%TheObject[]%`,
  `%ListObjectsOn[]%`, `%LocationName[]%`, perspective/number helpers …) plus
  user UDFs. See `Global.vb` (4028 lines) and `clsUserFunction.vb`.
- **ALR** substitution passes (`clsALR`).
- **Perspective & pronouns** (1st/2nd/3rd person; it/him/her/them) —
  `_FrankenDrift_Pronouns.vb`.
- **Formatting tags** (font/colour/bold/centre/wait/clear) — map onto Scarier's
  existing `SCR_TAG_*` tag pipeline in `scprintf.cpp`/`os_glk.cpp`; ADRIFT 5 uses
  HTML-ish tags, so add a tag translator rather than reusing the v4 tag set 1:1.

---

## 6. Resources & save (Phase 5)

- **Images / sounds**: ADRIFT 5 references media by blorb resource number via
  `BlorbMappings` (filename → resource #) embedded in the game. Use Bocfel's
  blorb `find(Pict|Snd|Data, n)` to fetch bytes, then hand to the Glk layer
  exactly as `os_glk.cpp` already does for other terps (giblorb). Headless/ANSI
  builds ignore media.
- **Save/restore**: v5 saves are their own XML game-state (`FileIO.SaveState`/
  `LoadState`, `clsState.vb`, 516 lines). Implement last; until then, rely on
  Scarier's existing Glk autosave of the process state if feasible, or stub.

---

## 7. Integration points in the existing tree

- `sctaffil.cpp`: add v5 signature/marker detection; on v5, route to the new
  loader instead of `sctafpar`. Reuse its zlib and (optionally) PRNG.
- `scinterf.cpp`: `scr_game_from_filename`/`_stream`/`_callback` detect v5 and
  build a v5 game behind the same opaque `scr_game`; `scr_interpret_game`,
  `scr_save_game`, `scr_load_game`, hints, etc. dispatch on an engine tag stored
  in the handle.
- `scprotos.h` / `scarier.h`: declare the new internal API; keep the **public**
  `extern "C"` surface unchanged so Spatterlight/os layers are unaffected.
- Build: add the new `a5*.cpp` (and borrowed `iff.cpp`/`blorb.cpp`/`io.*`) to
  `Makefile`, `Makefile.headless` (`SRC := $(wildcard sc*.cpp a5*.cpp)` etc.), and
  the Spatterlight Xcode target. Borrowed Bocfel files use STL/exceptions — fine
  now that Scarier is C++, but keep their use contained to the container layer.

---

## 8. Testing strategy

- Extend `Makefile.headless` with an `a5` harness that loads `bullets.blorb`
  headlessly and feeds a command script (like the existing battle/precedence
  harnesses), capped with `ulimit`/`head` per the SCARE-harness OOM note.
- **Ground truth** — DONE (harness): a headless FrankenDrift runner now exists.
  Rather than the Glk runner (needs a Gargoyle shared lib), a ~200-line console
  frontend (`FrankenDrift.Headless`, preserved at
  `test/frankendrift-headless/`) implements the tiny `UIGlue`/`frmRunner`/
  `RichTextBox`/`Map` interfaces and drives the engine via
  `UserSession.Process`; the sole text sink is `UIGlue.OutputHTML`, stripped to
  plain text like `GlkHtmlWin`.  It forces a fixed RNG seed (reflection on the
  private `SharedModule.r`) so runs are reproducible, and skips real-time timer
  events so they are turn-deterministic.  `test/a5_groundtruth.sh
  <game> <script>` runs the same script through both engines and prints a
  whitespace-normalised diff.  **RNG caveat:** FrankenDrift uses .NET
  `System.Random`, Scarier the erkyrath xoshiro128\*\* — RAND-selected text
  (dream/epigraph variants, combat rolls, the `Roller==1` catch-all) will NOT
  match; diff the RAND-independent portions.
  - **Divergences surfaced by the first diff (concrete P4/P5 follow-ups, not yet
    fixed):**
    1. **Take "(from X)" line** — the stock `TakeObjectsFromOthersLazy` emits
       `(from %objects%.Parent.Name)`.  FrankenDrift renders `(from the Table)`
       *before* the "You take … from the Table." sub-task message (parent still
       the source); Scarier runs the inline `SetTasks Execute` first, so it
       renders `(from you)` *after* (parent already the player).  The exact
       ordering is governed by FrankenDrift's position-based `AddResponse`/
       `htblResponses` output assembly — needs careful porting, do not guess.
    2. **`%ListObjectsOn%`-style list** — examine-table shows
       `The Yellow Note, The Silver Gun and The Silver Bullets` vs FrankenDrift's
       `the yellow note, the silver gun and the silver bullets are on the
       Table.` (lowercase articles + an " are on the <container>." suffix).
    3. **Start-room auto-LOOK** — Scarier renders the start location
       ("Dead or Dreaming?") description during the intro; FrankenDrift does not
       auto-look the start room (it shows only the intro/WAKE-UP text).
- Keep the original `.exe` (official Runner) for spot-checks.
- Add per-phase dumps mirroring the existing `SCR_DUMP_*` env knobs:
  `A5_DUMP_XML` (decompressed XML), `A5_DUMP_MODEL` (parsed objects/properties),
  `A5_TRACE_TASKS` (match + restriction eval), to debug divergences.
- Milestone goal: a deterministic walkthrough of **Six Silver Bullets** to a
  known score/ending, checked into `adrift-walkthroughs/` like the v4 games.

---

## 9. Phase checklist

- [x] **P0 Plumbing**: blorb(Bocfel) + v5 detect + 1024-byte XOR + inflate →
      dump XML for `bullets.blorb`. **DONE** — `a5blorb.cpp`/`a5deobf.cpp` +
      `test/a5dump.cpp` (`make -f Makefile.headless a5dump`). Extracts 2 337 625
      bytes of well-formed XML from Six Silver Bullets (`<Adventure>`: 71
      Locations, 171 Objects, 1046 Tasks, 18 Characters, 77 Variables, 58
      Properties, 16 Groups, 5 Events). zlib header `78 da` recovers after the
      XOR; header=16/trailer=14 layout confirmed.
- [x] **P1 Model+load**: XML reader + typed object model. **DONE** —
      `a5xml.cpp` (in-situ DOM pull parser: BOM/prolog skip, entity decode,
      `<x />` self-close, verbatim leaf text) + `a5model.cpp` (typed index over
      the DOM: Adventure header + Property/Location/Object/Character/Task/Event/
      Variable/Group/ALR(`TextOverride`)/UDF(`Function`), property maps, name/
      command/member/descriptor lists, per-type key lookup) + `test/a5model_dump.cpp`
      (`make -f Makefile.headless a5model`). Parses Six Silver Bullets fully
      (counts match the XML exactly; Player→Location44, ALR `OkYou`, tasks with
      `%refs%`/`{}`/`[]` commands), **ASan/UBSan-clean**. NOTE: descriptions,
      restrictions, actions, walks, topics and sub-events are retained as DOM
      node pointers for the later phases to interpret — they are loaded but not
      yet decoded.
- [x] **P2 Static world**: text engine MVP (vars/ALR/core functions/tags);
      LOOK / room + object descriptions / inventory render correctly. **DONE** —
      `a5state.cpp` (runtime: object/character location decode incl. recursive
      `ExistsAtLocation`, variable values, per-task done flags — mirrors
      `clsObject.Location`/`ExistsAtLocation`), `a5restr.cpp` (restriction eval:
      a ported `EvaluateRestrictionBlock` over the `#`/`A`/`O`/brackets
      `BracketSequence` with `A#O→(#A#)O` normalisation, plus a typed
      `PassSingleRestriction` covering the Object/Location/Variable/Task/Character
      operators the static world's descriptions use), and `a5text.cpp` (the
      engine: `Description.ToString` DisplayWhen/Restrictions join with the
      `AddSpace` double-space rule, `%function%`/`%variable%` substitution with
      recursive `[args]`, ALR "Text Override" passes, ADRIFT sentence
      auto-capitalisation, and an HTML-ish tag/entity → plain-text renderer
      mirroring `GlkHtmlWin`).  Harness `test/a5text_dump.cpp`
      (`make -f Makefile.headless a5text`; `A5_TRACE_RESTR=1` traces restriction
      eval) renders Introduction / LOOK at start + a named room / object
      descriptions / inventory.  Verified on **Six Silver Bullets** (the
      restricted "On the table:" segments resolve correctly — note/gun/bullets
      shown via per-description object restrictions), and the loader+engine also
      render **Anno 1700** and **Stone of Wisdom** intros + start rooms cleanly
      (3/3 games — obfuscation toggle holds). **ASan/UBSan-clean.** NOTE:
      DisplayOnce, description FailMessage text, the full perspective/pronoun +
      UDF engine, and "seen"/visibility tracking are Phase 4 (HaveBeenSeen* /
      BeVisibleTo* are approximated for now).  Ground-truth diff vs the official
      Runner / frankendrift transcripts still TODO (no VB.NET build set up yet).
- [x] **P3 Verbs (core)**: command matcher + reference resolution + turn loop +
      action executor. **DONE (movement fully working; take/examine gated on the
      P4 items below).** New files:
      `a5parse.cpp/.h` (ported `ConvertToRE`/`GetRegularExpression`: author
      command → `std::regex` with the `[a/b]`/`{..}`/`_`/`*` transforms and
      `%ref%` → capture groups; singular-ref normalisation `%object%`→`%object1%`
      per FileIO.vb:647; **the bare-direction edge case** — a literal space after
      an optional `)?` group is made optional so `{[go]{to{the}}} %direction%`
      matches a bare `se`; `a5parse_canonical_direction` se→`SouthEast`),
      `a5run.cpp/.h` (port of `GetGeneralTask`/`ExecuteActions`: walk General
      tasks ascending `Priority`, match command, resolve refs in scope, eval
      restrictions, run the first passing task's actions + completion message,
      else surface the first failing restriction's `<Message>`; actions:
      MoveObject {ToCarriedBy/Onto/Inside/ToWornBy/ToLocation/ToLocationGroup},
      MoveCharacter {InDirection via the location Movement table, ToLocation,
      To{Standing/Sitting/Lying}On}, Set/Inc/DecVariable (+ `RAND` deterministic
      lower-bound stub), SetProperty, EndGame, SetTasks Execute, built-in Look).
      Extended: `a5state` (char positions, a `SetProperty` override layer, the
      per-turn reference-binding table), `a5restr` (`HaveRouteInDirection` +
      `a5restr_exit_in_direction`, `BeExactText`, `BeInState` via overrides,
      `a5restr_fail_message`, reference-bound `resolve_key`, **the real `[`→`((`
      / `]`→`))` bracket expansion from FileIO.vb:640 — single-bracket conversion
      was wrong and unbalanced the TakeObjects sequence**), `a5text`
      (`%direction%`/`%text%`/`%object%`/`%TheObject%` from bindings + `[//s]`
      perspective conjugation).  Harness `test/a5run_dump.cpp`
      (`make -f Makefile.headless a5run`; `A5_TRACE_RUN`/`A5_TRACE_RESTR`) +
      self-contained matcher regression `test/a5parse_test.cpp` (in `make test`).
      **Validated:** movement (compass + go/walk + route-blocked messages),
      LOOK, room contents/visibility, restriction gating render correctly on
      **Six Silver Bullets**, **Stone of Wisdom** AND **Anno 1700**
      (intro→start→move), **ASan/UBSan-clean** on all three.  Fixed a
      use-after-free (action operands must resolve to stable model-key pointers,
      not temporary token / per-turn-binding buffers — `act_key`/`canon_key`).
      **NOT yet done (folded into P4): general-reference disambiguation prompt
      (currently picks the first in-scope match); the ADRIFT property-expression
      engine `%X.Property%` / `%X.Location.Description%` (so bare `examine` shows
      `%object%.Description` raw and the stock inventory line is unrendered); and
      — key finding — the **bundled ADRIFT 5 Standard Library merge**: the stock
      verb tasks (`TakeObjects`, `DropObjects`, …) ship in the game file with NO
      `<Actions>`/`<CompletionMessage>`; their behaviour comes from the standard
      library frankendrift loads separately and merges (FileIO `bLibrary` /
      `ShouldWeLoadLibraryItem`).  So `take`/`drop`/`open` need that library
      ported/bundled (cf. the Geas typelib approach) — tasks with embedded
      actions (PlayerMovement, "Take the money", custom puzzle tasks) already run.
- [~] **P4 Dynamic**: **core verbs + property/text engine + disambiguation +
      events/timers DONE; walks/conversation/scoring remain.**
      - **Key correction to the P3 "standard-library merge" finding:** the stock
        verbs are **not** missing their actions.  Six Silver Bullets (and every v5
        game) **ships the full ADRIFT Standard Library inside the game file** —
        `TakeObjects`/`DropObjects`/`ExamineObjects`/… all carry their
        `<Restrictions>`/`<Actions>`/`<CompletionMessage>` (the library is merged
        at *compile* time, not load time; frankendrift's `GetLibraries` is dead
        code).  The real mechanism is **General-parent + Specific-child override
        dispatch**: a `<Type>General</Type>` parent task (e.g. `TakeObjects`,
        priority/visibility gating, no action) plus `<Type>Specific</Type>` child
        tasks linked by `<GeneralTask>` with `<Specific>` ref constraints and a
        `<SpecificOverrideType>` (e.g. `TakeObjectFromLocation` = the actual
        `MoveObject ToCarriedBy`).  **Implemented** (`a5run.cpp run_general` +
        `refs_match_specifics` + the `a5_specific_t`/`general_key`/`override_type`
        model fields): walk the matched General task's Specific children in
        priority order, run the first whose specifics match the resolved refs and
        whose restrictions pass (Override replaces parent text+actions;
        Before*/After* compose).
      - **Property-expression engine `%X.Property%`** — DONE: new
        `a5expr.cpp/.h` ports `Global.ReplaceOOProperty`/`ReplaceOO`: a key (or
        `key1|key2` pipe list) navigated by `.Func(args)` steps —
        `Name`/`List`/`Description`/`Count`/`Sum`/`Parent`/`Children`/`Contents`/
        `Location`/`Held`/`Worn`/`Objects` + bare `<property>` filter — over
        objects/characters/locations/groups.  `a5text` emits the resolved entity
        **key** for a `%ref%.` token and runs `a5expr_replace` as a post-pass
        (so `%object%.Description`, `%objects%.Name`, `%objects%.Parent.Name`
        resolve).  **The "<>" segment-separator fix** (frankendrift
        `Description.ToString`): appended `SingleDescription`s are joined with a
        `<>` zero-width marker + an `AddSpace` rule that recognises a trailing OO
        expression — without it `…Description` and the next segment merge into a
        corrupt `…DescriptionThe…` token (was rendering a stray `0`).
      - **Perspective / pronouns** — DONE for the common case:
        `a5text_character_subjective` (+ `%CharacterName[key,pronoun]%`) renders
        first/second-person as the pronoun (`I`/`you`) and third-person as the
        name, used by `%CharacterName%`, `%Player%` and OO `.Name`.  (Full
        mention-tracking pronoun substitution for NPCs still simplified.)
      - **Text functions** added: `%ParentOf%`, `%NumberAsText%`, and the
        `%ListObjectsOn/In/OnAndIn%`, `%ListCharactersOn/In/OnAndIn%`,
        `%ListHeld/ListWorn%`, `%ListObjectsAtLocation%` family.
      - **`SetTasks Execute Task (arg|arg)` reference passing** — DONE
        (`command_refs` + `eval_arg_to_key`): binds the sub-task's command refs to
        the evaluated argument keys, so the stock "take from others lazy" →
        "take from others" re-dispatch resolves `%object2%` (the container) and
        prints "from the table".  `MoveObject ToSameLocationAs` (drop) added.
      - **General-reference disambiguation** — DONE
        (`a5run.cpp resolve_refine`/`scan_tasks`/`run_remembered` +
        `possible_keys`/`amb_word`/`build_amb_prompt`).  Ports
        clsUserSession.RefineMatchingPossibilitesUsingRestrictions + the
        GetGeneralTask ambiguity check + DisplayAmbiguityQuestion /
        ResolveAmbiguity / PossibleKeys:
        - a reference now gathers **all** in-scope candidates (visible-first),
          which the task's own restrictions then refine (a candidate that fails
          the restrictions is dropped — so "take key" with one key already held
          resolves to the other **without** a prompt);
        - a still-multiple reference raises `Which <word>?  The a, the b or
          the c.` (AmbWord = the common typed noun; the definite-name list joined
          ", "/" or " through ToProper, with pSpace's two spaces) and remembers
          the task;
        - the next turn is tried as a **clarifier** first (PossibleKeys narrows
          the remembered candidates — "brass" → the brass key), then, failing
          that, as a fresh command (so "look" interrupts a pending prompt), and
          re-prompts if neither resolves.
        Validated by `test/a5_disambig_test.cpp` — a **self-contained synthetic
        v5 adventure** (built as in-memory XML, run through the real
        a5xml/a5model/a5run pipeline; no game data) covering the prompt wording,
        clarifier resolution, fresh-command interrupt, and restriction-based
        refinement — wired into `make -f Makefile.headless test` as `a5distest`.
        **ASan/UBSan-clean.**  NOTE: in Six Silver Bullets the natural ambiguity
        rooms (The Bar's two signs, The Hamlet's five houses) are shadowed under
        the deterministic seed by a `Roller==1` catch-all task (`*`) — faithful,
        since the official Runner would also fire it whenever its RAND lands on 1
        (the prompt is confirmed reached: `ExamineObjects` returns the ambiguity
        before the catch-all runs).  Routing v5 RAND through the shared
        `erkyrath_random` (open item #5) would deconflict it.
      - **Validated**: `test/a5_bullets_script.txt` → `test/a5_bullets_expected.txt`
        golden transcript (wake/look/examine table+note/take/inventory/drop),
        wired into `make -f Makefile.headless test` as `a5runtest` (skips if the
        game file is absent — it can't be checked in).  Movement + intros for
        Anno 1700 and Stone of Wisdom remain clean (no raw `%…%`/`.Prop` tokens).
        **ASan/UBSan-clean** on all three games.
      - **v5 RAND through the shared `erkyrath_random`** (#5) — DONE.  New
        `a5rand.cpp/.h` (`a5rand_seed`/`a5rand_between`) wraps
        `terps/common_utils/randomness.c` (the same seedable xoshiro128**
        generator the v4 path uses via scutils.cpp); `a5run_new` seeds 1234 per
        game so headless runs are reproducible, and `eval_num_value` parses
        `RAND (min, max)` to an inclusive draw (frankendrift
        `Global.Random(iMin, iMax)`).  `Makefile.headless` compiles
        `randomness.c` as `test/a5_randomness.o` (`-I../cheapglk` for glui32)
        and links it into the a5run / a5distest harnesses.  This deconflicts the
        Six Silver Bullets `Roller==1` catch-all (it now fires only when RAND
        lands on 1) and makes the intro's RAND-selected dream/epigraph variants
        real; the golden transcript was regenerated (dream variant 4 under seed
        1234).  **ASan/UBSan-clean.**  NOTE: when the v5 path is wired into
        Spatterlight, seeding should honour the determinism / native-seed toggle
        as scutils.cpp does, and `a5rand.cpp` + `randomness.c` must be added to
        the Xcode target.
      - **Events / timers** — DONE.  A faithful port of `clsEvent`'s turn-based
        state machine into `a5run.cpp` (model parsing in `a5model.cpp`):
        - Model: `a5_event_t` now carries the parsed `WhenStart`/`Length`/
          `StartDelay`/`Repeating`/`RepeatCountdown`, the `<SubEvent>` list
          (`a5_subevent_t`: when {FromStart/FromLast/BeforeEnd} + turn-offset
          range + what {ExecuteTask/UnsetTask/DisplayMessage/SetLook} + key/
          description) and the `<Control>` list (`a5_eventctrl_t`: control +
          Completion flag + task key).
        - Runtime (`a5_event_rt`): a per-event `clsEvent` instance — Status,
          TimerToEndOfEvent, iLastSubEventTime/LastSubEvent, bJustStarted,
          NextCommand, the Immediately→BetweenXY WhenStart flip, and resolved
          random `Length`/sub-event offsets (via `a5rand_between`; a `from==to`
          range draws no RNG).  Ported `IncrementTimer` / `DoAnySubEvents` /
          `RunSubEvent` / `lStart` / `lStop` / the `TimerToEndOfEvent` setter and
          the deferred-vs-immediate (`bEventsRunning`) Start/Stop on control
          triggers.
        - Driver: `ev_init` (game start, after the intro — Immediately events
          start) and `ev_tick_all` (clsUserSession.TurnBasedStuff) run once per
          *successful, non-system* player turn; `ev_on_task_completed` fires the
          EventControls when a player **or** event-fired task completes.
        - **Key fix surfaced:** the stock list-runner tasks (e.g. the bomb
          cascade `Bomb2List` → `SetTasks Execute Bomb2BoomG1…`) rely on each
          candidate's **restrictions** to pick which fires.  `SetTasks Execute`
          was running them unconditionally (every bomb message every turn, then
          insta-death); it now mirrors `AttemptToExecuteTask` — gate on
          Completed/Repeatable + restriction-check (with any just-bound refs) +
          fire controls.  `attempt_event_task` (event-fired ExecuteTask) does the
          same, restriction-checked and silent on failure.
        - **Validated on Six Silver Bullets:** Roller re-rolls each turn (no
          spam), bombs stay gated, and `TheVisitor2` fires the "ENCOUNTER: THE
          VISITOR" knock exactly 5 turns after waking, its `PurpleRese`
          sub-event moving the purple agent out of the hall (so the peephole's
          `ThePurpleA Must BeAtLocation Location1`-gated line correctly
          disappears).  Golden transcript regenerated; deterministic; a 40-turn
          soak is stable.  Anno 1700 + Stone of Wisdom still clean.  **ASan/
          UBSan-clean** on all.
      - **Integer arithmetic expression evaluation** — DONE.  New
        `a5arith.cpp/.h` (`a5_eval_arith`): a recursive-descent integer
        evaluator (`+ - * / mod ^`, unary minus, parentheses, right-associative
        power; div/mod-by-zero → 0 per frankendrift's `SafeInt`) that ports the
        arithmetic core of `clsVariable.SetToExpression`'s token reducer.
        `eval_num_value` now runs it on the post-`a5text_process` string and only
        falls back to `strtol` when the result is not well-formed arithmetic — so
        a variable assignment like Six Silver Bullets'
        `Sneakscore = "%roller%+%sneakbonus%"` evaluates to the **sum** instead
        of `strtol`-truncating to `%roller%`.  Self-contained regression
        `test/a5arith_test.cpp` (`make -f Makefile.headless a5arithtest`, in
        `make test`) covers precedence/associativity/mod/power/parens/unary-minus
        + non-arithmetic rejection.  Golden transcript unchanged; **ASan/UBSan-
        clean**.  NOTE: the full `SetToExpression` **function library**
        (`min`/`max`/`if`/`either`/`abs`/`upr`/`val`/`str`/`oneof`/comparison +
        logic ops …) is **not** ported — no shipped Six Silver Bullets / Anno
        1700 / Stone of Wisdom assignment uses it; add on demand.
      - **Still TODO**: characters/walks/conversation/topics; full UDF
        (`%FunctionName[args]%`) + array variables + the
        `SetToExpression` function library (see the arithmetic note above);
        scoring display (`%swissaccount%`-style score variables render, but no
        ADRIFT Score/MaxScore status); "seen"/visibility
        (`HaveBeenSeen*`/`BeVisibleTo*` still approximated);
        DisplayMessage/SetLook sub-events (no-op/best-effort — unused by Six
        Silver Bullets); and the ground-truth diff vs the official Runner /
        frankendrift (no VB.NET build set up yet).
- [ ] **P5 Resources & save**: blorb media via Glk; v5 XML save/restore; finish a
      full deterministic walkthrough of Six Silver Bullets.

---

## 10. Open questions / risks

1. **XML dependency**: Scarier has no XML parser. Write a tiny one (the input is
   regular .NET-`XmlTextWriter` output) vs. vendoring a small library. Lean
   toward a ~few-hundred-line bespoke pull parser to stay self-contained.
2. **Command-pattern regex**: translating ADRIFT patterns + general references to
   a C matcher is the riskiest single piece; prototype early in P3.
3. **Obfuscation toggle**: the "deobfuscate unless metadata lacks
   `compilerversion`" heuristic is fragile — validate across a few v5 files, not
   just Six Silver Bullets.
4. **Scale**: realistically a multi-week effort. Property semantics (P1) and the
   task/restriction engine (P3) are where correctness is won or lost — budget
   accordingly and lean on frankendrift's enums verbatim.
5. **Determinism**: route any v5 randomness through the shared `erkyrath_random`
   (seed 1234) like the other Scarier engines, for reproducible walkthroughs.
   **DONE** — see `a5rand.cpp/.h` + the P4 note above.
6. **STL boundary**: borrowed Bocfel code pulls in `<map>`/exceptions; keep it at
   the container edge so the bulk of the engine stays "C-like".
```
