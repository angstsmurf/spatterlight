# TODO — ADRIFT 5 support in Scarier

Add support for **ADRIFT 5.0** games to the Scarier interpreter, validated against
the IFComp 2018 entry **Six Silver Bullets**:

```
terps/scarier/test/adrift5-games/Six Silver Bullets (v 1.0).blorb
(== bullets.blorb, 376 934 bytes)
```

> **Game files**: the ADRIFT 5 test corpus lives in the untracked (gitignored)
> folder `terps/scarier/test/adrift5-games/` — the harnesses default to it via
> `A5_GAMES_DIR`.  The blorbs are copyrighted and **not committed**; populate the
> folder locally.  Six Silver Bullets, Anno 1700 and Stone of Wisdom are the
> ground-truth set; other v5 games (FBA, Bug Hunt On Menelaus, Grandpa, Lost
> Coastlines, RtC, TBN, TEE, XXR, MI, Oct 31st, Halloween) are there for wider
> soak testing.

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
  - **Ground-truth status (all three shipped games now byte-match
    FrankenDrift):** with the FrankenDrift.Headless reference actually built and
    run, **Anno 1700 and Stone of Wisdom diff to ZERO** hunks (intro→start
    room→moves), and **Six Silver Bullets** diffs only on the five RAND-selected
    intro lines (epigraph / dream / training-block variants — the .NET
    `System.Random` vs xoshiro caveat).  The five divergences the first diff
    surfaced are all fixed:
    1. **Take "(from X)" line** — FIXED.  The stock `TakeObjectsFromOthersLazy`
       completion message `(from %objects%.Parent.Name)` must print *before* its
       `SetTasks Execute` sub-task ("You take … from the Table.") so the parent
       is still the source ("the Table"), not the player.  Root cause: v5's
       `FileIO.Load` defaults a missing `<MessageBeforeOrAfter>` to **Before**
       (FileIO.vb:1618 overrides the class default of After); `run_task` was
       defaulting to After.  Fixed by `before = !streq(when, "After")`.  Emitting
       the message pre-action also evaluates it pre-action, matching FrankenDrift's
       position-locked AddResponse assembly for the common case.  `emit_completion`
       now trims trailing whitespace/newlines from each rendered completion message
       (FrankenDrift trims each response), removing the spurious blank line a
       Before-message's trailing `\n` left before the sub-task output.
    2. **`%ListObjectsOnAndIn%`** — FIXED (and the original note was wrong about
       the bare `%ListObjectsOn/In%`).  Per Global.vb:2199+, **`ListObjectsOn` and
       `ListObjectsIn` are bare indefinite lists** (`Children(On|Inside).List(, ,
       Indefinite)`) — Scarier's `list_objects` already matches; only
       **`ListObjectsOnAndIn`** wraps via `clsObject.DisplayObjectChildren`, and
       it **ignores its argument**, looping every object with on/in children
       (joined by pSpace's two spaces).  The wrap is `ToProper(<on-list,
       indefinite>) + " is/are on " + <FullName definite> [+ ", and inside is/are
       <in-list>"] + "."` where **`ToProper` upper-cases the first char and
       lower-cases the rest** (Global.ToProper bForceRestLower=True), and the
       inside list is suppressed for a closed openable object.  So examine-table:
       Scarier now renders `The yellow note, the silver gun and the silver bullets
       are on the Table.` (was `The Yellow Note, The Silver Gun and The Silver
       Bullets`) — the objects' own article happens to be "The", lower-cased by
       ToProper except the leading one.  New `to_proper`/`display_object_children`
       helpers in `a5text.cpp`.
    3. **`<cls>` screen-clear** — FIXED.  Anno 1700's `<Introduction>` ends with
       `<cls>` (after `<waitkey>`/credits), which FrankenDrift.Headless handles by
       clearing the buffer (`sb.Clear()`), so its entire credits/preamble is wiped
       and the intro shows *nothing* post-clear.  `a5text_render_plain` now treats
       `<cls>` as a buffer reset (mirroring GlkHtmlWin), making Anno 1700's start
       byte-match FrankenDrift.  `<waitkey>` and other tags still drop.
    4. **Location-view leading blank** — FIXED.  A v5 location view is prefixed
       with a blank line (clsLocation.ViewLocation: `If Adventure.dVersion >= 5
       Then sView = vbCrLf & sView`), so after a `> look` echo the room name has
       a paragraph break.  `a5text_view_location` now emits a leading `\n`.
    5. **`<DisplayOnce>` description segments** — FIXED.  Stone of Wisdom's start
       room has an `AppendToPreviousDescription` segment with `<DisplayOnce>1`
       (the "soft rain" line): shown at game start, suppressed on a later LOOK.
       `a5state` now tracks displayed segment nodes (`disp_once`/`marking_display`)
       and `a5text_eval_description` skips a seen DisplayOnce segment and retires
       it only on real output (the location-view render sets `marking_display`,
       mirroring clsDescription.ToString's Displayed flag gated on
       `bTestingOutput`).  ReturnToDefault and non-location DisplayOnce uses are
       still best-effort.
  - **Earlier note — Start-room auto-LOOK** — FIXED.  Scarier rendered the start location
       ("Dead or Dreaming?") during the intro; FrankenDrift gates that on
       `Adventure.ShowFirstRoom` (XML `<ShowFirstLocation>`, default true), which
       Six Silver Bullets clears.  Added `a5_adventure_t.show_first_location`
       (parsed in `a5model.cpp`, "0" => false) and gated the post-intro
       `a5text_view_location` in `a5run_intro` on it.  Verified: Six Silver
       Bullets no longer shows the room at start (matches FrankenDrift), while
       Anno 1700 and Stone of Wisdom (`<ShowFirstLocation>1`) still display their
       start rooms.  Golden transcript regenerated; **ASan/UBSan-clean**.
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
      events/timers + conversation + walks + DisplayMessage/SetLook sub-events
      DONE; scoring display + full UDF/expression library remain.**
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
      - **Ground-truth diff vs FrankenDrift — DONE & GREEN.**  The
        FrankenDrift.Headless reference is built (`dotnet build` under
        `~/frankendrift`) and `test/a5_groundtruth.sh` drives it; **Anno 1700 and
        Stone of Wisdom now diff to zero** and **Six Silver Bullets** only on the
        RAND-selected intro variants.  Five real divergences found and fixed —
        Before-default completion ordering, `ListObjectsOnAndIn`/
        `DisplayObjectChildren` wording, `<cls>` screen-clear, v5 location-view
        leading blank, and `<DisplayOnce>` segments (see the §8 list).  All three
        games **ASan/UBSan-clean**; golden transcript regenerated.
      - **Anno 1700 walkthrough ground-truth pass (8 fixes; diff 733 → 362
        hunks).**  Denk's full Anno 1700 walkthrough (`~/Desktop/anno1700.txt`,
        242 commands) was run through `test/a5_groundtruth.sh`; it surfaced and
        fixed eight real engine bugs, none game-specific:
        1. **Character on/in-object state + restriction** — `%Player% Must
           BeOnObject AnyObject` was unhandled in pass_character (fell through to
           `return 1`), so the stock `GetOffBeforeMoving` task fired every move
           and stranded the player.  Added `char_onobj`/`char_in` tracking +
           the BeOnObject/BeInsideObject/Be{Standing,Sitting,Lying}OnObject
           character ops, and honour `<Continue>ContinueAlways`.
        2. **DisplayOnce terminates a description** — clsDescription.ToString
           does `Return sb.ToString` inside `If .DisplayOnce`, so a not-yet-shown
           DisplayOnce segment ends the description (the "long first-visit /
           short thereafter" location pattern).  a5text now breaks after it.
        3. **`%PropertyValue[entity,prop]%`** text function (Text property values
           are rich `<Description>` blocks → `value_node`, rendered).
        4. **`%direction%` must include up/down/in/out** — the regex emitted only
           the 8 compass points; frankendrift's `North To NorthWest` loop spans
           all 12 by enum *value* (Up=4…Out=7).  Bare "u"/"up"/"in"/"out" now
           match movement (Anno's upstairs).
        5. **A failing Specific override claims the turn** — a Specific task is a
           first-class higher-priority task; on restriction failure it shows its
           fail message and suppresses the parent (no-output failures still fall
           through).  Fixed Anno's `MovingFrom1` stairs gate.
        6. **`*` wildcard regex corruption** — the bare-`*`→`.*?` pass rewrote
           the `*` inside an already-inserted `.*?`; use a placeholder like
           frankendrift's `[[#]]` (we use a `\x01` sentinel).  Fixed the
           take/search `*[string/twig]`-style commands.
        All eight are committed (one per fix), Six Silver Bullets golden +
        a5parse/a5arith/a5distest unchanged, ASan/UBSan-clean over the whole
        walkthrough.
      - **`<# IF(...) #>` expression engine + `%X.Location.Exits.Count/.List`**
        — **DONE.**  New `a5sexpr.cpp/.h` (`a5_eval_sexpr`): a string-capable
        recursive-descent port of `clsVariable.SetToExpression`'s token reducer —
        numbers / "quoted strings" / bare-identifier-as-string; arithmetic
        `+ - * / mod ^` with ADRIFT's precedence (`* /` tighter than `+ - mod ^`,
        all left-assoc; `+` is numeric-add when both sides are numeric else
        string concat; `/` rounds away from zero and follows IEEE so `0/0`→NaN,
        `x/0`→∞ — formatted as .NET does, "NaN"/"∞"); comparisons
        `= == <> != > < >= <=` (numeric or string → "1"/"0"); logic `AND OR`
        (`&& ||`); and the deterministic function set `if/min/max/abs/upr/lwr/
        ppr/len/val/str/mid/replace/lft/rgt/ist` (`either/oneof/rand/urand` are
        best-effort, no shipped expression uses them).  Wired into `a5text`
        `process_inner` as `replace_expressions` (`<#…#>`) after `a5expr_replace`,
        mirroring frankendrift's ReplaceFunctions→ReplaceExpressions→ALR order:
        `process_inner` now **protects** each `<#…#>` behind a `\x01N\x01`
        sentinel (like frankendrift's GUID swap) so the `%function%`/OO passes
        leave the body untouched, restores them, then `replace_expressions`
        evaluates each via `expr_substitute` + `a5_eval_sexpr`.  `expr_substitute`
        ports the **quoted** OO/reference substitution (frankendrift
        `bExpression=True`, Global.vb:639-647): a substituted value is wrapped in
        `"…"` unless it is a bare integer or already lies between quotes, so the
        evaluator sees a self-contained expression (e.g. `Exits.List`'s
        "north and south" is quoted, `Exits.Count`'s "2" is not).  Added **`.Exits`
        to a5expr** (a `Ctx.is_dirs` direction-list path): a location's Exits is
        the DirectionsEnum-order list of Movement directions with a non-empty
        Destination (no restriction filter, matching `arlDirections(d).LocationKey
        <> ""`), a character's via `a5restr_exit_in_direction` (restriction-
        checked, like `HasRouteInDirection`); `.Count`/`.List`/`.Name`/bare-`` ``
        terminate it (`.List` lowercases canonical names, ", "/" and "/" or "
        join).  **Validated vs FrankenDrift ground truth:** the no-route messages
        in **Anno 1700** ("There is no route to the west, only south and up."; bare
        "up"/"down"/"in"/"out" correctly drop the "to the " via the up/down/in/out
        IF) and **Six Silver Bullets** ("There is no route to the west." when
        `Exits.Count=0`) now byte-match; the Six Silver Bullets `score` line
        matches FrankenDrift's `(NaN%)` (the game divides by a missing
        `%maxscore%`); **StoneOfWisdom and the Six Silver Bullets golden
        transcript are unchanged**, and the Anno walkthrough diff has **zero**
        remaining `<#…#>`/Exits hunks.  Self-contained regression: the evaluator
        is exercised directly; **ASan/UBSan-clean** on all three games.  Added
        `a5sexpr.cpp` to `Makefile.headless` (A5_TEXT_SRC/A5_RUN_SRC); when the v5
        path is wired into Spatterlight it must be added to the Xcode target
        alongside the other `a5*.cpp`.  NOTE: the bare Adventure-level
        `%score%`/`%maxscore%`
        specials (outside an expression, when the game has no Score/MaxScore
        *variable*) are still a separate TODO (scoring display); they are not
        needed by the expression engine.
      - **NPC in-room presence + conversation** — **DONE.**  Three pieces, all
        validated against FrankenDrift ground truth (Anno 1700 full-walkthrough
        diff dropped 362 → 187 hunks, with **zero** remaining presence/
        conversation hunks; the Six Silver Bullets golden + Stone of Wisdom stay
        clean; ASan/UBSan-clean across the whole 15-game v5 corpus):
        1. **In-room presence** (`a5text_view_location`, ported from
           clsLocation.ViewLocation's character loop): after the room
           description + listed objects, each visible NPC contributes its
           `CharHereDesc` property (rendered with the character as the text
           context — new `a5_state_t.ctx_char` — so a bare `%CharacterName%`
           resolves to it, and retiring its `<DisplayOnce>` segments), else the
           default "<Name> is here.".  Identical descriptions from several
           characters merge via the `##CHARNAME##` de-named form (" is "→" are ",
           names listed); a single character emits its description **verbatim**
           (preserving the authored capital, matching FrankenDrift, whose
           round-trip is a no-op for the singleton case).  So entering Anno's
           Office shows "A young woman stands leaning over the big desk. She
           greets you with a big smile as you enter." (first visit) then "a young
           woman is here." (the descriptor name — Susan is not yet Known).
        2. **Conversation engine** (`a5run.cpp` `exec_conversation` /
           `find_conv_node`, ports of clsUserSession.ExecuteConversation /
           FindConversationNode; topics parsed into `a5_topic_t` in `a5model`):
           a `<Conversation>` action (GREET/ASK/TELL/SAY → Greet/Ask/Tell/Command
           + ENTERWITH/LEAVEWITH) drives implicit/explicit intro on first
           contact, then the matching topic's reply + actions, else the default
           "%CharacterName% doesn't appear to understand you." / "ignores you."
           (or a topic restriction's fail text).  Ask/Tell match comma-keywords
           (ContainsWord, most-matches-then-%); Command matches the subject via
           the existing `a5parse_match_command`; Greet/Farewell match on
           restrictions only.  Conversation state lives in
           `a5_state_t.conv_char`/`conv_node`; `%ConvCharacter%`/`%AloneWithChar%`
           text specials and the `BeInConversationWith`/`BeAloneWith`/`BeAlone`/
           `HaveSeenCharacter` restriction ops were added (`a5restr`), with a
           per-turn player-"seen" set (`char_seen`, updated like
           PrepareForNextTurn).
        3. **TaskExecution = HighestPriorityTask precedence** (the v5 default,
           the real reason Anno's `say hello` → "I'm not sure who you are
           referring to."): the first command-matching task that fails its
           restrictions **with output** now claims the turn (shows its fail
           message, no lower task runs) — but **only for tasks that bear
           references**, mirroring clsUserSession's
           `htblCompleteableGeneralTasks` pre-filter (a refless task is kept only
           if `HasReferences OrElse IsCompleteable`, so Six Silver Bullets'
           refless "LOOK" peephole task correctly falls through to the real Look
           instead of stranding the player on "You simply cannot do that.").
        - Also faithful now: a character's **proper name is unusable until
           Known** (clsCharacter.sRegularExpressionString — the game defines a
           Known character property), so `say hello to susan` does **not** resolve
           "susan" while she is unintroduced and falls through to the bare `Say`
           task's message, exactly as FrankenDrift does; and
           `character_display_name` renders the descriptor ("a young woman") vs
           the proper name ("Susan") off the same Known-selected test.
        - NOTE: no new source files — all changes are in the existing `a5model`/
           `a5state`/`a5restr`/`a5text`/`a5run`, so the Makefiles and the Xcode
           target need no new entries.
      - **Character walks + DisplayMessage/SetLook sub-events** — **DONE.**  A
        faithful port of `clsWalk` and the previously-stubbed event
        `RunSubEvent` DisplayMessage/SetLook cases, all in the existing files
        (no new sources, so no Makefile/Xcode entries):
        1. **Walk model** (`a5model`): each `<Character>`'s `<Walk>` children are
           parsed into `a5_walk_t` — `<Step>` route legs (`a5_walkstep_t`:
           destination key + turn duration; the in-situ DOM Step text is
           truncated at the space so the key aliases cleanly), `<Control>`
           triggers (the shared `EventOrWalkControl` parse, now factored into
           `a5_parse_control_text`), and `<Activity>` sub-walks (`a5_subwalk_t`:
           when {FromStart/FromLast/BeforeEnd/**ComesAcross**} + what
           {DisplayMessage/ExecuteTask/UnsetTask} + the `OnlyApplyAt` display
           gate).
        2. **Walk runtime** (`a5run.cpp` `a5_walk_rt`, one slot per character
           walk, flattened): a per-walk `clsWalk` instance — `wk_lstart`/
           `wk_lstop`/`wk_increment`/`wk_set_timer` (the Status/Timer machine,
           re-rolling step durations each start but, like `clsWalk`, **not**
           resetting LastSubWalk/the sub-walk offsets), `wk_do_steps` (the
           location / random-adjacent-group-member / follow-character move
           dispatch + `Move`), and `wk_do_subwalks`.  Driven by `wk_tick_all`
           (run **before** the events in `ev_tick_all`, mirroring TurnBasedStuff),
           `wk_init` (StartActive walks at game start), and `wk_on_task_completed`
           (the WalkControls, fired before the EventControls).  The
           **ShowEnterExit** narration (`wk_show_enter_exit` + new
           `a5text_char_proper_name`, `loc_is_adjacent`/`loc_direction_to` ports
           of clsLocation.IsAdjacent/DirectionTo over the DirectionsEnum order)
           prints `<Name> enters/exits [to/from <dir>]` when a `ShowEnterExit`
           NPC crosses the player's room boundary.
        3. **Event sub-events**: the event `SubEvent` parse now honours the
           `<What>` override and the `<OnlyApplyAt>` gate (`se->key`);
           **DisplayMessage** is gated on `key <> "" AndAlso
           Player.IsInGroupOrLocation(key)` (new `a5state_in_group_or_location`),
           and **SetLook** pushes a (gate, rendered-text) entry onto a per-state
           look stack (`a5state_push_look`) that `a5text_view_location` consults
           (`a5state_player_look` / clsEvent.LookText), appended after the listed
           objects.  Walk DisplayMessage sub-walks use the same gate.
        - **Validated**: a new self-contained synthetic regression
          `test/a5_walk_test.cpp` (`make -f Makefile.headless test` as
          `a5walktest`) — Adventure A drives a StartActive looping two-room
          patrol and asserts the exit/enter ShowEnterExit prose (correct
          DirectionTo), the ComesAcross→DisplayMessage nod (OnlyApplyAt-gated),
          and the loop restart; Adventure B asserts a SetLook line riding the
          room view and a DisplayMessage shown only when its OnlyApplyAt gate
          matches the player's room (the Room1 bell shows, the Room2 horn does
          not).  **Ground truth**: the Anno 1700 walkthrough diff is
          **byte-identical** to the pre-change baseline (187 hunks — Susan's
          stock "Follow Player" walk does not cross the player boundary visibly
          in this transcript, matching FrankenDrift; the "walks by you" line is
          the RNG-selected guest *event*, not a walk).  Six Silver Bullets golden
          (its six StartActive=0, output-less patrol walks) + Stone of Wisdom
          stay clean; **ASan/UBSan-clean** across the whole 15-game corpus.
      - **Unknown-word message wording + `wait`/`z` + clean walkthrough script**
        — **DONE.**  Three changes, validated against FrankenDrift ground truth:
        1. **The `clsUserSession.NotUnderstood` ladder** replaces the single
           catch-all "I don't understand.".  A lazily-built known-words set
           (`a5_run_t.known_words`, seeded exactly as NotUnderstood does: every
           General task command word with `[]{}/` blanked, each object's
           article/prefix/names, each character's article/prefix/descriptors/
           proper name, the direction words via the new
           `a5parse_directions_re`, and `"and"`) drives the word-validity check:
           the first input word the game doesn't recognise stops the turn with
           `I did not understand the word "X".`; otherwise the fall-through is the
           adventure's catch-all `Sorry, I didn't understand that command.`
           (FileIO.vb:850 default).  Every former "I don't understand." was
           already a hunk (FrankenDrift never emits that literal), so this is
           strictly non-regressing.
        2. **`wait`/`z` system command** (`system_command`, a minimal
           `clsUserSession.SystemTasks`, tried after task-match fails but before
           NotUnderstood): prints `Time passes...` and advances `WaitTurns`
           (new `<WaitTurns>` model field, default 3) turns of `ev_tick_all`
           (TurnBasedStuff).
        3. **Cleaned walkthrough script** `test/anno1700_walkthrough.txt` — the
           community Anno 1700 walkthrough rewritten as a bare-command
           ground-truth script: the prose header and every inline `(...)`
           play-by-play note stripped (both harnesses skip blank / `#`-prefixed
           lines), so each line actually executes instead of being rejected
           wholesale.  This is the fix for the old "walkthrough comment lines
           parsed as commands" divergence — the game now genuinely plays through
           (the diff compares real gameplay, 187 → 142 hunks).  **NOTE:** the
           post-"drink wine" kidnapping/storage-room wake event does not fire
           deterministically under the fixed seed (the original author was unsure
           of its trigger too), so the waits and following commands are
           best-effort.  **ASan/UBSan-clean; `make -f Makefile.headless test`
           green** (Six Silver Bullets golden + a5parse/a5arith/a5distest/walk
           unchanged).
      - **Reference-resolution visibility messages + object "seen" tracking** —
        **DONE (the bulk; Anno diff 142 → 46 hunks, ~10 of those now the
        unfixable RNG guest-event lines).**  Six engine fixes, each validated
        against FrankenDrift ground truth, Six Silver Bullets golden + Stone of
        Wisdom unchanged, ASan/UBSan-clean on all three:
        1. **`%TheObject%` definite article for prefixed names** — the stock
           "not visible" fail text `%CharacterName% can't see %TheObject[%object%]%.`
           rendered no article because a bare inner `%object%` produces the full
           "prefix + noun" display name and the handler's own name search only
           matched `names[]`.  Now uses `resolve_object_arg` (key / name /
           "prefix + name"), so the article lands ("...the framed newspaper
           article.").
        2. **`You can't see any <plural>!`** — a typed object noun naming several
           objects, none in scope, now yields this canned message
           (clsUserSession.DisplayAmbiguityQuestion `bCanSeeAny=False` on
           `MatchingPossibilities.Count>1`) via a new `RR_CANTSEE` resolve
           outcome + ported `clsObject.GuessPluralFromNoun` (irregulars + the
           quirky "feet"→"feets").
        3. **Player object "seen" tracking** (`a5_state_t.obj_seen`, marked each
           turn for every object visible in the room) drives
           `HaveBeenSeenByCharacter %Player%`, so the examine chain distinguishes
           "never encountered" (`You see no such thing.`) from "seen but not here
           now" (`You can't see the X.`).
        4. **Seen-then-any scope tier** — `resolve_object_candidates` prefers
           visible, else seen, else any, and `RR_CANTSEE` fires only on the
           never-seen bag (a lone seen object resolves to itself).
        5. **Mass-noun plural** — `IsPlural == (Article == "some")`, so
           "some firewood" stays "You can't see any firewood!" (not "firewoods").
        6. **NotUnderstood seen-object branch** — an unmatched command naming a
           seen+visible object answers "I don't understand what you want to do
           with the <object>.".
      - **Deferred `RR_CANTSEE` so a character task can override it** — **DONE.**
        A noun like "woman" matches BOTH an object reference (`ExamineObjects`
        `%object%` → the out-of-scope `Woman`/`Woman1` objects, yielding
        `RR_CANTSEE`) AND a lower-priority character reference (`ExamineCharacter`
        `%character%` → the seen-but-absent NPC Susan, whose
        `BeInSameLocationAsCharacter` restriction fails *with* an authored
        message).  `RR_CANTSEE` was emitting "You can't see any women!" and
        claiming the turn immediately, shadowing the character task; FrankenDrift
        instead records the can't-see as a pending ambiguity (`sAmbTask`) that is
        shown only if no later task claims `sTaskKey` (GetGeneralTask /
        DisplayAmbiguityQuestion).  Fix: `scan_tasks` now treats `RR_CANTSEE`
        exactly like `RR_AMBIG` — record the first one (`amb_cantsee` flag) and
        keep scanning, so a later uniquely-resolving (`RR_OK`) or failing-with-
        output (`RR_FAIL`) task overrides it; only when nothing else claims does
        `a5run_input` emit the deferred "You can't see any <plural>!" (the
        emission is factored into `emit_cantsee`).  So `x woman` (Susan seen,
        elsewhere) now byte-matches FrankenDrift's authored
        "You can't see a young woman!" while the genuine object-only out-of-scope
        cases (60 in the Anno walkthrough) still emit "You can't see any …!".
        Anno diff 46 → 37 hunks; Six Silver Bullets golden + a5distest unchanged;
        ASan/UBSan-clean across the whole 15-game corpus.
      - **TaskExecution mode drives claim-on-fail (not `n_refs`)** — **DONE.**
        The earlier "a failing-with-output task claims the turn only if it bears
        references" gate was an approximation; the real distinguisher is
        `Adventure.TaskExecution` (parsed into `a5_adventure_t.hp_passing`).
        `HighestPriorityTask` (the clsAdventure default, element absent —
        Anno/Stone): the highest-priority command-matching task that fails its
        restrictions *with output* claims the turn, refless and reference-bearing
        alike (`htblCompleteableGeneralTasks` has no `HasReferences`/
        `IsCompleteable` filter — that lives only in the background UI predictor).
        `HighestPriorityPassingTask` (Six Silver Bullets): it does NOT claim;
        the first/highest-priority fail message is recorded and the scan keeps
        looking for a *passing* task, which overrides it (so SSB's refless `LOOK`
        peephole, priority 49000, falls through to the real Look at 50031 — the
        case the old gate papered over).  A recorded fail is non-empty output, so
        emitting it advances the turn (`sOutputText <> "" => TurnBasedStuff`).
        Fixed Anno's "light lantern" => "Light what?" (refless `LightBrass1`).
      - **Property restriction implemented** — **DONE.**  Property-type
        restrictions were stubbed to always-pass; ported
        clsUserSession.PassSingleRestriction's Property case
        (`a5restr.cpp pass_property`).  The spec is
        `<propKey> <itemRef> Must|MustNot <Op> <value>` — an extra item token the
        one-key `parse_spec` layout can't hold, so the raw spec is re-tokenised
        (new `a5_restr_t.raw`).  An item lacking the property fails; `EqualTo`
        compares the override-aware value as a string (StaticOrDynamic /
        OpenStatus / state lists / integer flags / object-key props); the numeric
        inequalities evaluate only for a plain-integer value — an expression value
        (`%Player%`-relative weight/bulk sums) still passes (the lenient default).
        Fixed Anno's "drop trunk" => "You can't drop the old crooked tree!" (the
        static tree now fails DropObjects restriction 2 `StaticOrDynamic ... Must
        EqualTo Dynamic` — and since the AND chain short-circuits on the first
        failure, restriction 2's message wins over restriction 5's "not
        carrying").
      - **Fail message respects the BracketSequence (sRestrictionText port)** —
        **DONE.**  `a5restr_fail_message` used to return the *first document-order*
        restriction that fails its single check, ignoring the `<BracketSequence>`
        boolean structure entirely — so a `Must BeExactText All` with no message
        (failing first under an AND that is itself inside an OR alternative) made
        the whole task emit nothing and fall through to NotUnderstood.
        FrankenDrift instead leaves the message in `sRestrictionText` as a *side
        effect* of `PassSingleRestriction` (cleared on pass, set on fail) while
        `EvaluateRestrictionBlock` walks the bracket sequence with short-circuit,
        so the surfaced message is the failing single restriction on the *deciding
        path*.  Ported: `eval_block` now threads a `last_fail` index (cleared on a
        passing `#`, set on a failing `#`, untouched by short-circuited `#`s) plus
        a parallel `nodes[]` map; `a5restr_pass`/`a5restr_fail_message` share a new
        `eval_restrictions` core that returns the result and the deciding
        `<Restriction>`.  Fixed Anno's `get key` (held) =>
        `You are already carrying the corroded skeleton key!` — TakeObjects'
        sequence `R1..R4 A ((R5 BeExactText-All ...)O(R10 .. R11 already-carrying
        ..))A(..)` now correctly surfaces R11's message instead of R5's empty one
        (was falling through to "I don't understand what you want to do with the
        corroded skeleton key.").  Anno diff 58 → 54 hunks; Six Silver Bullets
        golden + a5parse/a5arith/a5distest/walk unchanged; Stone of Wisdom clean;
        ASan/UBSan-clean across the 15-game corpus.
      - **CompletionMessage `<DisplayOnce>` retirement** — **DONE.**
        `emit_completion` rendered completion messages without setting
        `marking_display`, so a multi-segment CompletionMessage whose first
        segment is `<DisplayOnce>1` showed the first-visit text *every* time
        instead of retiring it (clsDescription.ToString marks `Displayed` on real
        output, `bTestingOutput=False`).  Now renders under `marking_display` like
        `a5text_view_location`.  Fixed Anno's MovingFrom5: `open door`
        (porch→reception) says "...step into the hotel." only the first time, then
        "...step into the reception." thereafter, matching FrankenDrift.  Anno diff
        54 → 52 hunks; full headless suite green; Stone of Wisdom clean;
        ASan/UBSan-clean across the corpus.
      - **Remaining Anno divergences (~28 non-RNG, fine-grained; diminishing
        returns):** the bulk is now the **`You can't see any <plural>!` family**
        (lines like cannon/doors/pistols/threads/wine/trees), which all stem from
        one architectural difference: **Scarier pre-filters object-reference
        candidates by a visible→seen→any scope tier (`resolve_object_candidates`),
        collapsing the count; FrankenDrift keeps ALL name-matching objects (any
        scope — `InputMatchesObject` adds every regex-matching object) and lets
        the task's restrictions refine them**, then decides at the end
        (`DisplayAmbiguityQuestion`):
          - `MatchingPossibilities.Count > 1` after refinement AND **none
            currently VISIBLE** (`Player.CanSeeObject`; "seen" is irrelevant here)
            => "You can't see any <plural>!" (no prompt); if ≥1 visible => "Which
            <noun>?" prompt.
          - `Count == 1` => resolve to it; its restrictions yield "You can't see
            the X." / "You see no such thing." etc.
          - `Count == 0` (all refined away) => `sNoRefTask` runs the task with an
            unresolved ref, surfacing e.g. DrinkObject's `ReferencedObject Must
            Exist` "Sorry, I'm not sure which object you are trying to drink."
        So fixing this means **restructuring resolution so candidates are the full
        any-scope set + scope filtering comes from restrictions, not a pre-filter**
        — a deliberate change with real regression risk against `a5distest` and
        the ~60 currently-correct "can't see any" cases; parked.  Other residuals:
        object name-alias selection when several same-noun objects are seen
        ("the walls" vs "the wall"); a few stock "trying to <verb>" no-reference
        messages; and game-specific puzzle-task responses (parchment/hat/key-rack,
        "prime wick"=>"...do with Player").  The RNG-selected guest-event lines
        ("a guest walks by you and hangs their room key on the rack") are expected
        divergence (.NET `System.Random` vs xoshiro).
      - **Still TODO (earlier list)**: full
        UDF (`%FunctionName[args]%`) + array variables + the `SetToExpression`
        function library; scoring display (no ADRIFT Score/MaxScore status);
        "seen"/visibility (`HaveBeenSeen*`/`BeVisibleTo*` still approximated);
        `<DisplayOnce>` ReturnToDefault + non-location uses (best-effort).
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
