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
| `a5state.cpp` | v5 save/restore (XML game-state; Scarier's own `<SaveState>` — FrankenDrift-compatible `.tas` interop is TODO, see `TODO_a5_frankendrift_save_compat.md`) | frankendrift `FileIO.SaveState`/`LoadState`, `clsState` |

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
- [x] **P4 Dynamic**: **DONE.** Core verbs + property/text engine + disambiguation +
      events/timers + conversation + walks + DisplayMessage/SetLook sub-events +
      scoring display + UDF/expression library + localization + multiple-object
      references + full walkthrough-corpus conformance (see `TODO_a5_walkthrough_bugs.md`).
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
      - **Blocked-exit message (sRouteError) overrides the generic "no route"**
        — **DONE.**  A movement task's `Player Must HaveRouteInDirection
        %direction%` restriction carries a generic `There is no route to the
        %direction%, only %...%.` message.  But when the exit *exists* and is
        gated by its own `<Movement><Restrictions>`, FrankenDrift surfaces that
        exit-restriction's message instead (clsCharacter.HasRouteInDirection sets
        sErrorMessage = the exit's sRestrictionText; PassSingleRestriction:
        "If sRouteError <> sRestrictionText AndAlso sRouteError <> '' Then
        sRestrictionText = sRouteError").  Ported: `a5restr_exit_in_direction`
        gained an optional `blocked_msg` out-param (the blocked exit's deciding
        `<Message>` node); `pass_character`'s HaveRouteInDirection sets
        `a5_state_t.route_error` to it when the exit is present-but-blocked;
        `eval_restrictions` keeps `route_error` only when the deciding failing
        restriction *is* that HaveRouteInDirection; `a5restr_fail_message` then
        prefers it.  Fixed Anno's `e` from the reception before introducing
        yourself => "Maybe you should introduce yourself to your new employer
        before going around exploring things. There'll be plenty of time for that
        later." (was the generic "There is no route to the east, only ...").  Anno
        diff 52 → 46 hunks; full headless suite green; Stone of Wisdom clean;
        ASan/UBSan-clean across the corpus (incl. direction-spam soak).
      - **Reference resolution restructured to full-set + three-tier refine
        (the parked architectural item) — DONE.**  Three coupled changes in
        `a5run.cpp`, validated against FrankenDrift ground truth (Anno
        walkthrough diff **25 → 14 hunks**, the residual 12 being RNG guest-event
        lines + 2 fine-grained cases below; Six Silver Bullets golden + Stone of
        Wisdom byte-identical to HEAD; **ASan/UBSan-clean across the whole 15-game
        v5 corpus**; `a5distest` updated for the now-faithful both-held case):
        1. **Strict name matching (`name_match`)** — the candidate gatherers used
           a loose word-set test (`words_match`) that treated each word of a
           multi-word Name as an independently-matchable noun, so "key" matched
           "key rack" and "cannon"/"pistols"/"threads"/"wine" over-matched.
           `name_match` now mirrors `clsObject/clsCharacter.sRegularExpressionString`
           exactly: `^(article |the )?(prefix_i )?...(name1|name2|...)$` — optional
           article, each prefix word independently optional **in order**, then
           **exactly one whole Name** (a multi-word name must appear in full).
           The looser `words_match`/`object_words` is kept only for the clarifier
           narrowing (`possible_keys`) and `AmbWord`.
        2. **Full any-scope candidate set + Applicable/Visible/Seen refine**
           (port of `RefineMatchingPossibilitesUsingRestrictions`, vb:5734-5962):
           `resolve_object_candidates`/`resolve_character_candidates` now return
           the **full** name-match set (visible-first ordering is only a
           default-pick hint), and `resolve_refine` Pass 2 runs the three tiers
           with **reset-to-original-on-empty** per reference.  Post-refine count
           decides (GetGeneralTask / DisplayAmbiguityQuestion): **1** => run the
           task (its restrictions yield "You can't see the X." / "You see no such
           thing."); **>1 with ≥1 visible** => `RR_AMBIG` "Which <noun>?"; **>1
           none visible** => `RR_CANTSEE` "You can't see any <plural>!"
           (`emit_cantsee` now type-aware: characters use the bare AmbWord, no
           GuessPlural).  This is the cannon("you see no such thing") vs
           doors("can't see any doors!") post-refine count-1-vs-many split.
        3. **`RR_NOREF` (sNoRefTask)** — when an object/character reference names
           no entity at all, the task is no longer skipped (old `RR_NOMATCH`);
           it is remembered as the no-reference fallback and, if nothing else
           claims, `run_noref` re-binds the resolvable references and surfaces the
           task's unresolved-reference restriction message
           ("Sorry, I'm not sure which object you are trying to drink.").
           Precedence in `a5run_input` now matches GetGeneralTask:
           claim(RR_OK) > recorded fail-with-output > sNoRefTask > ambiguity
           display.
        Fixed Anno's pull/cannon/doors/pistols/key/wine/trees lines and the
        latent "Which key? … or the key rack" mis-prompt (`get key` while holding
        the skeleton key now says "You are already carrying …" because "key" no
        longer matches "key rack").
        Anno 1700 reached **full MATCH** (commit `7a1b9e4a`).
      - **Inventory rendering: worn-container holder key + `ListObjectsOnAndIn`
        argument** — **DONE.**  A Stone of Wisdom `inventory` (`%ListWorn%` /
        `%ListHeld%` template + `%ListObjectsOnAndIn[%Player%.WornAndHeld]%`
        contents line) diverged from FrankenDrift two ways, both engine bugs:
        1. **`%Player%` holder key not resolved** — Stone's backpack stores
           `WornByWho` as the literal variable `%Player%`, not the resolved
           player key, so it was dropped from `ListWorn` ("wearing a ring, a
           scabbard and your clothes" vs FD's "wearing a backpack, a ring, …").
           FrankenDrift resolves `%Player%`→`Adventure.Player.Key` in every
           holder accessor (clsObjectLocation.Key getter, clsObject.vb:1058);
           `a5state.cpp compute_objloc` now normalises any holder key `%Player%`
           → "Player" (the engine's player key) for all where-types.
        2. **`ListObjectsOnAndIn` ignored its argument** — the handler looped
           *every* object with on/in children, so inventory leaked the room's
           "small sword … on the table" / "Stone … on the pedestal".
           Global.vb:2213 instead iterates only the **argument's** object set.
           Ported: new `arg_object_keys` resolves the arg (a key, a `key1|key2`
           pipe list, or an OO expression like `Player.WornAndHeld` via
           `a5expr_eval`) to its object keys, and the handler shows
           DisplayObjectChildren for each with on/in children — or, when the set
           is a single object, unconditionally (so its "Nothing is on or inside
           …" canned line can surface; `display_object_children` now emits it).
           Added the missing **`WornAndHeld`** OO step to `a5expr.cpp` (worn list
           then held list, per Global.vb:1350).  **Stone of Wisdom now diffs to
           ZERO** (look + inventory); Six Silver Bullets golden (its
           `examine table` `%ListObjectsOnAndIn[%object%]%` → single-key path)
           and Anno (14 hunks, all RNG/residual) unchanged; ASan/UBSan-clean
           across the whole v5 corpus.
      - **Examine-character text functions (`%ListExits%`, `%DisplayCharacter%`/
        `%DisplayObject%`, `BeWearing/HoldingObject AnyObject`, `ListWorn`/
        `ListHeld` sub-objects)** — **DONE.**  A Stone of Wisdom exploration +
        `examine me` soak through `test/a5_groundtruth.sh` surfaced five engine
        bugs, all game-agnostic, each validated to byte-match FrankenDrift:
        1. **`%ListExits[%character%]%`** rendered raw — now the comma/"and"-
           joined, lower-cased list of the character's (restriction-checked)
           route directions, "nowhere" when none (clsCharacter.ListExits),
           implemented by reusing a5expr's character `.Exits` machinery.  Fixed
           Stone's "There is no route to the north, only east, down and in.".
        2. **`%DisplayCharacter[key]%` / `%DisplayObject[key]%`** unimplemented —
           now the entity's `.Description` (a5expr), with the canned "sees
           nothing interesting about" fallback (Global.vb:2122/2138).
        3. **`BeWearingObject`/`BeHoldingObject AnyObject`** failed (looked up an
           object literally keyed "AnyObject") — now "wears/holds ≥1 object"
           (clsCharacter.IsWearing/IsHoldingObject `If sObKey = ANYOBJECT`).  This
           is what gated the examine-character CompletionMessage's
           StartAfterDefaultDescription "wearing %ListWorn%, and carrying
           %ListHeld%" segment.
        4. **`%ListWorn%` / `%ListHeld%` ignore sub-objects** — both call
           `List(, bIncludeSubObjects:=True, Indefinite)`
           (StronglyTypedCollections.vb:183), so the worn/held list is followed
           by each container's on/inside-contents block (".  Inside the backpack
           are four food rations.  Inside the scabbard is a long sword", no
           trailing period).  New `list_objects_subobj` port; the **Inventory**
           task's explicit `.Worn(False).List(Indefinite, False)` keeps its
           no-sub-objects path (still zero-diff).  `examine me` now byte-matches
           except FrankenDrift's automatic pronoun "and **you** are carrying"
           (vs our "and are carrying") — the simplified perspective/pronoun
           engine (§5), left as-is.
        Stone look/inventory/examine all diff to zero (modulo that one pronoun);
        Six Silver Bullets golden + Anno (14 hunks) unchanged; full headless
        suite green; ASan/UBSan-clean across the v5 corpus.
      - **`MoveObject ... ToSameLocationAs <object>`** — **DONE.**  The action
        was implemented only for a *character* target (the "drop" case: place the
        object in the character's room); when the target was an **object** it fell
        through to Hidden.  This broke the standard "eat one of N rations" idiom
        (and any object-swap puzzle): Stone of Wisdom's `EatAFourFo` does
        `MoveObject FourFoodRa1 ToSameLocationAs FourFoodRa` then hides
        `FourFoodRa`, where `FourFoodRa1` is the hidden "three food rations" stack
        — so eating made the rations *vanish* from the backpack instead of
        decrementing four→three→two→one.  Ported clsUserSession.vb:1570's
        object-target branch: when `sKey2` is an object, copy that object's full
        runtime location (`where` + `key`) — `dest = obDest.Location.Copy` (the
        Hidden→Hidden special-case is subsumed since a hidden target's objloc is
        already Hidden).  Character target keeps the existing room-drop path
        (checked first, matching FD's `htblCharacters`-then-`htblObjects` order).
        **Validated:** the full Stone `eat food` chain (four→three→two→one, with
        `examine food`/`inventory` between each) now **diffs to ZERO** vs
        FrankenDrift; Six Silver Bullets golden + Anno (11 hunks, RNG residual)
        unchanged; full headless suite green; ASan/UBSan-clean.
      - **Wider-corpus soak pass (Grandpa / TEE / the 15-game corpus via a
        generic `look/examine me/inventory/score/<dirs>/get all/wait` script) —
        four game-agnostic fixes:**
        1. **`<Expression>`-type task restrictions** were stubbed to always-pass,
           so Grandpa's `cl_MoreThanTw1` ("more than two words", command
           `%text%`, restricted by `LEN(REPLACE(%text%," ","gg"))-LEN(%text%)>1`)
           fired on *every* command and shadowed the whole game.  Ported
           clsUserSession.vb:5165 `SafeBool(EvaluateExpression(StringValue))`:
           new `a5text_eval_expression` (substitute `%refs%`/OO via the existing
           `expr_substitute`, then `a5_eval_sexpr` — i.e. Global.EvaluateExpression
           → clsVariable.SetToExpression) + a CBool-ish `safe_bool` in `a5restr`.
        2. **Variable restrictions on `ReferencedText`/`ReferencedNumber`**
           resolved no user variable (`vi<0` → always-pass).  Ported
           clsUserSession.vb:4459: `key1` `ReferencedText[n]`/`ReferencedNumber[n]`
           = the parser-matched `%text%`/`%number%`; the RHS is de-quoted then
           `EvaluateExpression`'d (`restr_text_value`), so the stock library's
           `ReferencedText Must BeContain "" the ""` compares against ` the `
           (FileIO.LoadRestrictions strips one outer quote layer; the inner
           `" the "` evaluates to ` the `).
        3. **`a5expr` `.Held`/`.Worn` ignored their argument and never recursed**,
           so `.Held.Count == .Held(False).Count` and (once Expression
           restrictions began evaluating) the stock Inventory contents segment
           (shown iff `Held.Count+Worn.Count > Held(False).Count+Worn(False).Count`)
           was dropped.  clsCharacter.Held/Worn(`bIncludeSubObjects=True` default):
           recurse into the held/worn containers unless `(False)` (`add_descendants`).
        4. **`BeOnCharacter`** fell through `pass_character` to the best-effort
           `return 1`, leaking a raw `%character%.CharOnWho.Name` examine-character
           segment ("you are Standing on Player.CharOnWho.Name.") for a player on
           the floor.  Ported clsUserSession.vb:4956 (location ExistWhere ==
           OnCharacter).
        Plus a harness fix: `a5run_dump` printed a blank line between the title
        and the introduction; FrankenDrift emits them adjacent (the intro supplies
        its own leading blank when it has one), so games whose intro starts with
        content (Grandpa) showed a spurious blank.  **Results:** Grandpa explore
        diff 16 → 2, TEE 3 → 2; Stone eat-chain + inventory back to zero; SSB
        golden regenerated (one fewer blank); Anno (11) + the full headless suite
        unchanged; ASan/UBSan-clean across the corpus.
      - **Multiple-object references (`%objects%` / "all")** — **DONE.**  A port
        of clsUserSession.InputMatchesObjects (vb:5305) + the ExecuteSingleAction
        ReferencedObjects loop (vb:1391), in the existing files (no new sources):
        - **Grammar** (`a5run.cpp match_objects` / `match_object_one`): `all`,
          `all <plural>`, `X and Y`, comma lists, a trailing `… except/but/apart
          from …`, and plural-noun matching.  Plural nouns match via a new
          `name_match_plural` keyed on `clsObject.GuessPluralFromNoun` (the
          existing `guess_plural_from_noun`, now shared with emit_cantsee).  A
          bare `all` expands over the player's seen objects
          (`expand_all_objects`, the `htblObjects.SeenBy` analogue).
        - **Resolution** (`resolve_plural`, called from `resolve_refine` only for
          a *genuinely* multi-object input — `match_objects` yielding > 1 item):
          each item's chosen key is restriction-filtered (the Applicable tier per
          item); items that pass are kept, and if **none** pass the whole set
          resets so the task fails.  A *single*-object `%objects%` input is bound
          and refined exactly like a bare `%object%` (one item), so the existing
          ambiguity / "You can't see any &lt;plural&gt;!" / no-ref semantics are
          byte-identical to before (e.g. Anno's `x candleholder`, `x threads`).
          A multi-item set that resolved to only out-of-scope objects also yields
          `RR_CANTSEE`.
        - **Per-item execution**: the resolved keys are stored in
          `a5_state_t.ref_items` (+ `ReferencedObject` = the first, and
          `ReferencedObjects` = the `key1|key2|…` pipe list so the OO/text engine
          renders the whole set).  `run_action` runs each `ReferencedObjects`
          action once per item; a bare `%objects%` renders the items as an
          "a, b and c" name list.
        - **FailOverride**: a "get all"-style command whose `%objects%` resolved
          to no passing item shows the matched task's `<FailOverride>` ("There is
          nothing worth taking here." — vb:788), now modelled
          (`a5_task_t.fail_override`) and emitted by `scan_tasks`.  **Follow-up
          fix:** `resolve_refine` only routed `%objects%` to `resolve_plural`
          when `match_objects` yielded > 1 item, so a bare `all` that expands to
          **zero or one** seen object (e.g. `get all` in an empty room) fell into
          the single-reference path and emitted the no-ref "Sorry, I'm not sure
          which object you are trying to take." instead of the FailOverride.  Now
          any `had_all` input (the `all` flag, matching FrankenDrift's
          InputMatchesObjects) takes the plural path regardless of count, so the
          empty/single "all" cases reach `resolve_plural`'s FailOverride/per-item
          logic.  Six Silver Bullets' `get all` now byte-matches FrankenDrift
          ("There is nothing worth taking here."); the SSB golden + `a5objtest` +
          Anno/Stone are unchanged; ASan/UBSan-clean.
        - **Fix surfaced**: a sentence-ending `%objects%.`/`%object%.` was
          mis-parsed as the start of an OO property chain (rendering a raw key);
          the OO branch now requires an alpha char after the dot, so the bare
          name/list path renders it.
        - **Validated**: new self-contained synthetic regression
          `test/a5_objects_test.cpp` (`make -f Makefile.headless test` as
          `a5objtest`) covering "X and Y", `all` + per-item filtering, "all
          except Z", plural nouns and the FailOverride.  **Ground truth**: the
          Anno 1700 walkthrough, Stone of Wisdom and the Six Silver Bullets
          golden are **byte-identical** to the pre-change baseline (the single-
          object `%objects%` path is unchanged); a 15-game `all`/`drop all`/`get
          all` soak shows only the intended new behaviour (FailOverride + real
          per-item moves) and **no crashes**.  **ASan/UBSan-clean** across the
          corpus.
        - NOTE (simplifications): a *still-ambiguous individual item* inside a
          multi-object list ("take key and coin" with two keys) is collapsed to
          the visible-first candidate rather than re-prompting per item; the full
          combinatorial RefineMatchingPossibilites cross-product across two
          reference slots is not ported (the single-plural-reference case is);
          and the plural grammar is wired for `%objects%` only, not `%characters%`
          (no shipped corpus command needs the latter).  None are exercised by the
          ground-truth corpus.
      - **Expression negative-zero formatting (score percentage)** — **DONE.**
        A `score` soak across the 15-game corpus surfaced Lost Coastlines'
        score line `(<#%score%/%maxscore%#>%)` diverging: with `score=0` and a
        negative `maxscore=-96`, `0 / -96 == -0.0`, which .NET Core's
        `Math.Round(.., AwayFromZero).ToString()` renders as **"-0"**, but
        `a5sexpr` produced "0" twice over — `parse_mul`'s away-from-zero
        `floor(q + 0.5)` collapses `-0.0` to `+0.0`, and `fmt_num`'s
        `(long long)` cast drops the sign anyway.  Fixed both: the division
        rounding now `copysign`s the sign of `q` back onto a zero result (faithful
        to `Math.Round`, which preserves `-0.0`), and `fmt_num` emits "-0" for a
        negative zero.  Lost Coastlines `score` now byte-matches FrankenDrift;
        Six Silver Bullets' `(NaN%)` (divide by a missing `%maxscore%`) and the
        whole-corpus `score` soak (Anno/Stone/TEE/XXR/Halloween/… all zero)
        unchanged; full headless suite green; ASan/UBSan-clean.
      - **`BePartOfCharacter` / `BePartOfObject` object restrictions** — **DONE.**
        Both were unhandled in `pass_object`, falling through to the lenient
        "unknown operator => don't suppress" `return 1`.  This let MI's and TBN's
        stock `cl_HandfireOu1` **System** task ("As you move into the light, the
        ball of handfire winks out with a 'pop'.") fire on turn 0: its AND chain
        is `Player MustNot BeWithinLocationGroup DarkLocations` (true at the
        non-dark `StartOptio` start room) AND `Player MustNot BeAtLocation
        cl_MagorSCham1` (true) AND `cl_Handfire1 Must BePartOfCharacter Player`
        — and that last restriction wrongly passed even though the handfire is a
        Static object whose StaticLocation is Hidden, so the task ran and printed
        the spurious line FrankenDrift never shows.  Ported
        clsUserSession.vb:4291/4306: `BePartOfCharacter` = object `where ==
        PART_CHAR` and holder key == k2; `BePartOfObject` = `where ==
        PART_OBJECT` (k2 == ANYOBJECT) or also parent key == k2.  MI and TBN now
        diff to ZERO; the three ground-truth games + the whole-corpus score soak
        unchanged; ASan/UBSan-clean.
      - **Non-English localization (Halloween — the next big P4 subsystem; BOTH
        halves DONE).**  Halloween is a Danish ADRIFT 5 game; a generic
        `look/examine me/inventory/get all/<dirs>/wait` soak originally showed
        ~29 divergence lines, all localization, from two coupled mechanisms:
        1. **ALR applied at the Display boundary, translating the stock
           literals.** — **DONE.**  The real FrankenDrift text pipeline is
           **`clsUserSession.Display` → `Global.ReplaceALRs`** (Global.vb:519:
           ReplaceFunctions → ReplaceExpressions → ALR loop → auto-capitalise →
           a second ALR round), and `Display()` is called **per output fragment**
           (room desc, completion message, conversation, event text, *and* the
           explicit stock literals — e.g. `sOutputText = ReplaceALRs("Time
           passes...")`, clsUserSession.vb:3638).  So FrankenDrift is **per-
           fragment**, not whole-output; Scarier's per-fragment `a5text_process`
           already matched it for game-authored text — the gap was only the
           engine's **stock C-literals** (the NotUnderstood ladder, the `Also
           here is ` prefix, `Time passes...`, the canned visibility messages)
           which were emitted as raw `sb_puts` and never saw an ALR pass.
           Halloween ships ~138 `<TextOverride>` ALRs whose OldText are exactly
           those English built-ins (`I did not understand the word ` → `Jeg
           forstod ikke ordet `, `Also here is ` → `Desuden er her `, ` and ` →
           ` og `, `Time passes...` → `<br>Tiden går...<br>`, etc.).  **Fix:** a
           single Display-boundary ALR pass — new `a5text_display_alr` (ALR loop
           → auto-capitalise → second ALR round, `boundary_alr_once` rendering
           each NewText to plain so an override carrying `<br>` lands as a
           newline, not a literal tag) — applied to the assembled turn output by
           the new `finish_turn` helper at every `a5run_intro`/`a5run_input`
           exit.  It is a **pass-through when the game has no ALRs** (the three
           English ground-truth games never pay for it), and double-applying the
           already-per-fragment-ALR'd game text is idempotent for these games
           (their OldTexts never recur in their NewTexts) — verified: **Six
           Silver Bullets golden + the synthetic tests unchanged; Anno/Stone
           byte-identical (Stone actually improved 3→1, the boundary capitalise
           tidying two lines)**; ASan/UBSan-clean across the 15-game corpus.
           Halloween's NotUnderstood / catch-all / `Also here is ` / `Time
           passes` lines now byte-match FrankenDrift's Danish.
           **Remaining Halloween divergences are NOT localization:** (a) FD's
           richer **NotUnderstood ladder** — a verb-only input prompting
           `<Verb> who?` / `what?` (and the seen-noun fallback) — **NOW DONE**
           (see the dedicated "Verb-only NotUnderstood ladder" item below; `tag`→
           "Tag hvad?" byte-matches FD); the residual `<Verb> where?` is dead in
           FD too (the normalised-command quirk, documented there); and (b)
           pre-existing multi-segment description blank-line spacing.
        2. **Localized direction names from the Adventure header.**  — **DONE.**
           The header carries 12 `<DirectionNorth>Nord/N/Nordpå</DirectionNorth>`
           … `<DirectionOut>Ud/Udenfor/Forlad</DirectionOut>` fields (slash-
           separated synonyms; the **first** is the display name).  These drive
           BOTH (a) **direction parsing** — only the listed synonyms match, so
           Danish `w`/`e` are *not* directions and fall through (FrankenDrift's
           `Jeg forstod ikke ordet "w"`; Scarier formerly hardcoded the English
           compass words and wrongly matched `w`→west), and (b) **direction
           display** — the no-route / exit-list messages now use `nord, syd og
           op`, not `north, south og up`.  Implemented: the 12 `<Direction*>`
           fields parse into `a5_adventure_t.dir_re[12]` (DirectionsEnum order,
           `a5model.cpp`); `a5parse_set_directions` (called once from
           `a5run_new`, resets to English first so it is idempotent across games)
           installs them into the parser's mutable per-direction table —
           `directions_re`/`a5parse_canonical_direction`/`a5parse_directions_re`
           now read it (Global.DirectionRE: lowercase, `/`→`|`), and the new
           `a5parse_direction_name` (Global.DirectionName: the first synonym
           before `/`) feeds a5expr's `.Exits`/`%ListExits%` renderer
           (`dir_display`).  **The three ground-truth games define no
           `<Direction*>` fields (verified), so they stay byte-identical**
           (English fallback); Six Silver Bullets golden + the synthetic tests
           unchanged; ASan/UBSan-clean across the whole 15-game corpus.  NOTE:
           the `nord, syd og op` join still relies on the per-fragment ` and `→
           ` og ` ALR; the remaining Halloween divergences are all item 1 (the
           stock C literals — NotUnderstood, `Also here is `, `Time passes…` —
           never see the ALR pass, so they stay English).
      - **Verb-only NotUnderstood ladder (`<Verb> what?/who?`) + remembered-verb
        retry + seen-character branch** — **DONE.**  The previously-flagged
        "richer NotUnderstood ladder" P4 item (clsUserSession.NotUnderstood,
        vb:3469-3613): a bare verb that matches a "verb %ref%" command now prompts
        for the missing reference and remembers the verb, the next input is re-run
        as "<verb> <input>", and a seen+visible character noun gets the canned
        "I don't understand what you want to do with <Name>." line.
        - **Verb-ladder** (`a5run.cpp not_understood`): for a single-word input,
          walk the General tasks (priority order); the first whose command(0)
          contains the verb + a space + an object/character reference prompts
          `PCase(verb) what?` / `who?` (confirmed by regex-matching `verb
          sdkfjdslkj`), stores `run->remembered_verb`, and returns.
          **FAITHFUL QUIRK reproduced:** FrankenDrift (and the original Runner)
          test the *normalised* command — every bare singular reference suffixed
          with "1" (`%object%`→`%object1%` …, FileIO.vb:647).  The object/character
          branch checks the substring `"%object"`/`"%character"` (no trailing %),
          which still matches `%object1%`/`%character1%`, but the direction branch
          checks `"%direction%"` *with* the trailing %, which no longer matches
          `%direction1%` — so the "where?" prompt is **dead code** in the Runner
          (a bare movement verb falls through to the catch-all).  Verified in
          FrankenDrift by instrumenting `NotUnderstood` (the `%direction%` branch
          is never entered).  We normalise the command before the same three
          Contains checks, so `where?` is dead by the identical mechanism — `go`
          gives the catch-all, matching FD, while `take`/`drop`/`open`/`unlock`
          give "X what?".
        - **Remembered-verb retry** (`a5run_input`): a bare clarifier after a
          "<Verb> what/who?" prompt is re-run as a fresh turn `"<verb> <input>"`
          (FD prepends and re-`EvaluateInput`s; the retried turn always produces
          output so it claims the turn).  `rverb` is captured-and-cleared at the
          top of each turn (so any successful turn naturally drops it, matching
          FD's clear-on-success) and only a SystemTask re-sets it (FD keeps it
          across `SystemTasks`).
        - **Seen-character branch**: mirrors the existing seen-object branch —
          a seen+visible character whose descriptor/name appears in the input
          yields "...what you want to do with <Name>." via the new
          `a5text_character_known_name` (clsCharacter.Name's Known-aware
          descriptor/proper-name selection).
        - **Validated vs FrankenDrift ground truth:** `take`/`drop`/`open`/
          `unlock` → "X what?"; `go`/`walk`/`run`/`move` → catch-all (where? dead);
          `get`/`ask` → the unresolved-reference message; all byte-match.  The
          **Danish Halloween** game now byte-matches too — `tag`→"Tag hvad?"
          (the stock " what?" literal is ALR-translated at the Display boundary to
          " hvad?"), `gå`→the Danish catch-all — and the remembered-verb retry
          re-runs the Danish noun.  **No regressions:** Anno 1700 (now full MATCH),
          Stone of Wisdom (diff to ZERO), the Six Silver Bullets golden
          + a5parse/a5arith/a5distest/a5walk/a5objtest all unchanged.
          **ASan/UBSan-clean** across the whole 15-game corpus (ladder +
          remembered-verb-retry-recursion soaks).  No new source files — all in
          the existing `a5run`/`a5text` (+ one new `a5text` export), so the
          Makefiles and Xcode target need no new entries.
      - **Corpus residuals**: all resolved. RtC reached full MATCH (commit
        `65752758`); Grandpa's Ranch fixed (commit `74a67056`); FBA / Bug Hunt On
        Menelaus / Oct 31st / XXR RNG/event-timing divergences also resolved.
- [x] **P5 Resources & save**: **DONE.** v5 save/restore wired into `os_glk.cpp` Glk
      save stream; v5 path integrated into Spatterlight (commit `2a45b0e0`); full
      deterministic walkthroughs committed for Six Silver Bullets and a large
      ADRIFT 5 corpus (see `test/run_a5_walkthroughs.sh`).
      - **v5 save/restore (engine level) — DONE.**  `a5run_save` / `a5run_restore`
        (in `a5run.cpp`, declared in `a5run.h`) serialise the full mutable runtime
        to a self-contained `<SaveState>` XML buffer and apply it back, mirroring
        FrankenDrift's `FileIO.SaveState`/`LoadState` (clsGameState).  Captured:
        object locations (`Where`+holder/location `Key`), character location/
        position/on-object, variable numeric+text values, completed tasks,
        property overrides (`SetProperty` layer), the cumulative object/character
        "seen" sets, the per-event timer machine (status / Length / TimerToEnd /
        LastSubEvent time+index / WhenStart / triggering task / resolved sub-event
        offsets), the per-walk machine (status / timer / LastSubWalk / step
        durations / sub-walk offsets / ComesAcross flags), the `<DisplayOnce>`
        displayed-segment set (by DOM pre-order index, stable across an identical
        re-parse), the SetLook stack, conversation state and end-game state.
        **Beyond FrankenDrift's format we also record the RNG state** (new
        `a5rand_get_state`/`a5rand_set_state` over `erkyrath_random_get/set_detstate`)
        so a restored game replays the identical deterministic sequence instead of
        re-seeding to 1234 and diverging.  On restore, holder/location keys are
        re-interned to the model's own stable string pointers (`intern_key`) so
        they outlive the freed save buffer; entity arrays restore positionally
        (model order), the task/seen/override/displayed/look sets sparsely.
        - **Validated**: a new self-contained synthetic regression
          `test/a5_save_test.cpp` (`make -f Makefile.headless test` as
          `a5savetest`) plays an intro + pre-split sequence (take object, roll a
          RAND variable, tick a timed event), saves, then proves a fresh run
          restored from the blob (no intro replay) produces **byte-identical**
          continuation output — exercising object location, the variable + RNG
          round-trip (the rolled value must match across the restore), the event
          timer (the clock strike lands on the same turn), the seen sets and the
          completed-task / carried state via the room view.  Plus a real-game
          self-check knob in the `a5run_dump` harness (`A5_SAVE_AT=N`: save after
          the Nth command, tear the run down, rebuild + restore, continue): the
          **Six Silver Bullets golden script, Anno 1700 and Stone of Wisdom all
          round-trip byte-identically** to a plain run at splits 1/3/5/7, and a
          `save@3` soak across the whole 15-game corpus is **ASan/UBSan-clean**.
        - NOTE: the save format is Scarier's own `<SaveState>`, not
          byte-compatible with the official Runner / FrankenDrift `.tas` saves
          (interop is a separate effort — see **`TODO_a5_frankendrift_save_compat.md`**
          for the full FrankenDrift-compatible-save plan: zlib-framed `<Game>`
          schema, the object dynamic/static location split, and a dual-format
          read-sniff).  Mid-prompt disambiguation state
          (the "Which X?" `amb_*` fields), the remembered-verb retry and the
          `known_words` cache are transient and not serialised (a save resumes at
          a fresh command boundary).  The engine API is wired into `os_glk.cpp`
          and Spatterlight as of commit `2a45b0e0`.

---

## 10. Open questions / risks (all resolved)

All pre-implementation risks were addressed during development: the bespoke XML
pull parser (`a5xml.cpp`), the command-pattern regex translator (`a5parse.cpp`),
obfuscation toggle validated across the full v5 corpus, determinism via
`erkyrath_random` (`a5rand.cpp`), and the STL boundary contained to the blorb
container layer.
```
