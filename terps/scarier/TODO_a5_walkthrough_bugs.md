# TODO: ADRIFT 5 conformance bugs surfaced by the walkthrough corpus

## ⭐ Unmatched sibling reference must not preempt another reference's cantsee (DieFeuerfaust, broad)  ✅ DONE (2026-06-30)

**DieFeuerfaust → full MATCH (golden `test/DieFeuerfaust_expected.txt`); MagneticMoon
6→4 bonus.** `pour oil on me` printed Scarier "Sorry, I'm not sure which object you
are trying to pour." where FD prints **"You can't see any oil!"** (the singular
`%object%` out-of-scope cantsee for object1="oil", which matches 2 hidden liquid
objects, neither in scope).

**Root cause — `resolve_refine` (`a5run.cpp`) short-circuited on the *first*
unmatched reference.** `pour %object1% on %object2%`: object1="oil" resolves to 2
out-of-scope candidates (an ambiguity → cantsee), object2="me" matches no object. As
soon as object2 hit the `have_noref` branch, Scarier returned `RR_NOREF` immediately
— before refining object1 or detecting its cantsee — so scan_tasks surfaced the
task's `ReferencedObject2 Must Exist` message instead. FD instead treats an
unmatched-but-`Must Exist` reference as a **second-chance zero-Item match**
(`InputMatchesObject` returns True via `HasObjectExistRestriction` but appends no
Item, clsUserSession.vb:5404), so the post-refine loop skips it while a *sibling*
reference's ambiguity still sets `sAmbTask` and wins (DisplayAmbiguityQuestion).

**The discriminator (why `pour oil on me`→cantsee but `blow dart at <absent>`→Must
Exist).** Both are `%object1% PREP %object2%` with object1 out-of-scope-ambiguous and
object2 unmatched — structurally identical. The difference is the **BracketSequence**:
the pour task's `#A#A#` evaluates only object1's 3 restrictions — object2's `Must
Exist` is **truncated out**, so object2 is *not actually required* → object1's
ambiguity surfaces. The blow task's `#A#A(#O#)A#A#` and Spectre's hang task
`#A#A#A#A#A#` **include** object2's `Must Exist` → object2 is required → its absence
is the no-reference fallback ("...trying to blow.", "...trying to hang..."). ADRIFT
evaluates only the first M restrictions where M = the `#` count in the bracket (the
same truncation `eval_block` already models).

**Fixed (both faithful, validated zero-regression):**
1. **`a5restr.cpp a5restr_exist_evaluated(restrictions, ref_alias)`** — is a
   `<ref_alias> Must Exist` restriction within the evaluated bracket prefix (first M
   `#`)?  Default (empty) bracket ANDs every restriction, so it's required then.
2. **`a5run.cpp resolve_refine`** — the deferred `have_noref` return now runs only
   *before* the post-refine ambiguity loop when the unmatched reference is genuinely
   **required** (`a5restr_exist_evaluated` of its `Referenced…`-alias true). Otherwise
   the sibling ambiguity (`RR_CANTSEE`/`RR_AMBIG`) is checked first, and the optional
   no-reference fallback returns only if nothing was ambiguous.

**Result:** **DieFeuerfaust 2→0** (full MATCH, golden captured), **MagneticMoon 6→4**
(two hunks of the same class), Axe/Spectre unchanged at baseline (the `blow dart` /
`hang amulet` Must-Exist messages stay correct). No other corpus game moved; all a5
unit tests pass (parse/arith/dis/walk/obj/save); budgets re-blessed.

## ⭐ Plural `%objects%` reference model: singular-token leak + spurious cantsee (AxeOfKolt, broad)  ✅ DONE (2026-06-30)

**Two faithful fixes to the `%objects%` reference model, both broadly beneficial:
AxeOfKolt 12→4, SpectreOfCastleCoris 17→8, RevengeOfTheSpacePirates 5→4,
DieFeuerfaust 6→2, LostChildren 4→2, RunBronwynnRun 7→4** — no other corpus game
moved, all a5 unit tests pass (arith/parse/dis/walk/obj/save), budgets re-blessed.

1. **A `%objects%` command wrongly bound the *singular* `%object%`/`%object1%`
   text token.** Axe's give task (`Task268`, `[give/offer] %objects%`) fails its
   first restriction (`AnyCharacter Must BeInSameLocationAsCharacter Player`) when
   nobody is present, whose message is `There is nobody here to give
   %TheObject[%object%]% to!` — note **`%object%`** (singular), not `%objects%`.
   FD renders the singular as **nothing** (`GetReference("ReferencedObject")`
   resolves only a reference whose `ReferenceMatch="object1"`,
   clsUserSession.vb:3990; a `%objects%` command's ReferenceMatch is "objects"),
   so FD prints **"...give nothing to!"**.  Scarier's `bind_reference` aliased the
   plural's first key onto `ReferencedObject` (stem-strips "objects"→"Object"), so
   `%object%` rendered "the domino" → **"...give the domino to!"**.  **Fixed**:
   `bind_reference` still binds the key (override-key matching and the
   `ReferencedObjects` restriction path need it) but sets a new
   `a5state.ref_object1_plural` / `ref_character1_plural` flag; `a5text`'s
   `oo_firstkey` returns empty for the singular `%object%`/`%object1%`
   (resp. `%character%`/`%character1%`) token when the slot was plural-derived.
   Fixes Axe `give domino/hat/fruit/hammer`.

2. **`resolve_plural` returned a spurious `RR_CANTSEE` ("You can't see any
   <plural>!") for an out-of-scope plural reference.**  FD's `InputMatchesObject`
   spreads each matching object into its **own Item with a single
   MatchingPossibility** (clsUserSession.vb:5387-5391), so a plural reference
   NEVER has an Item with >1 possibilities — and `DisplayAmbiguityQuestion`
   (the source of "You can't see any X!") only fires for Count>1.  So a plural
   command whose nouns are all out of scope is NOT a cantsee: GetGeneralTask runs
   the task and `PassRestrictions` surfaces the first failing restriction's
   message.  Axe `give planks`/`give nails` (nobody present) → the give task's
   "There is nobody here to give nothing to!"; `take seeds` (never seen) →
   `ReferencedObjects Must HaveBeenSeen` → "Sorry, I'm not sure which object you
   are trying to take."; `wear goggles` likewise.  **Fixed**: dropped the
   `RR_CANTSEE` return from `resolve_plural`'s none-passed branch (it now resets to
   the full set and returns `RR_FAIL`, letting restrictions decide).  The genuine
   "You can't see any X!" message still comes from the **singular** `%object%`
   path (`resolve_refine` `RR_CANTSEE`, e.g. `press button`, `examine rack` — one
   Item, >1 possibilities, none visible), which StarshipQuest/anno1700 goldens
   exercise and which is unchanged.

**Residual (AxeOfKolt 4):**
- **`say to <absent char> <text>` / `tell <unseen char> about <text>`** (3 hunks).
  Axe is **`HighestPriorityPassingTask`**; in that mode FD reassigns
  `GetGeneralTask` for *every* failing-with-output task (clsUserSession.vb:6076),
  so the **highest**-priority failing task's message wins, not the first.
  `say to apprentice "nails"` matches both `Task314` (`[say to] %character%
  %text%`, Priority 100989 → "You can't see the blacksmith's apprentice!") and the
  higher `Say` (`[say] %text%`, 801176 → "I'm not sure which character you are
  referring to."); FD shows Say's.  Scarier's `scan_tasks` keeps the *first*
  (lowest-Priority) failing-with-output message.  **A naive "keep highest under
  hp_passing" fix mis-ticks Six Silver Bullets' turn timer** (its bell-toll/sniper
  events fire early and the agent dies), so the precise interaction with turn
  advancement / the `iPriorityFail`+LowPriority guard is still open.  Same cause
  fixes the `tell magor` "must be seen" vs "You can't see Magor!" hunk.
- **`throw seed`** (1 hunk): Scarier "Sorry, I didn't understand that command." vs
  FD "Sorry, I'm not sure which object you are trying to throw."  `throw %object%`
  (singular); "seed" resolves to no in-scope object, but FD keeps the throw task as
  the second-chance `sNoRefTask` (it has a `Must Exist` restriction) and surfaces
  that message, while Scarier drops it to NotUnderstood.  The singular-noun
  second-chance/noref path for a never-matched object.

## ⭐ Grandpa's Ranch → full MATCH: BeforeActionsOnly swap + deferred room view  ✅ DONE (2026-06-30)

The last two GrandpasRanch hunks fixed; **GrandpasRanch 2→0** (golden
`test/GrandpasRanch_expected.txt`), and the deferred-look fix also dropped
**JacarandaJim 101→99** vanilla / **5→3** xoshiro. Two independent root causes:

1. **`BeforeTextOnly`/`BeforeActionsOnly` parent-suppression flags were swapped**
   (`a5run.cpp execute_task_with_overrides`). FD (clsUserSession.vb:1106/1111):
   parent **actions** run for `BeforeActionsOnly`|`BeforeTextAndActions`; parent
   **text** is output for `BeforeTextOnly`|`BeforeTextAndActions`. Scarier had
   `BeforeTextOnly→parent_text=0` and `BeforeActionsOnly→parent_actions=0`, the
   exact inverse. Grandpa's first `open gate` (`vnl_OpenGate1`, `BeforeActionsOnly`,
   "safe enough to open…") therefore printed the generic **"You open the gate."**
   that FD suppresses (and, via the run_task both-text-and-actions gating, still
   opened the gate, so the bug was invisible until the message). Fixed by swapping
   the two assignments: `BeforeActionsOnly→parent_text=0`,
   `BeforeTextOnly→parent_actions=0`.

2. **The room view (`Execute Look`) rendered eagerly, at pre-After-child state.**
   The stock `Look` task's CompletionMessage is `%Player%.Location.Description`
   with no `<Aggregate>` ⇒ `AggregateOutput=True`, so FD **defers** its function
   replacement to final Display (clsUserSession.vb:1184/1210) — the room view
   reflects state changed by `AfterText/Actions` children that run after the
   parent's `Execute Look`. Grandpa's `go west` matches `vnl_GoWestBust`
   (`AfterTextAndActions`, `MoveCharacter Buster ToLocation Location14`), so the
   new room must list Buster **"in the western enclosure"** (`ListDescription`
   gated on his location), not "just on the other side of the fence" (his old
   spot). Scarier's `emit_look` rendered immediately, before the after-child moved
   him. **Fixed** with a faithful deferral in `a5run.cpp`: when a parent has After
   children on the direct path, `run->defer_look` makes `emit_look` record a
   pending look (skip render) instead of emitting; after the After children run
   (into a side buffer), `render_look_string` renders the view **once** at final
   state and it is emitted view-then-children (so `sb_pspace` computes each
   separator against real text — Jacaranda's "…Exits are … in.  It is raining out
   here." regained its two-space join; an earlier splice-at-offset attempt lost
   it). The capture is **nesting-safe**: only the frame that *enabled* deferral
   consumes the pending look, so the nested `MovePlayer` frame (run via `SetTasks
   Execute`, which emits the actual Look) leaves it pending for the enclosing
   `PlayerMovement` frame. A single render means `view_location_impl`'s
   `marking_display` retires each `<DisplayOnce>` segment exactly once.

**Result:** **GrandpasRanch 2→0** (full MATCH, golden captured), **JacarandaJim
101→99 / 5→3** (the deferred room view also corrected JJ's rain/Alan room-entry
lines). No other corpus game moved; all a5 unit tests pass
(arith/parse/dis/walk/obj/save); budgets re-blessed.

---


Source: `test/run_a5_walkthroughs.sh` — 15 real-game walkthroughs replayed through
Scarier (`test/a5run_dump`) and FrankenDrift (ground truth) via
`test/a5_groundtruth.sh`. Full per-game diagnosis + baselines:
`test/A5_WALKTHROUGH_FINDINGS.md`.

**Reproduce one game** (diff: `<` Scarier, `>` FrankenDrift):
```sh
make -f Makefile.headless a5run
test/a5_groundtruth.sh test/adrift5-games/<Game>.blorb test/<Game>_walkthrough.txt
```
**Verify a fix:** `test/run_a5_walkthroughs.sh` — the game's hunk count must
drop below its baseline budget (then re-bless the budget in the runner's MAP and,
if it reaches 0, capture a golden `test/<Game>_expected.txt`). Achtung Panzer,
StarshipQuest, and anno1700 are fully conformant (MATCH) today.

Ordered by impact. Each item: symptom → evidence → suspected locus → fix → verify.

---

## ⭐ Per-command response aggregation (`htblResponses`) — plural path  ✅ DONE (2026-06-30)

**Implemented the plural-`%objects%` slice of FD's response-aggregation layer**
(`a5run.cpp`). A genuine multi-item `%objects%` command now runs **one item at a
time** (`run_general` iterates `st->ref_items`, FD's `ExecuteSubTasks`), each item
independently selecting its Specific overrides, with completion + override-fail
messages collected into a per-command **`resp_map`** (keyed/deduped/ref-merged)
and flushed once at the end (`resp_flush`, FD's `AttemptToExecuteTask` Display
loop). The single-reference path is untouched (`run->resp == NULL`), so non-plural
turns stay byte-identical — the entire blast radius is multi-object commands.

Key pieces: `resp_entry`/`resp_map` + `resp_add_comp` (aggregate → dedup by comp
node, merge `%objects%`; non-aggregate → render + dedup on text), `resp_add_text`
(override-fail / pinned messages), `resp_insert` (position reservation = FD's
`iResponsePosition`), `sink_len` (the override scan's "produced output?" probe,
map-aware). `run_task`'s Before message follows FD's `sBeforeActionsMessage`
logic: rendered pre-action, **pinned** to that text if the actions change its
render (e.g. `TakeObjectsFromOthersLazy`'s `(from %objects%.Parent.Name)`, whose
parent flips to the player once the take runs — was rendering "(from you)"),
else left **deferred** so a second item merges. The override-fail branch uses a
**match-any vs object-specific** distinguisher: a "lazy" empty-key override
(`child_all_any`) that fails its restrictions declines the item (general still
runs, fail dropped by the pass-reconciliation) — Amazon's loose bottle; an
object-specific override (Key=Ballgown) that fails suppresses the parent and
keeps the fail — RunBronwynn's gown. The `defer_text`/AggregateOutput hack is
bypassed on the map path (the map handles deferral).

**Result:** `get crown and bottle` takes both (crown via lazy "(from the
pedestal)" + `TakeObjectsFromOthers`, bottle via general "pick up"); the whole
Amazon bottle→pour→passage chain plays. `remove tiara, gown and shoes` →
"Ok, you remove the diamond tiara and the high-heel shoes.  You need to be
standing up to do that." (gown peeled). **TreasureHuntInTheAmazon 47→42**,
**RunBronwynnRun 9→7**; no other corpus game moved; all a5 unit tests pass
(arith/parse/dis/walk/**obj**/save + Six Silver Bullets golden); budgets
re-blessed.

**Still OPEN (the other half — the dedup-of-identical-messages mechanism #2):**
the Amazon residual 42 is now almost entirely **`Date:`/time lines** — the
off-by-one-minute clock drift (day 5+, see sub-bug below) plus the **startup
`Date:` lines FD shows and Scarier omits** (StartGame → Execute Look →
`Beforeplay1`). Enabling those needs the *within-command identical-message
dedup* (so routing Execute Look through overrides doesn't *double* every move's
date) — that requires extending the `resp_map` to the **single-reference /
movement path** too (currently `run->resp` is plural-only), plus turning on the
`SetTasks Execute Look` → `Beforeplay1` route. Higher-risk (touches every turn);
deferred. Jacaranda's buzzard (`Task62`, `DropObjects` Multiple=1) didn't move
the JJ count (plural-drop not on its walkthrough's critical path) but now runs
through the same per-item machinery.

### Original analysis (retained for the remaining dedup work)

FD does not emit task output directly. Within one **top-level** command
(`AttemptToExecuteTask`, *not* a child/sub-task), it collects every
completion/restriction message into two ordered maps —
`htblResponsesPass` / `htblResponsesFail` (cleared at the top of each top-level
call) — keyed by the **message string**, then displays each once at the end
(clsUserSession.vb:777-863). Two mechanisms hang off this:

1. **Per-item subtask iteration** (`ExecuteSubTasks`, vb:695-716). ✅ **DONE** (see
   above). A plural `%objects%` reference is expanded **one item at a time**: for
   each item, a fresh single-item ref set runs the full `AttemptToExecuteSubTask`
   (task selection + overrides). So `get crown and bottle` runs *twice* — crown
   hits the `<Multiple>1</Multiple>` `TakeObjectsFromOthersLazy` override ("(from
   the pedestal)" → `TakeObjectsFromOthers`), bottle (loose) hits the general
   `TakeObjects` ("Ok, you pick up …").

2. **Message merge + dedup** (`AddResponse`, vb:1295-1320). 🔲 **plural path done,
   single/movement path OPEN.** When a message text is
   already in the map, the new item's **references are merged into the existing
   entry** (object list grows) instead of adding a second line. For an
   **AggregateOutput** task the stored key is the **raw, un-substituted** template
   (`%TheObjects[%objects%]%` deferred to Display, vb:1184/1210), so two items
   through the same task collapse to one entry whose merged refs render
   "the ammunition and the rifle". Identical messages dedup outright — which is why
   a move shows **one** `Date:` line even though both `Beforeplay` (the move
   override) *and* `PlayerMovement`'s `Execute Look` → `Beforeplay1` each run
   `Execute ts_tasCheckTime` and emit the same "Date: …" text.

**What it would fix:**
- **Amazon** the bulk of the residual 47: the startup `Date: 12:04` (StartGame →
  Execute Look → `Beforeplay1`, currently skipped because routing Execute Look
  through overrides would *double* every move's date without the dedup — confirmed
  by experiment), the per-move date lines FD shows and Scarier omits, and the
  `get crown and bottle` → bottle cascade (open bottle → pour whiskey → secret
  passage → iron key → unlock door → camp), which is ~all of the non-date hunks.
- **RunBronwynn** the ballgown (`remove tiara, gown and shoes` → `RemoveBall`
  `<Multiple>1</Multiple>` "need to be standing" peeled per-item).
- **Jacaranda** the buzzard (`Task62`, `DropObjects` override `Multiple=1`).

**Key control-flow insight (the ordering puzzle):** a `SetTasks Execute` sub-task
runs `AttemptToExecuteTask(..., bChildTask:=True, bEvaluateResponses:=False)`
(vb:2307) — so it **shares the parent's map and does NOT flush**; all completion
messages of the whole command tree accumulate into the *one* top-level map and
flush together at the end, in insertion order. That is why the lazy "(from the
pedestal)" (a deferred Before message) still prints *before* the sub-task take
text: the position is reserved at add time (`iResponsePosition`).

**`<Multiple>` flag:** still **not parsed** into the model (`a5model.cpp`; FD
`clsTask.Specific.Multiple`, `KeyListsMatch` `bMultiple`). The plural path didn't
need it — `refs_match_specifics` already matches an empty-key specific against any
single item, and `child_all_any` is the practical lazy/specific distinguisher.

**Locus:** `a5run.cpp` `execute_task_with_overrides` / `run_general` /
`run_task`'s completion emission; `emit_completion`; the `SetTasks Execute Look`
shortcut (the deliberate no-Beforeplay1 NOTE there comes out once dedup exists);
`a5model.cpp` (parse `<Multiple>`). Mirror vb:695-863 + 1295-1320.

**Independent sub-bug (may be separable):** an **off-by-one-minute** clock drift
deep in Amazon (`10:59` vs `11:00`, day 5+) — the early clock matches exactly, so
it's an extra/missing `ts_tasIncrement` somewhere, possibly surfaced only once the
date lines align. Re-check after the aggregation layer; might be a standalone
`MinutesPerTurn` accumulation fix.

---

## Object `HaveProperty` ignored the runtime SetProperty layer (Amazon `Purchased`, broad)  ✅ DONE (2026-06-30)

**Was:** Treasure Hunt in the Amazon went dark for the entire back half of the
game. After buying the flashlight (`buy torch` → "You purchase the flashlight.")
and carrying it through the lit caves, the carriers' totem event (`CarriersFl1`,
`MoveObject EverythingHeldBy %Player% ToSameLocationAs %Player%`) drops everything
at the totem; from there FD shows the bought items "lying on the ground" and
`get flashlight` picks it up, but Scarier omitted the "lying on the ground" lines
and `get flashlight` printed **"The flashlight costs 10."**. With the flashlight
never re-acquired, every later cave rendered **"It is too dark to make anything
out clearly."** and the whole navigation/puzzle chain (open bottle → pour whiskey
→ secret passage → iron key → unlock door → camp) cascaded wrong.

**Root cause — `a5restr.cpp pass_object`'s `HaveProperty` only checked the
*static* model props** (`a5_prop_find(o->props,…)`), never the runtime override
store. `Purchased` is a `SelectionOnly` property (DependentKey `BuyableItem`) that
exists *only* once `buy`'s `SetProperty ReferencedObject Purchased Selected` adds
it at runtime (`a5state_set_prop`). So `Flashlight HaveProperty Purchased` was
**always false**: the take task's `ReferencedObjects Must HaveProperty Purchased`
restriction failed → "costs 10", and the flashlight's `ListDescriptionDynamic`
("…lying on the ground", gated on the same prop) rendered empty. (It read false
even while carried — the carried case just short-circuited on the earlier
`BeHeldByCharacter` checks, masking it.) This is the exact object-side analogue of
the character `HaveProperty` fix at the top of this file.

**Fixed** in `a5restr.cpp`: new `obj_has_property` helper mirroring
`char_has_property` — a runtime override wins (value reads absent when it carries
the `Unselected` sentinel, per clsUserSession.vb:2058), else the static prop set.
The `HaveProperty` object case (both the specific and `ANYOBJECT`/`NOOBJECT`
branches) now uses it.

**Result:** flashlight re-acquired at the totem, darkness lifts, the cave chain
plays. **TreasureHuntInTheAmazon 54→47**; no other corpus game moved (vanilla and
xoshiro both clean); all a5 unit tests pass; budget re-blessed 54→47.

**Residual (47):** 32 are `Date:`/time lines (the startup `Beforeplay1`
`Execute Look` date line FD shows but Scarier's `Execute Look` shortcut skips, plus
an off-by-one-minute clock drift); the title's leading spaces (centring); and the
`get crown and bottle` cascade — the crown is on the pedestal, the bottle loose, so
FD's `<Multiple>1</Multiple>` lazy override (`TakeObjectsFromOthersLazy`) peels only
the crown to "(from the pedestal)" while the bottle takes the general path. Scarier
runs the override over the whole piped set → "(from the pedestal and Treasury)" and
takes neither. That's the deferred per-item `<Multiple>1</Multiple>` override
feature (also RunBronwynn ballgown, Jacaranda buzzard) — FD's `ExecuteSubTasks`
iterates each item of a plural reference and aggregates same-message results
(clsUserSession.vb:695); faithful porting needs the response-aggregation layer.

---

## Character `HaveProperty` ignored the runtime SetProperty layer (Anno greetings, broad)  ✅ DONE (2026-06-30)

**Was:** Anno 1700 re-introduced an already-greeted NPC on the FIRST `tell`/`ask`.
`tell susan about cave` printed the implicit-intro greeting `She looks up at you.
"Hello there, what can I do for you?"` *before* the topic reply (FD goes straight to
the Tell topic); same with `tell lulu about raid` leaking Lulu-Belle's `"Hello
there, I'm Lulu-Belle, I own this establishment…"` ahead of the raid topic.

**Root cause — `pass_character`'s `HaveProperty` only checked the *static* model
props.** Susan's implicit-intro topic (Topic2) is gated `Susan MustNot HaveProperty
Known`; her explicit greeting (`say hello`, Topic1) runs `SetProperty Susan Known
<Selected>`, which Scarier stores in the runtime override layer (`a5state_set_prop`).
But `a5restr.cpp pass_character`'s `HaveProperty` case read only
`c->props`/`a5_prop_find` — never the override store — so `Susan HaveProperty Known`
stayed false after the greeting, the implicit intro's `MustNot` passed, and
`exec_conversation` emitted the greeting again on the next `tell`/`ask`. FD's
`clsRestriction.CharacterEnum.HaveProperty` (clsUserSession.vb:4888) calls
`clsCharacter.HasProperty` (clsItem.vb:386) over the *merged* (static + runtime)
property table; `SetPropertyValue(Boolean)` adds a SelectionOnly prop on `<Selected>`
and *removes* it on `<Unselected>` (clsUserSession.vb:2058).

**Fixed** in `a5restr.cpp`: new `char_has_property` helper — a runtime override
wins (its value reads absent when it carries the `Unselected` sentinel, mirroring
the display-name "Selected" check in `a5text.cpp`), else the static prop set. The
`HaveProperty` character case now uses it and also handles `ANYCHARACTER` (any
character carrying the prop), matching FD's enum branch.

**Result:** **anno1700 6→3** (vanilla and xoshiro) — both leaked greetings gone. No
other corpus game regressed; all a5 unit tests pass (arith/parse/obj/save +
Six Silver Bullets golden); budget re-blessed 5→3.

**Residual (3) — storage-room wake event ✅ DONE (2026-06-30); anno1700 3→0, now a
full MATCH (golden `test/anno1700_expected.txt`).** The walkthrough's best-effort
"wait until you wake in a storage room" tail: the `EffectOfTh` turn-based event
(Length 15, teleport subevent at offset 15) reached tick 11 then stalled in Scarier,
so the storage-room teleport fired one command late and the post-wake
closet/hole/wall chain played in the corridor (3 hunks). The discriminator was
**one missing event tick** on `open closet` — FD ticks the turn, Scarier didn't.

**Root cause — NOT the Applicable→Visible→Seen refinement** (the earlier hypothesis,
disproven by instrumented traces of both engines: both refine identically here).
The real cause is **FD never resetting `sRestrictionText` between commands**. In the
wait sequence, `open closet` matches the *reference-free* specific task `OpeningHid`
("Opening hideous closet in storage room") whose `<BracketSequence>` is the malformed
`##A#` (2 restrictions, 3 `#`). FD's `EvaluateRestrictionBlock` hits its inner
`Case Else` ("Error"), returns its default False **without calling any
`PassSingleRestriction`** — so `sRestrictionText` is never overwritten and keeps the
**stale** value the *previous* command (`x hole`) left from `ExamineObjects`'
`BeVisibleToCharacter` refine pass (`%CharacterName% can't see %TheObject[%object%]%.`).
GetGeneralTask sees a non-empty `sRestrictionText` → treats `OpeningHid` as
failing-*with-output* → claims the turn (HighestPriorityTask) and **ticks**, rendering
the carried template with `OpeningHid`'s empty refs as **`You can't see nothing.`**
Scarier reset its fail text per command, so `OpeningHid`'s message-less
`BeAtLocation` failure recorded nothing and the turn fell through to `OpenObjects`'
`RR_CANTSEE` (`You can't see any closets!`) — no tick.

**Fixed (four faithful pieces, all validated zero-regression across the corpus):**
1. **`a5state.restriction_text`** — a persistent (non-owned `<Message>` DOM node)
   analogue of FD's `sRestrictionText`, **not** reset between commands.
2. **`a5restr.cpp eval_restrictions`** updates it as FD's `PassSingleRestriction`
   side effect: a failing deciding restriction sets it (its `<Message>`, NULL when
   none), a passing one clears it, and a call that evaluates **no** single
   restriction (malformed bracket → `last_fail` stays the new `-2` "untouched"
   sentinel) leaves it untouched (the carry).
3. **`a5restr.cpp eval_block`** — the leading-`#`-then-non-operator (malformed) path
   now returns False **without** calling `pass_single` (matching FD's inner
   `Case Else`), so it no longer clobbers the carried text. Also the Applicable tier
   in `a5run.cpp resolve_refine` now evaluates candidates in **model order** (as FD's
   `InputMatchesObject` builds `MatchingPossibilities`), so the last-evaluated
   candidate — hence the carried `sRestrictionText` — matches FD (the surviving set
   is still kept visible-first for the default-pick).
4. **`a5run.cpp scan_tasks`** — an `RR_FAIL` task whose own restrictions name no
   failing message falls back to the carried `st->restriction_text` (failing
   *with output* → claims the turn, ticks). **`a5text.cpp`**: an unbound
   object/character reference used as a function arg renders empty (FD
   `GetReference` → `""`), and `%TheObject[]%`/`%ObjectName[]%` with an empty arg
   renders `nothing` (FD `htblObjects.List` on an empty set).

No other corpus game moved; all a5 unit tests pass (arith/obj/save/walk/dis/parse +
Six Silver Bullets golden); budget re-blessed 3→0 and golden captured.

---

## JacarandaJim RNG-stream alignment: five fixes → byte-identical draws (xoshiro 261→5)  ✅ DONE (2026-06-30)

**Was:** JacarandaJim diverged massively even under `FD_RNG=xoshiro` (261 hunks),
where the RNG stream is supposed to align byte-for-byte. The rain/Alan-remark text
and the whole champagne-teleport tail drifted because Scarier's RNG draw *sequence*
differed from FD's — a draw-order/count desync (per the runner's invariant, any
xoshiro divergence is a real Scarier bug, not RNG noise). Instrumenting both
engines' draws (`a5rand_between` ↔ the headless `XoshiroRandom`) located the first
divergence; each fix below pushed it later until the streams were **identical (187
== 187 draws)**.

1. **Event StartDelay/Length drawn in the wrong order.** `ev_init`'s
   `BetweenXandYTurns` case drew **Length then StartDelay**, but FD's
   `TimerToEndOfEvent = StartDelay.Value + Length.Value` evaluates left-to-right
   (**StartDelay first**). The two random values got swapped, so the rain event's
   initial delay was wrong and every later random timer desynced. **Fixed** in
   `a5run.cpp ev_init` by drawing StartDelay before Length.

2. **`MoveCharacter ToSameLocationAs` / `ToLocationGroup` unimplemented.** The
   chain-pull teleport runs `MoveCharacter Character8 ToSameLocationAs %Player%` to
   bring Alan along (→ "Alan is with me."); the champagne runs `MoveCharacter
   %Player% ToLocationGroup Group7` (a **RandomKey** draw over Group7's 21 members —
   the single missing draw that desynced the whole post-champagne tail). Scarier's
   MoveCharacter handled neither. **Fixed** in `a5run.cpp`: `ToSameLocationAs`
   (copy the target's location/on-furniture, faithful to clsUserSession.vb:1777)
   and `ToLocationGroup` (one `group.RandomKey` draw per action, applied to all
   affected characters, mirroring vb:1767).

3. **`Event.Position` / `Event.Length` OO-properties unresolved.** The rain "It is
   still raining quite heavily" message (`Event13`, every turn) is gated on
   `Task176 Must BeComplete AND Event12.Position > 0`. Scarier had no event-property
   support, so the AND short-circuited and the gated text (with its `RAND(8)` draw)
   never evaluated. **Fixed:** `a5expr_event_prop_hook` (wired in `a5run.cpp` to read
   the live `length_value - timer_to_end` / `length_value`), `item_kind` returns
   `'e'` for events, and `a5text_eval_expression` now runs the bare-key `a5expr_replace`
   pass (expr_substitute only handled `%ref%.Prop`, not a bare `Event12.Position`).

4. **Single-arg `RAND(n)` returned the literal (no draw).** `a5sexpr`'s 1-arg
   `rand`/`urand` returned `n` verbatim where FD's `clsVariable.SetToExpression`
   computes `Random(0, n)` (a real draw). So `<# If(RAND(8) = 1, …) #>` never drew
   and the rain RAND(8)s were absent from Scarier's stream. **Fixed** to
   `a5sexpr_rng_hook(0, n)`.

5. **Champagne teleport prose + nested override (no RNG, but visible).**
   `%LocationOf[%Player%]%` / `%DisplayLocation[loc]%` text functions were
   unimplemented (the teleport message leaked the literal) — added to `a5text.cpp`
   (LocationOf → char/object location key; DisplayLocation → an arbitrary location's
   ViewLocation via a parametrised `view_location_impl`). And `give tape to pirate`
   matched the generic Task48 ("Ta very much!") instead of Task49 (the tape-specific
   "…scuttles away" + Score+75): Task49 is a **nested** Specific override of Task48
   (itself an override of GiveObjectToChar) with FEWER specifics than the references.
   Ported FD's clsUserSession.vb:1060 expansion — a child override's specifics are
   expanded against its parent's (keep the parent's constrained slots, fill its "any"
   slots from the child) — so Task49 → effective `[tape, pirate]`.

**Result:** **JacarandaJim xoshiro 261→5** (the RNG stream is byte-identical to FD;
the residual 5 are non-RNG: a pSpace gap before "Alan appears in a puff of smoke",
a night/DarkLocations-group timing on the champagne room, a "can't climb the
slippery rocks" line, an "I move east" echo after the glacier slide). **Vanilla
271→101** — System.Random can't align to xoshiro, so the now-firing rain/Alan-remark
text diverges throughout (the documented RNG caveat); the structural game now plays
correctly. All a5 unit tests pass. Only JJ and Amazon moved; budgets re-blessed.

**Bonus — Amazon `BeHeldByCharacter` recursion (115→54).** The JJ `ToSameLocationAs`
fix also moves Amazon's jaguar (`MoveCharacter Jaguar ToSameLocationAs %Player%` —
previously a no-op, so the player couldn't `shoot jaguar` and DIED, the truncated
355-line transcript counting as a misleading "6"); Amazon now plays the full game.
That exposed a `BeHeldByCharacter` gap: the silver key (Door1's LockKey) sits inside
the carried jewelry box, so `unlock door`'s `ReferencedObject2 Must BeHeldByCharacter`
failed — FD's `clsCharacter.IsHoldingObject` recurses through held containers.
**Fixed** in `a5restr.cpp` (`is_holding_object`). Amazon 115→**54**; the residual 54
is the P2b Date/Time `Beforeplay`/`Beforeplay1` override-placement (FD shows the time
99×, Scarier 87×) + the title's leading spaces — tracked under P2b. Also the MOD
precedence fix (below) was needed for Amazon's `%Hour%+8 Mod 24` sleep clock.

**Mod operator precedence (a5arith).** FD's `clsVariable.SetToExpression` reduces
`Mod` on the same pass (run=2) as `+`/`-`, left-to-right — so `22+8 Mod 24` is
`(22+8) Mod 24` = 6, not `22+(8 Mod 24)` = 30. Scarier had `mod` at the `*`/`/`
precedence level (`arith_term`). **Fixed** by moving it to `arith_expr` (+/− level).
Amazon's hour clock now matches (was showing "30:45" for "06:45").

---

## Boolean model fields parsed as `"1"`-only, breaking `True`-serialised games (broad)  ✅ DONE (2026-06-30)

**Was:** **Return to Camelot** desynced massively (142-hunk residual). Two
distinct symptoms, one root cause:

1. **`x desk` (and every later examine) printed "I don't understand what you want
   to do with the desk." once any object had been examined.** Bisected: examining
   the gun/folders/clip (but not the drawer) broke it. Trace showed the general
   `ExamineObjects` task with `done=1 rep=0` on the failing turn — it was marked
   Completed by the first examine and then skipped (`scan_tasks`:
   `if (task_done && !repeatable) continue`), so the command no longer matched any
   task and fell to the seen-noun NotUnderstood branch.

2. **The whole conversation system was inert** — `say hello to woman` → "The woman
   ignores you.", every `ask about X` → "doesn't appear to understand you.", so the
   player never got Merlin's teleport and all later navigation cascaded
   ("no route to …"). `exec_conversation` found `intro=(none)` despite the woman
   having 12 topics incl. an `<IsIntro>True</IsIntro>` implicit introduction.

**Root cause — model boolean fields compared against the literal `"1"` only.** RtC
(and other Generator versions) serialise booleans as `True`, not `1`. FrankenDrift
loads every such field through `FileIO.GetBool` (FileIO.vb:1182), which accepts
`True`/`1`/`-1`/`Vrai`. Scarier already had a faithful `a5xml_bool` (matching
GetBool) but several parse sites still used a raw `strcmp(s,"1")==0`:
- `a5model.cpp` task `<Repeatable>` (→ ExamineObjects et al. left non-repeatable),
- topic flags `<IsIntro>/<IsAsk>/<IsTell>/<IsCommand>/<IsFarewell>/<StayInNode>`
  (→ every topic flag false ⇒ no intro/ask/tell/command/farewell ever matched ⇒
  the entire say/ask/tell conversation subsystem dead),
- walk `<Loops>/<StartActive>` and event `<Repeating>/<RepeatCountdown>`.

**Fixed** by routing all of them through `a5xml_bool`. RtC has no walk/event bool
elements, so its win comes entirely from the Repeatable + topic-flag fixes.

**Result:** **RtC 142 → 22** (vanilla and xoshiro). No vanilla regressions; all a5
unit tests pass; budget re-blessed 142→22. The remaining 22 are scattered
downstream gameplay divergences (a Merlin conversation-tree child topic giving the
wrong reply ⇒ no leather pouch ⇒ the rose-freeze puzzle and ~6 inventory lines
diverge; a guard ambush event; the known top-of-file 5.0.26 auto-correct prose).

**JacarandaJim xoshiro 256 → 261 (re-blessed), vanilla unchanged at 271.** JJ's
"Raining" event ships `<RepeatCountdown>-1</RepeatCountdown>`; GetBool("-1")=True,
which Scarier previously read as false. With the fix the event now re-draws a fresh
random `StartDelay` (0–50) on each repeat (clsEvent.vb:295-303) exactly like FD —
correct, intended random-interval rain. Vanilla is unchanged (the primary
baseline), so the +5 is pure RNG-draw realignment in JJ's already-irreducible
rain/champagne-teleport tail. Suite green in both modes (exit 0).

---

## AggregateOutput deferred render + `<# OneOf(...) #>` RNG draw (Stone of Wisdom)  ✅ DONE (2026-06-30)

**Was:** Stone of Wisdom's `x window` showed the conditional window segments one
turn late and never showed the second ("…strange creatures crawling…"), and the
troll's per-turn approach line always read "The troll goes one step forward…"
where FD varied it — a 5-hunk residual (the last two non-RNG SoW bugs). Two
independent root causes:

1. **Aggregate parent text rendered before its After-children's actions.** `x
   window` runs the general `ExamineObjects` task, whose CompletionMessage is
   `%object%.Description` (the `Opening` object's three Windowcoun-gated
   segments). `ExamineAnO` is an **AfterTextAndActions** Specific override of it
   that runs `IncVariable Windowcoun`. `ExamineObjects` carries no
   `<MessageBeforeOrAfter>` (FD load-defaults `eDisplayCompletion=Before`,
   FileIO.vb:1618) and no `<Aggregate>` (so `AggregateOutput=True`,
   clsTask.vb:318). With AggregateOutput, FD **defers the message's function
   replacement to final Display** (clsUserSession.vb:1184/1210 replace eagerly
   only when *not* aggregate) — i.e. AFTER the After-child increments Windowcoun
   — so turn 1 shows "movement" (Windowcoun==1) and turn 2 "creatures"
   (Windowcoun>1). Scarier rendered the parent text eagerly in
   `execute_task_with_overrides` (`run_task(parent)` before the After loop), at
   the pre-increment count, lagging one turn and never reaching the >1 segment.
   **Fixed:** new model field `a5_task_t.aggregate` (`<Aggregate>`, default True,
   `a5model.cpp`). In `execute_task_with_overrides`, an aggregate parent with no
   actions of its own and passing After-children now runs the children into a
   side buffer first, then renders the parent CompletionMessage at the final
   state and splices it ahead of the children's output — mirroring FD's deferred
   Display order (parent text positioned first, but reflecting post-action state).

2. **`<# OneOf(...) #>` always returned the first operand.** The troll line is
   `<# OneOf("…step forward…","…aggressively…","…madman…","…wildly…") #>`.
   `a5sexpr`'s `apply_function` stubbed `either/oneof/rand/urand` to the first
   operand ("no shipped `<#…#>` uses these" — disproven by SoW), so the message
   never varied *and* never drew an RNG number (desyncing the stream). **Fixed:**
   a host RNG hook `a5sexpr_rng_hook` (set to `a5rand_between` by the run harness;
   NULL → deterministic fallback for the RNG-less `a5text_dump` build), and
   faithful `oneof`/`either`/`rand`/`urand` per `clsVariable.SetToExpression`
   (oneof 0-based index = `Random(n-1)` = `a5rand_between(0,n-1)`; either =
   `Random(1)==1 ? left : right`; rand = `Random(min,max)`).

**Result:** **StoneOfWisdom 5→2**, a **perfect MATCH (0)** under
`FD_RNG=xoshiro` — the vanilla residual 2 is only the troll `OneOf` drawing a
different element from FD's unaligned System.Random stream (the documented RNG
caveat). No other corpus game moved (every game at baseline); all a5 unit tests
pass (arith/parse/dis/walk/obj/save); the RNG-less `a5text_dump` still builds via
the NULL-hook fallback; baseline re-blessed 5→2.

---

## Doubled-quote numeric variable assignments (`= ""1""`) silently set to 0 (broad)  ✅ DONE (2026-06-30)

**Was:** Axe of Kolt desynced throughout — the brewer's-dray-arrival event prose,
the "An area here has been roped off…" room segment, the "…you see a brewer's dray"
lines, and Score increments were all missing (42-hunk residual). Traced concretely:
`Task831` ("See Man with Stakes", LocationTrigger, non-repeatable) printed its
narrative **once** and ran `SetVariable Variable158 = ""1""`, but Variable158 stayed
0, so Location36's `AppendToPreviousDescription` segment gated on `Variable158==1 AND
Variable79==0` was dropped on every visit (`grep -c "roped off"` = 0 in Scarier, 15
in FD).

**Root cause — these games serialise numeric assignments with *doubled* quotes**
(`= ""1""`, `Score = ""5""`); other corpus games use the single-quote form `= "1"`
and were fine. Counts: **AxeOfKolt 1480, Spectre 825, LostChildren 15** (Fortress
768, XXR 23, Tingalan 16 — non-corpus). Scarier's `split_assignment` (`a5run.cpp`)
strips **one** surrounding quote pair → `"1"`, then `eval_num_value` runs it through
`a5_eval_arith`, which had **no quoted-literal case** → `ok=false` → the
`strtol("\"1\"")` fallback returned **0**. So every `= ""N""` numeric assignment set
the variable to 0, leaving every downstream gate variable (and Score) at 0.

**How FD does it (the semantics matched):** (1) `FileIO.LoadActions`
(FileIO.vb:426-429) strips one surrounding quote pair for file version > 5.0000321
(`""1""`→`"1"`) — Scarier's `split_assignment` already replicates this; (2)
`clsVariable.GetToken`/the `vlu` token (clsVariable.vb:148-152, 587-589) values a
remaining quoted literal `"X"` numerically via VB `Val` (`"1"`→1, non-numeric→0).
The missing piece was (2).

**Fixed** in `a5arith.cpp arith_atom`: added a quoted-string-literal case (`"`/`'`)
mirroring FD's `vlu` — scan to the matching close quote, value the inner content as a
leading integer (`strtol`, Val semantics; non-numeric→0), consume the close quote,
keep `ok` true. Lives in the shared evaluator (sole caller `eval_num_value`), and
also handles a quoted literal embedded in a larger expression (`%var% + "5"` after
substitution). `split_assignment` left unchanged. `a5arith_test.cpp` extended with
`"1"`→1, `"0"`→0, `"5"`→5, `'7'`→7, `"x"`→0, `"5" + "3"`→8.

**Result:** **AxeOfKolt 42→12** (the dray event/roped-off area/Score chain plays);
no game regressed (verified my fix is a literal no-op for the games with zero quoted
assignments — JacarandaJim/RunBronwynn/StoneOfWisdom: same diff with the change
reverted). Spectre/LostChildren stay at budget (their doubled-quote vars don't gate
visible walkthrough output enough to move the hunk count). All a5 unit tests pass
(a5arith, Six Silver Bullets golden, save); AxeOfKolt baseline re-blessed 42→12.

**Harness note (re-learned):** running many things concurrently can get a
FrankenDrift child killed mid-game under load, poisoning `test/.fd_cache` with a
truncated transcript and inflating that game's hunk count (saw JJ 109→233,
RunBronwynn 9→10 transiently). A clean re-run refreshed the cache and they returned
to baseline. Confirm surprising counts with a fresh cache before treating them as
regressions.

---

## SetTasks-Execute function-arg auto-cap + `BeInState` appended-state property (broad)  ✅ DONE (2026-06-29)

**Was:** **Stone of Wisdom** `unlock door` produced **no output** where FD prints
`(using the big skeleton key)` / `You unlock the steel door with the big skeleton
key.`, then every later command (open/lock/sleep/window/rope/bridge…) played in
the wrong state — a 173-hunk cascade. Two independent root causes, both broad:

1. **SetTasks-Execute function/OO args were auto-capitalised** (the same class of
   bug as the earlier bare-`vnl_Dial` literal-key fix, but for `%fn[..]%` args).
   `unlock door` matches `UnlockObjectLazy`, whose action is `SetTasks Execute
   UnlockObWithKey (%object1%|%PropertyValue[%object1%,LockKey]%)`. `eval_arg_to_key`
   (`a5run.cpp`) evaluated the key-bearing arg `%PropertyValue[%object1%,LockKey]%`
   through `a5text_process`, whose display pipeline **auto-capitalises the first
   letter** → the door's `LockKey` value `s_SkeletonKe` became `S_SkeletonKe`, so
   `a5state_object_index` failed, object2 never bound, and `UnlockObWithKey`'s
   `ReferencedObject2 Must BeHeldByCharacter` silently failed (no output, no
   unlock). FD applies auto-cap only at Display time, never when resolving a
   reference. **Fixed** with a new `a5text_process_nocap` (`a5text.cpp`/`.h`:
   `process_inner` + `resolve_perspective`, no `auto_capitalise`); `eval_arg_to_key`
   now uses it for the function/OO arg path.

2. **`BeInState` consulted *appended* state-list properties.** Even with the key
   resolved, `UnlockObject`'s `SetProperty ReferencedObject1 OpenStatus Closed`
   ran, but the next `open door` still said "…as it is locked!". The door carries
   two StateList props: `OpenStatus` (states Open/Closed, the value that holds
   "Locked" when locked, **no `AppendTo`**) and `LockStatus` (state Locked,
   **`<AppendTo>OpenStatus</AppendTo>`** — a child pseudo-state). Scarier's
   `BeInState` (`a5restr.cpp`) iterated **all** of the object's properties, so the
   never-changed `LockStatus="Locked"` kept `BeInState Locked` true forever. FD's
   `BeInState` (clsUserSession.vb:4253) only matches a property that is
   `Type=StateList` **and** `AppendToProperty=""`. **Fixed** by parsing the
   propdef `<AppendTo>` (new `a5_propdef_t.append_to`, `a5model.cpp`/`.h`, + a
   `a5model_propdef` lookup) and skipping non-StateList / appended properties in
   `BeInState`, so unlocking (OpenStatus→Closed) clears the "Locked" state even
   though the child `LockStatus` still reads "Locked".

**Result:** the whole Stone-of-Wisdom Manager's-office → window → rope →
bamboo-bridge → forest-net chain now plays. **StoneOfWisdom 173→43**,
**anno1700 67→6** (its lock/open + many SetTasks-Execute `%fn%` args were hit by
the same auto-cap bug), **LostChildren 5→4**. No regressions; all a5 unit tests
pass; baselines re-blessed. SoW residual 43 = troll-attack RNG message picks
(3), a window-counter off-by-one, and the **Pamba "yes" trade** conversation
(`say yes`/`(to Pamba)`) + its downstream coin/navigation desync — a separate
conversation-puzzle family, no longer the bamboo bridge.

---

## Conversation state must end when the Player walks away from the partner (broad)  ✅ DONE (2026-06-29)

**Was:** **Stone of Wisdom** `say yes` to Pamba at the weapon stall printed `The
stall owner waits patiently for your answer.` where FD completes the trade
(`(to Pamba)` / `"I am happy to trade with you!"` … +1 gold coin). Same shape in
**Anno 1700** (Lulu-Belle's greeting was deferred to the wrong turn).

**Root cause — `conv_char` was never cleared on movement.** ADRIFT's `say %text%`
splits into `Say` (priority-higher, `Must BeInConversationWith AnyCharacter`,
routes `SayToCharacter (%text%|%convcharacter%)`) and `SayLazy` (`MustNot
BeInConversationWith AnyCharacter` **and** `Must BeAloneWith AnyCharacter`, routes
`SayToCharacter (%text%|%AloneWithChar%)`). At the stall the player is *alone with*
Pamba but **not** in conversation, so FD takes `SayLazy` → `SayToCharacter
(yes|Pamba)` → the Specific override `SayYesAtWe` (Text `yes`, Character
`s_Pamba`) fires the trade. Scarier's `st->conv_char` was still set from an
*earlier* conversation (it is only ever set, never cleared on a room change), so
`BeInConversationWith AnyCharacter` wrongly passed, `Say` claimed the turn, and
`SayToCharacter (yes|%convcharacter%)` dispatched against the stale/empty
character — the override never matched and the generic "waits patiently" message
showed. FD clears it in `clsCharacter.Move` (clsCharacter.vb:535-545): after the
Player moves, if the conversation partner is no longer at the Player's location,
show an implicit Farewell topic and reset `sConversationCharKey`/`Node`.

**Fixed** in `a5run.cpp` with `clear_conv_if_partner_gone(run, out)` (mirrors that
block: implicit `A5_CONV_FAREWELL` via `find_conv_node`/`emit_conv`, then
`a5state_set_conv_char("")`/`a5state_set_conv_node("")`), called after the Player's
location updates in both the `MoveCharacter InDirection` and `ToLocation` branches.

**Result:** SoW's Pamba trade completes (the gold coin is obtained) and anno1700's
Lulu-Belle greeting lands on the right turn — **anno1700 6→5**, no other game
regressed; all a5 unit tests pass. **StoneOfWisdom 43→44** (re-blessed): the now-
working trade exposes the *next* pre-existing blocker — bare **`put all in
backpack`** doesn't narrow "all" to held items (see below), so the player keeps
holding the coins and `climb tree` is refused ("…while you are holding something"),
desyncing the village navigation. Strictly more correct (trade + greeting fixed).

---

## Bare `all` narrowed by task restrictions (broad)  ✅ DONE (2026-06-30)

**Was:** **Stone of Wisdom** `put all in backpack` → `Sorry, I'm not sure which
object you're referring to.` where FD prints `You put the silver coin and the gold
coin inside the backpack.` (the SoW gating divergence after the Pamba trade fix;
also blocked the subsequent `climb tree`).

**Root cause:** `resolve_plural` (`a5run.cpp`) already narrowed a bare `all`/list
to the restriction-passing items (binds each candidate as `objects`, keeps those
that pass `a5restr_pass(t->restrictions)`), but `resolve_refine`'s **final
whole-set re-check** (the `a5restr_pass` after the `plural_idx` block) then ran the
task restrictions with `ReferencedObjects` bound to the `"k1|k2|..."` pipe — which
`a5restr`'s `resolve_key`→`a5state_object_index` couldn't resolve, so
`ReferencedObjects Must Exist`/`BeHeldByCharacter` → 0 and the whole multi-object
command failed. (Single-object commands and `get all` whose restrictions reference
the SINGULAR `ReferencedObject` were unaffected — that pipe was never bound there.)

**How FD does it:** `RefineMatchingPossibilitesUsingRestrictions`
(clsUserSession.vb:5734) narrows the `all` reference per-item against the
restrictions, then the post-refine `PassRestrictions` (vb:6057) resolves
`ReferencedObjects` via `GetReference` (vb:3990), which returns
`Items(0).MatchingPossibilities(0)` — i.e. only the **first** narrowed item.
`%TheObjects[%objects%]%` (the completion message) builds a temp `ObjectHashTable`
from the split pipe keys and renders `htblObjects.List` — a **definite-article
"the a, the b and the c"** list (StronglyTypedCollections.vb:183, Global.vb:2056/
2386).

**Fixed** (both faithful to FD, verified clean across the corpus):
1. **`a5restr.cpp resolve_key`** — a bound value containing `|` resolves to its
   first pipe-segment (the stable model-key pointer), mirroring `GetReference`
   returning `Items(0)`. Per-item narrowing already happened in `resolve_plural`,
   so checking only the first item in the final pass is exactly FD's behaviour.
2. **`a5text.cpp`** — `%TheObjects[...]%` / `%TheObject[...]%` (and bare `%objects%`)
   with a piped multi-key arg now render the article list via the new
   `list_objects_art` (definite for `The*`, indefinite for `A*`, none otherwise),
   splitting the pipe with the existing `arg_object_keys`. Single-key args keep the
   old single-name path. No pipe leaks.

**Result:** **StoneOfWisdom 44→5** (`put all in backpack` → the two coins, then
`climb tree` succeeds — the whole post-trade village chain plays). **RunBronwynn
stays 9** (`remove tiara, gown and shoes` now renders `Ok, you remove the diamond
tiara, the blue ballgown and the high-heel shoes.`; FD trims the gown into a
separate `<Multiple>1</Multiple>` per-item override — see residual below — but the
net hunk count is unchanged). No other corpus game moved; all a5 unit tests pass
(a5run/save/arith/parse/dis/walk/obj); budgets re-blessed.

**Residual (c) — `<Multiple>1</Multiple>` per-item specific overrides:** FD's
multi-object remove peels the ballgown out to its `RemoveBall` Specific override
(restriction `Player Must BeInPosition Standing` → "You need to be standing up to
do that."), running the general `RemoveObjects` only over the other two — Scarier
runs the general over all three. Same mechanism gates Jacaranda's buzzard
(`Task62`, `DropObjects` override Multiple=1). Not implemented; tracked as its own
item. It does **not** affect SoW (no per-item override on `put`).

---

## MoveCharacter bulk source forms (EveryoneAtLocation/InGroup/Inside/On/...)  ✅ DONE (2026-06-30)

**Was:** Jacaranda's `wave wand` (in the police cell) printed the teleport prose
but left the player put — the colour-button room (`Location34`) stayed out of
scope, so `push pink button`/`x pink button` → "You see no such thing.", the
button puzzle never advanced, the player never reached the quarry, and the
buzzard-nest event never fired (so the inventory diverged for the rest of the
game).

**Root cause:** the wand action is `<MoveCharacter>EveryoneAtLocation Location33
ToLocation Location34</MoveCharacter>`. Scarier's `MoveCharacter` handler
(`a5run.cpp`) bailed unless `tk[0]=="Character"`, so every bulk "who" form was a
silent no-op (the same class as the P12 `MoveObject EverythingInGroup` gap). FD's
`MoveCharacterWho` set (clsUserSession.vb:1689) collects a `chars` set first
(`EveryoneAtLocation` = `CharactersDirectlyInLocation(True)`, i.e. directly at the
location incl. Player; `EveryoneInGroup`/`Inside`/`On`/`WithProperty`) then applies
the destination per character.

**Fixed** by porting that set: the handler now builds a character-index vector from
`(tk[0], tk[1])` — `Character`, `EveryoneAtLocation` (`char_loc==key &&
char_onobj==NULL`), `EveryoneInGroup`, `EveryoneInside`/`EveryoneOn`
(`char_onobj==key`, `char_in` flag), `EveryoneWithProperty` — and applies the
existing per-character destination switch (`InDirection`/`ToLocation`/`ToStanding/
Sitting/LyingOn`) to each. Token layout matches `MoveObject`, so the destination
parsing is unchanged.

**Result:** the wand teleport works, the buttons push, navigation reaches the
quarry, and the buzzard fires (inventory aligns to within one item). This makes
Jacaranda play far deeper, which is why **JacarandaJim 109→271** (re-blessed):
**247 of the 271 hunks are AFTER the champagne event** — `Task67`'s `MoveCharacter
%Player% ToLocationGroup Group7` teleports to a *random* room, so its whole-game
tail cascade is irreducible under vanilla RNG (Scarier and FD pick different Group7
rooms; would align under `FD_RNG=xoshiro`). The other ~24 are the catalogued
Alan-follower / rain-timing RAND residual. No other game moved; rendering verified
clean (no pipe leaks, correct "a, b and c" lists).

**Remaining JJ blockers (next):** `MoveCharacter ToLocationGroup` +
`%DisplayLocation[…]%`/`%LocationOf[…]%` text functions (the champagne teleport
prose + move), the **Alan follower** ("Alan is with me" presence), and rain-event
RAND timing.

---

## P8 — Y/N prompt text not printed  ✅ DONE/STALE (verified 2026-06-29)

Not a bug in the current tree. There is **no** engine Y/N wait-state in either
Scarier or FrankenDrift — the `"(Y/N)"` text is the game author's task output, and
`yes`/`no` are ordinary General tasks gated by a flag the question task sets.
StarshipQuest (golden, 0) and MagneticMoon both print `Do You Wish To Explore The
Ship First? (Y/N)` after `b` via the normal `emit_completion` path. The earlier
symptom was fixed incidentally by the completion/output-join work. (The Grandpa
dial *numeric* prompt is a genuinely different feature — see P6 residual.)

---

## Character `BeVisibleToCharacter` restriction always passed (broad)  ✅ DONE (2026-06-29)

**Was:** bare `talk` near Grandpa produced **no output** where FD prints `Grandpa
says "Remember to talk to Molly… her secret nickname is Kennedy."`. That one miss
cascaded: the hint sets `vnl_Kennedyhea=1`, which gates the kitchen's
`vnl_TutorialEx` "Since Molly the parrot is here…" LocationTrigger and the rest of
the tutorial chain, so ~9 GrandpasRanch hunks (the whole tutorial-line cluster)
all stemmed from it.

**Root cause — `pass_character` had no `BeVisibleToCharacter` case.** Grandpa's
three bare-`talk` tasks (`vnl_JustTalk`→Molly, `vnl_JustTalkMo`→Buster,
`vnl_JustTalkBu`→Grandpa) are each gated on `<Character>X Must
BeVisibleToCharacter %Player%` and dispatch via `SetTasks Execute vnl_TalkSpeakC
(X)`. The OBJECT sub-evaluator implemented `BeVisibleToCharacter` (a5restr.cpp:273)
but the CHARACTER one did not — so a `<Character>` subject fell through to the
best-effort `return 1` (pass). The first task, `vnl_JustTalk` (Molly), thus
*always* "passed" even at the patio; it claimed the turn (`RR_OK`) and ran
`Execute vnl_TalkSpeakC (vnl_Molly)`, whose own `Must HaveSeenCharacter` check then
silently failed (`return` with no output) — empty turn, and the real Grandpa task
never reached. Confirmed via `A5_TRACE_RUN=1`: the matched task was
`vnl_JustTalk … -> 1`.

**Fixed** in `a5restr.cpp pass_character`: added the `BeVisibleToCharacter` case
mirroring `clsRestriction.CharacterEnum.BeVisibleToCharacter`
(clsUserSession.vb:4700) → `clsCharacter.CanSeeCharacter`/`IsVisibleTo` (same
BoundVisible location, subject not Hidden). Reduces to "subject is at the
observer's location" via the existing `a5state_character_at_location` (which
resolves on/in-object subjects through their carrier and treats Hidden as
not-present). Handles ANYCHARACTER on either side; the common `%Player%` observer
uses `a5state_player_location`.

**Result:** **GrandpasRanch 11→2** (residual = the Buster-the-bull
position-tracking, P6-still-open) and **DieFeuerfaust 14→6** (same talk/visibility
gate). No other game changed; all a5 unit tests pass; baselines re-blessed.

---

## Character `BeCharacter` restriction always passed (broad)  ✅ DONE (2026-06-29)

**Was:** `x horse` (and `x scarecrow`) before reaching the horse printed the full
character description plus a "Your horse are wearing a halter…" line where FD says
**"You see no such thing."** (RunBronwynn lines 108-111, 377-379).

**Root cause — `pass_character` had no `BeCharacter` case.** RunBronwynn ships
`ExamineCha1` ("Examine Me", Priority 13 — very high), command `[examine/…] %character%`,
restriction `<Character>ReferencedCharacter Must BeCharacter Player`. The CHARACTER
sub-evaluator implemented no `BeCharacter` op, so the restriction fell through to the
best-effort `return 1` and the task fired for *any* examined character. `x horse`
resolved "horse"→Fleetwind, `ExamineCha1` "passed", and printed Fleetwind's
description (`%DisplayCharacter%`). FD's `ExamineCha1` correctly fails (Fleetwind ≠
Player), letting the next task — the higher-priority `ExamineCharacter` (50425),
whose `%Player% Must HaveSeenCharacter` fails because Fleetwind is in the unvisited
stable — claim the turn with "You see no such thing." Confirmed via `A5_TRACE_RUN=1`
(`ExamineCha1 … -> 1`).

**Fixed** in `a5restr.cpp pass_character`: added the `BeCharacter` case mirroring
`clsRestriction.CharacterEnum.BeCharacter` (clsUserSession.vb:4579) — `ANYCHARACTER`
⇒ pass, else identity (`streq(k1, k2)`) on the resolved keys, exactly like the object
`BeObject`.

**Result:** **RunBronwynn 12→9** (the horse/scarecrow examine cluster gone; residual
= the `remove tiara/gown/shoes` multi-object worn-removal and the P9/P3b
noref-vs-cantsee parser-wording family). No other game changed; all a5 unit tests
pass; baseline re-blessed.

---

## BUG 13 — Troll knockout death (Stone of Wisdom; turn-based event not disarmed)  ✅ DONE (2026-06-29)

**Root cause (CONFIRMED — not the tick/order theory below).** The knockout task
`s_AttackTheT` is a **Specific override** of `AttackCharacterWithObject`
(attack-troll-with-ring). `StartTroll` disarms via `Control: Stop Completion
s_AttackTheT` — keyed on the *override child*. Scarier's
`execute_task_with_overrides` marked a completed override child `task_done` but
**never called `ev_on_task_completed` for the child's key**, so its EventControls
(and WalkControls) never fired. The `Stop` thus never reached `StartTroll`, which
kept ticking its offset-0 `TrollFinis` subevent (`Trollrange == 1` → `EndGame
Lose`) the same turn. FD's `AttemptToExecuteSubTask` → recursive
`AttemptToExecuteTask` runs the control loop (clsUserSession.vb:865-908) for
*every* completing task, parent and child.

**Fixed** in `a5run.cpp`:
1. `execute_task_with_overrides` now calls `ev_on_task_completed (run,
   child->key, out)` after each passing override child runs (both the main
   override loop and the `After*` children), so a child's controls fire exactly
   like FD's. So `s_AttackTheT` Stops `StartTroll` before `ev_tick_all`, and
   `TrollFinis` never fires that turn — the troll stays knocked out, `x troll`
   yields the skeleton key, and the game plays on.
2. Ported FD's parent-vs-child control de-dup guard
   (`Not task.Children(True).Contains(e.sTriggeringTask)`,
   clsUserSession.vb:872/893): new `task_is_descendant` walks a task's
   `general_key` chain; `ev_on_task_completed`/`wk_on_task_completed` now skip a
   control whose event/walk `triggering_task` is a (recursive) override
   descendant of the completing task, so a parent doesn't re-fire a control its
   child already handled.

**Result:** StoneOfWisdom no longer dies at turn 10 — the full 137-turn
walkthrough now plays. **JacarandaJim 114→109** (its conversation/keyword
override children fire controls correctly now too). Two budgets *rose* as the
fix unblocked gameplay and exposed pre-existing downstream blockers (the same
"now-playable" pattern as P1/CorrectCommand):
- **StoneOfWisdom 6→173** — past the troll, Scarier hits a NEW blocker: the
  **bamboo-bridge collapse** sequence (FD: bridge collapses → fall animation →
  "land in a huge net" → forest eco-system; Scarier stays in the dark mine and
  every later command plays in the wrong room). Separate unimplemented
  multi-step fall event — track as its own item.
- **SpectreOfCastleCoris 16→17** — the noon-bell event (`s_Event1`, `Start
  Completion GoToTheWes`, where `GoToTheWes` is a PlayerMovement-West override
  child) now **rings at all** (it never did before, since the override child's
  control never fired); it lands ~6 turns off FD only because of a separate
  175-turn-countdown turn-counting imprecision. Net +1 (bell now present);
  strictly more faithful.

All a5 unit tests pass (Six Silver Bullets golden + save); budgets re-blessed.

**Still open (downstream of this fix):** ~~the **Stone of Wisdom bamboo-bridge
collapse**~~ ✅ RESOLVED — the real blocker was earlier (the steel-door `unlock`
auto-key chain), fixed by the SetTasks-Execute auto-cap + `BeInState` appended-
state fix at the top of this file (SoW 173→43; the window/rope/bridge/net all play
now). The **Spectre noon-bell ~6-turn offset** (175-turn-countdown turn-counting
precision) remains. SoW's new gating divergence is the **Pamba "yes" trade**
conversation.

---

## ContinueAlways must restrict to higher-priority + filter LowPriority tasks (broad)  ✅ DONE (2026-06-29)

**Was:** **Run Bronwynn Run** moved the player off the bed even though FD blocked
it. While sitting on the bed, `n` → Scarier `Perhaps it would be a good idea if
you were to get off the bed first?  You walk north.` (message **and** the move)
where FD prints only the get-off message and stays put. Every subsequent move
then desynced (Scarier roamed the castle; FD stayed on the bed) — a ~30-hunk
cascade.

**Root cause — Scarier's `<Continue>ContinueAlways` continuation ignored task
priority and the `LowPriority` flag.** The stock library splits movement into
`GetOffBeforeMoving` (priority 50432, `ContinueAlways`, prints the "get off X
first?" line when the player is on furniture) and `PlayerMovement` (priority
50434, **`<LowPriority>1</LowPriority>`**, the actual mover). FrankenDrift's
`AttemptToExecuteTask` runs a `ContinueToExecuteLowerPriority` task and then
re-enters selection via `EvaluateInput(Priority+1)` →
`GetGeneralTask(iMinimumPriority = Priority+1)`, where the per-task gate is
`tas.Priority >= iMinimumPriority AndAlso Not (tas.LowPriority AndAlso
tas.Priority > iPriorityFail)` with `iPriorityFail` forced to `iMinimumPriority`
(clsUserSession.vb:5971/5981). So on the continuation only **strictly
higher**-priority tasks are eligible, and a **LowPriority** task above the floor
is dropped — `PlayerMovement` (50434, LowPriority) is filtered out, so no move.
Scarier instead just `break`ed out of the matched ContinueAlways task and kept
scanning every later task in `run->order` (ascending priority, like FD's
`listTaskKeys`), so `PlayerMovement` ran and moved.

**Fixed** in `a5run.cpp scan_tasks`:
- new model field `a5_task_t.low_priority` (parsed from `<LowPriority>` via
  `a5xml_bool`, `a5model.cpp`);
- when a `continue_lower` task runs, record a continuation floor
  `cont_floor = its Priority`; subsequent tasks are skipped when
  `priority <= cont_floor` (the `iMinimumPriority` gate) or when
  `low_priority && priority > cont_floor + 1` (the `iPriorityFail` LowPriority
  gate). A second ContinueAlways task raises the floor.
- scan_tasks now returns **1** (turn handled) once any ContinueAlways task ran,
  so the empty continuation does **not** fall through to the caller's "Sorry, I
  didn't understand that command." (FD's NotUnderstood is gated on
  `iMinimumPriority = 0`, i.e. only the first pass).

**Result:** **RunBronwynnRun 45→12** — the move is correctly blocked and the
whole castle-traversal cascade is gone. No other game changed (only RunBronwynn's
corpus path exercises a ContinueAlways+LowPriority movement split); all a5 unit
tests pass (incl. the Six Silver Bullets golden and save); baseline re-blessed.
Residual 12 are unrelated multi-object/noun-resolution edge cases (`remove tiara,
gown and shoes` partial-worn removal; `x horse`/`x scarecrow` resolving to a
character + a spurious "...are wearing..." line where FD says "You see no such
thing.") — the P3/P9 family.

---

## Walking-NPC CharEnters/CharExits read only the runtime override (broad)  ✅ DONE (2026-06-29)

**Was:** a `ShowEnterExit` NPC walking through the player's room narrated the
generic `<Name> enters from <dir>.` / `<Name> exits to <dir>.` where FD uses the
character's custom walk text — JacarandaJim's postman: `The postman rollerskates
towards me from the north.` / `The postman trundles away on his rollerskates to
the east.` Recurs on every postman move (~10 hunks).

**Root cause:** `wk_show_enter_exit` (`a5run.cpp`) read `CharEnters`/`CharExits`
via `a5state_entity_prop` only — the **runtime override** store — so the
postman's **static** `<Property>` values (each a rich `<Description>`/value_node
holding `rollerskates towards me` / `trundles away on his rollerskates`) were
never seen and the code fell back to the `enters`/`exits` defaults. FD's
`clsCharacter.DoAnySteps` (clsCharacter.vb:1537-1565) uses
`If .HasProperty("CharEnters") Then s = .GetPropertyValue(...)`, which reads the
merged (inherited + own) property set incl. statics.

**Fixed:** new `char_prop_value(st, charkey, propkey)` mirrors `GetPropertyValue`
— runtime override first, else the static prop (rendering a value_node via
`a5text_eval_description`, else the scalar), returning NULL only when the
property is genuinely absent so the `enters`/`exits` default still applies. Both
enter/exit sites use it.

**Result:** JacarandaJim **124→114**; no other game changed (only JJ's postman
carries these props in the corpus); all a5 unit tests pass; baseline re-blessed.
JJ residual is now almost entirely RNG-driven (`%AlanRemarks%` follower remarks,
rain-event timing) plus a `put down` multi-object divergence.

---

## P12 — Malformed bracket sequence + specific-override completion (Anno secret-passage cascade)  ✅ DONE (2026-06-29)

**Was:** Anno 1700 desynced from `open closet` onward (127-hunk cascade): Scarier
printed the over-specific `OpeningClo2` task's "You open the closet door." where
FD runs the general open task ("You open the tall closet.  Inside the tall closet
is a business suit, …"). Every downstream secret-passage step (`push wall`, …)
then diverged. Three independent root causes, all broad:

1. **Malformed `<BracketSequence>` passed instead of failing.** `OpeningClo2`
   ships `##A#` (3 `#` placeholders, only 2 restrictions). FrankenDrift's
   `EvaluateRestrictionBlock` (clsUserSession.vb:5190) consumes the first `#`,
   finds `#` where an operator is expected, takes the inner `Select Case … Case
   Else` (no `PassSingleRestriction` call) and **falls off the end returning the
   VB Boolean default `False`** — so the whole block FAILS and the task is
   skipped. Scarier's `eval_block` (`a5restr.cpp`) returned the *first* term's
   result (PASS) for the unexpected-operator case and `1` (PASS) for a block not
   starting with `(`/`#`. **Fixed:** both malformed paths now return `0` (False),
   mirroring FD's fall-through. Verified Jacaranda/others have **no** such
   malformed sequences (their `[`/`]` ones expand to balanced `((`/`))`), so the
   change only affects genuinely-broken sequences — strictly more conformant.

2. **`MoveObject` only handled the single-`Object` source.** Anno moves the
   player's clothes into the closet via `MoveObject EverythingInGroup PlayerBusi
   InsideObject Closet`; Scarier's handler bailed unless `tk[0]=="Object"`, so the
   clothes were never placed and the open task's "Inside … is %ListObjectsIn%"
   description (gated on `AnyObject Must BeInsideObject ReferencedObject`)
   produced nothing. **Fixed** in `a5run.cpp run_action`: ported
   clsUserSession.vb:1479's `MoveObjectWhat` set — `EverythingInGroup`
   (static members ∪ runtime `@InGroup`), `EverythingHeldBy`/`WornBy`,
   `EverythingInside`/`On`, `EverythingAtLocation`, `EverythingWithProperty` —
   collecting the affected object indices then applying the existing per-object
   destination switch (now also `ToPartOfObject`/`ToPartOfCharacter`).

3. **Specific *override* sub-tasks were never marked Completed.** `run_general`
   ran a matching child override via `run_task` but never set `task_done`, so
   `<task> Must BeComplete` restrictions referencing it stayed false — Anno's
   `PullHookIn` (a Specific override of `PullObjects`) ran ("A faint click…") but
   `PushingBri`'s `PullHookIn Must BeComplete` failed, so `push wall` kept
   printing the failure line. **Fixed** by mirroring FD's
   `AttemptToExecuteSubTask` (clsUserSession.vb:1193 `task.Completed = True`): the
   override child is marked `task_done` after it runs, **and** skipped when it is
   already complete + non-repeatable (FD's `AttemptToExecuteTask` entry guard,
   vb:730 `If task.Completed AndAlso Not task.Repeatable Then Return False`) so it
   doesn't re-fire and the parent runs instead.

**Result:** anno1700 **127→98**, **SpectreOfCastleCoris 34→16**, DieFeuerfaust
15→14, GrandpasRanch 45→39. JacarandaJim 442→**446** is pure RNG/event-timing
realignment (its rain + `%AlanRemarks%` RAND draws shift with the corrected
event/task timing). All a5 unit tests pass; baselines re-blessed.

---

## P1 — Event/timer scheduler fires too early  ✅ DONE (2026-06-29)

**Was:** turn-based events appeared to fire on the intro/options pages and far
before their trigger. The *actual* two root causes were both in how events/tasks
are armed at game start — not menu-page turn-consumption:

1. **`<WhenStart>0</WhenStart>`** — some events serialise WhenStart numerically
   (e.g. Axe of Kolt's "Xixon On Path" Event30). FrankenDrift loads this with
   `[Enum].Parse`, so 0 is a valid-but-unnamed enum value that matches **no**
   case in the game-start init `Select Case` (clsUserSession.vb:231) → the event
   stays `NotYetStarted`, armed only by its Start control. Scarier's model parser
   collapsed any non-`BetweenXandYTurns`/`AfterATask` value to the
   `Immediately` (=1) default, so the event started at load and its 1-turn
   subevent (ExecuteTask → death) fired on turn ~1.
   **Fixed** in `a5model.cpp a5_parse_event_body`: parse the numeric form
   (`0`/non-1-2-3 ⇒ that integer). `ev_init`'s switch already no-ops for it.

2. **`<LocationTrigger>` System tasks not run** — chapter/scene transitions are
   `Type=System` tasks with a `<LocationTrigger>` (e.g. Axe's Task3 "Wagon at
   Start" moves the Player to the Crossroads, Task6 "Haywain Goes" prints the
   haywain line). Scarier had **no** System-task auto-execution, so the player
   was stranded on the blank Chapter-card location and movement failed.
   **Fixed** by porting `clsCharacter.Move` (clsCharacter.vb:506): a Player move
   into a location enqueues every still-runnable System task whose
   `LocationTrigger` is that location onto a new `run->tasks_to_run`
   (= `clsAdventure.qTasksToRun`), drained in `drain_tasks_to_run` at the top of
   `ev_tick_all` (after the turn's task, before `TurnBasedStuff`). Model gained
   `a5_task_t.location_trigger`.

**Result:** Axe survives past `b` to "Crossroads"; Starship / Die Feuerfaust /
Run Bronwynn no longer hit a premature `[GAME OVER]`; Spectre no longer teleports.
Revenge 139→53 and Amazon 230→45 dropped sharply. The premature-death games'
budgets *rose* (Axe 1→72, Spectre 33→96, Starship 3→15, DieFeuerfaust 27→40, Run
Bronwynn 5→47) only because they now play the whole script and expose the
downstream P2/P9 divergences — those are the remaining work. Baselines re-blessed
in `test/run_a5_walkthroughs.sh`; full corpus is green (no FAILs). All a5 unit
tests (`a5runtest`, `a5savetest`, …) still pass.

**1b — RESOLVED by this same P1 LocationTrigger work (verified 2026-06-29).**
Lost Children's missed first-turn event (lost broadsword/money) is task
`StartMessa` (`Type=System`, `LocationTrigger=ByTheMegli`, `DisplayOnce`); `b`
moves into By-The-Megalith → `enqueue_loc_trigger_tasks` queues it →
`drain_tasks_to_run` (top of `ev_tick_all`) runs it that turn — it now prints at
the same line in both engines. The stray `Follow Cancelled.` was task
`FollowBoys3` (System, **no** LocationTrigger); the strict `streq(location_trigger,
new_loc)` gate means it can no longer be enqueued by a move, so it no longer
leaks. `ev_tick_all` order (drain → walk tick → event tick) already matches FD's
`ProcessTurn`/`TurnBasedStuff`. LostChildren residual = BUG 9 parser-wording, not
1b. No code change needed.

---

## End-of-game text  ✅ DONE (2026-06-29)

**Was:** the harness printed a bare `[GAME OVER]` stub when `a5run_is_over`; the
engine emitted nothing on EndGame, and the EndGame action's enum token
(`Win`/`Lose`/`Neutral`) was stored in `st->end_message` but never used.

**Fixed** by porting `clsUserSession.CheckEndOfGame`: `emit_endgame`
(`a5run.cpp`, called from `finish_turn` the first turn `game_over` is set) now
prints the `*** You have won/lost ***` banner (Neutral ⇒ none), the optional
`<EndGameText>`/WinningText (new `a5_adventure_t.end_game_text`), the
`In that game you scored S out of a possible M, in T turns.` line when MaxScore>0
(S/M read from the `Score`/`MaxScore` variables), and the
`Would you like to restart, restore a saved game, quit or undo the last command?`
prompt.  Added `a5_state_t.turns` (bumped once per processed command, mirroring
the Runner's per-command `Adventure.Turns`; the score line shows `turns-1`, the
count *before* the ending command) and `a5_state_t.end_displayed`; both
save/restored.  Harness `[GAME OVER]` removed.

**Result:** where the game ends at the same point as FD the banner now matches —
MagneticMoon 34→32, Amazon 45→44.  Where Scarier still ends at the *wrong* turn
the richer block surfaces that pre-existing timing bug rather than hiding it:
StarshipQuest 15→16 (Scarier plays one extra turn before the hyperspace-death
event ⇒ "in 53 turns" vs FD "52"), RunBronwynn 47→49 (Scarier ends prematurely;
FD plays on).  Budgets re-blessed; all a5 unit tests pass.

---

## Turn timer — "can't see" reference must not tick TurnBasedStuff  ✅ DONE (2026-06-29)

**Was:** turn-based event countdowns drifted *early*. StarshipQuest's hyperspace
death fired ~8 turns too soon (game ended "in 44 turns" vs FD's 52); Magnetic
Moon's timed "took too long" death fired at turn 213 vs FD's 301.

**Root cause:** an out-of-scope object reference ("You can't see any X!") was
ticking the event/walk timer when it must not. In FrankenDrift, an unresolvable
reference leaves `GetGeneralTask` returning `Nothing`, so `ProcessTurn`
(clsUserSession.vb:3436) takes the `sTaskKey Is Nothing` branch — it prints the
"can't see" message but **never** calls `TurnBasedStuff()`. `TurnBasedStuff`
runs only when a task actually matched *and* produced output *and* was not a
System task (`If sOutputText <> "" … If Not bSystemTask Then TurnBasedStuff()`).
A matched-but-failing task (Scarier's noref / have_fail paths) still ticks; only
the *no task matched* cantsee path doesn't. Scarier had been calling
`ev_tick_all` in its cantsee-emit path.

**Verified empirically** by instrumenting FD (`FD_EVTRACE`): the non-ticking
StarshipQuest commands were exactly the "You can't see any X!" ones (plus the
`NotUnderstood` "I don't understand what you want to do with X", which Scarier
already didn't tick).

**Fixed** in `a5run.cpp a5run_input`: dropped the `ev_tick_all` call from the
`have_amb && amb_cantsee` block. StarshipQuest 9→1 (death now at FD's exact "52
turns"), Revenge 49→27. MagneticMoon 27→**29**: its death moved 213→276 turns
(FD 301) — much closer; the +2 is realignment noise in the unwinnable flailing
tail (both engines fail every command). The residual 276-vs-301 gap is the
**separate, unfixed noref-vs-cantsee task-selection mismatch**: Scarier resolves
some commands via a no-ref task (ticks) where FD takes the no-task cantsee path
(no tick). That mismatch is the same P9/P3b object-reference-resolution issue
(Scarier says "Sorry, I'm not sure which object you are trying to get." where FD
says "You can't see any flashlights!"). Fixing it would converge MagneticMoon
further. All a5 unit tests pass; budgets re-blessed.

---

## End-of-game banner spacing  ✅ DONE (2026-06-29)

**Was:** when a game ends via a turn-based **event** (StarshipQuest's hyperspace
death, MagneticMoon's "took too long" timer) rather than a command, the death
text was emitted with a single trailing newline and the `*** You have won/lost ***`
banner abutted it — FrankenDrift always shows a blank line before the banner.

**Root cause:** FD's `CheckEndOfGame` Displays the banner via `pSpace`
(Global.vb:568 → clsUserSession.Display:310), so a paragraph break always
precedes it. Command-ending games' last response already ends in a blank line, so
Scarier matched those; event-ending games didn't.

**Fixed** in `a5run.cpp emit_endgame`: before the banner, count the trailing
newlines already in `out` (skipping `\r`/spaces) and top them up to a blank-line
separator (`nl<2 ⇒ add "\n\n"`/`"\n"`).

**Result:** StarshipQuest 1→**0** (now a golden `test/StarshipQuest_expected.txt`),
AxeOfKolt 56→54, MagneticMoon 14→13. RunBronwynn 46→47 and Amazon 41→42 each rose
by one line-shift artifact only (both emit the banner at the *wrong* point —
RunBronwynn ends prematurely, Amazon misses the P2b Date/Time lines and never
reaches the real win — so FD has no banner there and the extra blank just splits a
hunk). All a5 unit tests pass; budgets re-blessed.

---

## Command bracket normalisation (CorrectCommand)  ✅ DONE (2026-06-29)

**Was:** bare-direction movement (`N`, `U`, …) and commands like `look around`
emitted "Sorry, I didn't understand that command." in games whose direction/verb
tasks use the optional-with-space syntax `{[go] {to {the}}} %direction%` /
`[look] [around] {me/you/myself/yourself}`. **Jacaranda Jim** (every move failed),
**Amazon** (movement), and broadly.

**Root cause — Scarier never applied `clsUserSession.CorrectCommand`.** FD rewrites
every task command at game-start init (clsUserSession.vb:211, `ProcessBlock`/
`GetSubBlock`/`ContainsMandatoryText`): an optional group `{x} y` becomes `{x }y`
and `{a/b}` becomes `{ [a/b]}`, *moving the space adjacent to the optional group
inside it* so that space is itself optional.  So `[look] [around] {me/...}` →
`[look] [around]{ [me/...]}` → regex `(look) (around)( (me|...))?`, which matches a
bare "look around"; likewise the bare direction matches `{[go] …} %direction%`.
The raw blorb XML stores the *un*-corrected form (verified: FD's `<Command>`
InnerText == Scarier's a5dump bytes; FD transforms it at load), and Scarier had
been faking the effect with a single `")? " → ")? ?"` relaxation in
`a5parse.cpp convert_to_re`.

**Fixed:** ported `CorrectCommand` verbatim as `a5_correct_command` (+`cc_*`
helpers, `a5model.cpp`), applied to every task command at load (the collected
pointers alias the XML tree, so each is replaced with an owned bracket-corrected
copy, freed in `a5model_free`).  Verified character-for-character against FD via
an instrumented `CorrectCommand` dump (only diff = `%object%` vs `%object1%`,
which Scarier normalises later).  **Removed** the `")? "→")? ?"` hack so the
matcher mirrors FD's `ConvertToRE` exactly (the two would otherwise double-relax
and over-match).

**Result:** AxeOfKolt 50→42, SpectreOfCastleCoris stays 34, Revenge 7→5,
LostChildren 359→354, StoneOfWisdom 5→3; StarshipQuest still a clean golden;
MagneticMoon/RunBronwynn back at baseline (the hack removal undid its
over-matching there too).  **Amazon 34→65 and Jacaranda 147→450 ROSE** — without
CorrectCommand their bare-direction moves silently failed so Scarier was stuck
near the start; now it traverses the whole game and the transcript exposes the
pre-existing **darkness** bug (new item below) plus Amazon's P2b Date/Time
placement — the same "now-playable, downstream bugs visible" rise as P1.  All a5
unit tests pass; budgets re-blessed.

**Harness note:** FrankenDrift.Headless has a per-turn 10s wait-loop that can
*truncate* its transcript on a slow game (Axe FD takes >1 min) under system load;
the truncated output then poisons `test/.fd_cache`, inflating that game's hunk
count on the next run (seen as Axe 42→48 flapping).  The true value is the
`FD_NOCACHE=1` number.  Not a Scarier bug; consider validating FD output isn't
truncated before caching.

---

## DARKNESS — dark locations show full room text instead of "It is too dark…"  ✅ DONE (2026-06-29)

**Was:** in a dark location Scarier printed the full room description; FD prints
the game's darkness line (e.g. Jacaranda's "It is too dark to make anything out
clearly.").

**Root cause — two parts, both confirmed:**
1. **The darkness line is the stock `Look` task's CompletionMessage second
   description**, not engine: `%Player%.Location.Description` (= the room view)
   followed by a `StartDescriptionWithThis` description gated on `%Player% Must
   BeWithinLocationGroup DarkLocations` AND `%Player% MustNot
   BeInSameLocationAsObject LightSources`.  `clsDescription.ToString`'s
   StartDescriptionWithThis **replaces** the whole view when those pass.  Movement
   runs it via `PlayerMovement`'s `<Actions>` `SetTasks Execute Look`.  Scarier
   shortcut `Execute Look` (and the `run_task` Look path) straight to
   `a5text_view_location`, bypassing the override.
   **Fixed** with `emit_look` (`a5run.cpp`): render the room view exactly as
   before (NOT back through the text pipeline — that perturbs whitespace in the
   no-override common case and regressed anno1700/RtC when first tried), then walk
   any *further* Look-CompletionMessage descriptions and apply each passing one's
   DisplayWhen (the darkness override → StartDescriptionWithThis replaces).  Both
   Look sites now call it.
2. **`BeInSameLocationAsObject` didn't expand an object GROUP.** The light-source
   check references the `LightSources` *group*; Scarier's `a5state_object_index`
   returns −1 for a group key ⇒ predicate false ⇒ `MustNot` wrongly passed ⇒
   darkness shown even while holding the torch.  **Fixed** in `a5restr.cpp`: if
   the key is an Objects-group, the predicate is true when **any** member is in
   scope (`a5state_object_at_location`), mirroring `clsCharacter.CanSeeObject`'s
   group expansion.

**Result:** Jacaranda's tunnel/Alan's-Den render "It is too dark…" before the
torch and the normal room after `get it` (matches FD line-for-line).
JacarandaJim 450→449, Amazon 65→64 (the bulk of each game's remaining hunks is
downstream desync — rain timing, the `%AlanRemarks[...]%` array template P10, the
"Alan is with me" follower, postman move-message wording — not darkness).  No
regressions; all a5 unit tests pass; budgets re-blessed.

**Build-recovery note:** this session also restored `a5_correct_command`
(P3 CorrectCommand) — its *definition* (+`cc_get_sub_block`/`cc_contains_mandatory`/
`cc_process_block`) had been lost (declared in `a5parse.h`, called from
`a5model.cpp`, but undefined → link error).  Re-ported verbatim from
clsUserSession.vb:6126-6295 into `a5parse.cpp`; `a5parse_test.cpp`'s `expect` now
bracket-corrects the pattern first (mirrors the load flow), fixing the bare-"se"
movement case.

---

## Output joining — FD's pSpace model (broad; the actual biggest hunk inflator)  ✅ DONE (2026-06-29)

**Was:** Scarier separated every message within a turn with a single `\n`, so a
task's completion message and a following turn-based **event**/walk/sub-task
message landed on **two lines** where FrankenDrift joins them on **one** with two
spaces — e.g. Jacaranda `Ok, I pick up the bottle of milk.\nThe postman…` vs FD
`Ok, I pick up the bottle of milk.  The postman…`, and Amazon's `Date: … Time: …`
lines printed one line off from FD throughout.

**Root cause:** FrankenDrift's `Display` accumulates output through
`Global.pSpace` (Global.vb:568): before appending the next chunk it adds **two
spaces** *unless* the buffer is empty or already ends in a newline (vbLf).  So a
message that ends in text space-joins the next; one whose XML `<Text>` ends in a
newline forces a line/paragraph break.  Completion text is taken verbatim
(`clsDescription.ToString` → `FileIO.LoadDescription` `InnerText`, no trim;
`Display(sMessage)` adds no newline).  Scarier instead **stripped** each message's
trailing whitespace and appended its own `\n`, destroying the per-message
distinction.

**Fixed** in `a5run.cpp`: added `sb_pspace` (the pSpace rule) and `msg_has_output`
(FD's `bHasOutput` — a whitespace-only render produces nothing and is dropped).
Every message emitter (`emit_completion`, `emit_look`, `emit_conv`, the
event/walk `DisplayMessage`/enter-exit lines, the run_general fail message, the
`get all` FailOverride) now does `if (msg_has_output) { sb_pspace; sb_puts(raw) }`
— no strip, no forced `\n`, so the message's own trailing newline drives the
break exactly as FD's.  `finish_turn` trims only the turn's *tail* and re-adds a
single `\n` (the dump harness supplies the blank line; the ground-truth diff
`norm`s trailing whitespace/blank runs anyway, so this is cosmetic).
`a5_disambig_test`'s `check_exact` now ignores one trailing newline (a5run_input
ends every turn with one).  Six Silver Bullets golden regenerated (join collapses
a few two-line pairs; verified byte-equal to FD after `norm`, modulo the
pre-existing RNG epigraph/dream picks).

**Result:** **Amazon 64→6** (the ~28 Date/Time placement hunks were all this
join), **JacarandaJim 449→442**, **anno1700 131→127**, **DieFeuerfaust 16→15**,
**LostChildren 6→5**, **StoneOfWisdom 3→2**.  RtC 141→**143** (+2 line-shift noise
inside its known x-desk cascade — it actually *gained* a correct blank line).  No
other regressions; all a5 unit tests pass; budgets re-blessed.  Amazon's residual
6 are real bugs: title centring, a clock day-rollover (`Time: 30:45` vs `06:45`),
and the dominant `shoot jaguar` combat death (Scarier "not sure which character"
→ dies; FD shoots it and plays on to `*** You have won ***`).

---

## Auto-capitalisation after an ellipsis (broad)  ✅ DONE (2026-06-29)

**Was:** Scarier capitalised the first letter after the last dot of an ellipsis —
Anno's `Question is... will the fuse work?` rendered as `...Will the fuse...`, and
`Ow you say ... Traduire ehh translate` instead of FD's `ow you say ... traduire`.

**Root cause:** `is_sentence_start` (`a5text.cpp`) treated any `.`/`!`/`?` + space
as a sentence boundary. FrankenDrift's auto-cap regex
(`Global.vb:539`, `[a-z][\.\!\?] ( )?(?<cap>[a-z])`) requires a **lowercase letter
immediately before** the stop, so the char before the *last* dot of `...` (another
dot) fails the match and the next letter is left lower-case. The in-place
left-to-right pass already replicates FD's other quirk (once a letter is capped,
a following `[a-z]`-before-stop check sees the now-uppercase letter).

**Fixed** by adding the `A5_IS_LOWER(char-before-stop)` guard to both the one- and
two-space branches of `is_sentence_start`. anno1700 98→96; no other game changed;
all a5 unit tests pass; baseline re-blessed.

---

## "Also here is" list separator + DisplayOnce early-return (anno1700)  ✅ DONE (2026-06-29)

Two more anno1700 room-rendering conformance bugs, both broad:

1. **`"  Also here is "` hard-coded 2-space prefix.** FD's `clsLocation.ViewLocation`
   (clsLocation.vb:134) does `pSpace(sView)` before the general-listed object list —
   so when the room description ends in a newline the list starts a fresh line with
   **no** leading spaces (Anno's "...centuries.\nAlso here is an old brick."). Scarier
   always prepended `"  "`. **Fixed** in `a5text_view_location`: apply the pSpace rule
   (2 spaces only if the buffer is non-empty and doesn't already end in `\n`).

2. **DisplayOnce description segment must return even when its restrictions fail.**
   `clsDescription.ToString` (Global.vb:3928) runs `Return sb.ToString` inside
   `If .DisplayOnce` *unconditionally* — it only sets `Displayed=True` when the
   restrictions passed (`bDisplayed`). So a not-yet-shown DisplayOnce segment whose
   restrictions FAIL still **terminates** the description loop (later segments are
   skipped) yet stays un-retired for a future turn. Anno's Room 101 has two identical
   `AppendToPreviousDescription` "narrow opening" segments straddling a DisplayOnce
   faint segment; FD shows the line once because the failing faint segment returns
   before the second append. Scarier `continue`d past a restriction-failed segment,
   so it reached the second append and printed the line twice. **Fixed** in
   `a5text_eval_description`: on restriction failure, `break` (don't mark Displayed)
   when the segment is DisplayOnce, else `continue` as before.

anno1700 96→90; no other game changed; all a5 unit tests pass; baseline re-blessed.

---

## Task completion must be set BEFORE its actions run (broad)  ✅ DONE (2026-06-29)

**Was:** Anno's whole "dizziness / faint / wake in storage room" cascade (~70 hunks)
never fired — the player stayed in the hotel and every downstream command diverged.

**Root cause — completion-vs-actions ordering.** The teleport event `EffectOfTh`
is armed by an EventControl `Start Completion SettingOff`. The System task
`SettingOff` (restriction `DrinkWine Must BeComplete AND Translatin Must BeComplete`)
is `Execute`d as a `SetTasks` action of **both** DrinkWine and Translatin. Translatin
is the live trigger (the walkthrough drinks the wine *before* translating). But when
Translatin's `SetTasks Execute SettingOff` action ran, Translatin itself was **not
yet marked complete**, so SettingOff's `Translatin Must BeComplete` failed and the
event never started. FrankenDrift sets `task.Completed = True` (clsUserSession.vb:1193)
**before** `ExecuteActions` (vb:1194); Scarier marked `task_done` only in the callers
*after* `run_task` returned.

**Fixed** in `run_task` (`a5run.cpp`): mark the task's own `task_done` right after
the before-completion message and **before** the actions loop (and in the built-in
`Look` branch), mirroring FD's ordering. The existing post-`run_task` markings in the
override loops / scan / event / SetTasks paths are now redundant but harmless.

**Result:** SettingOff completes → EffectOfTh starts → the dizziness subevents and
the storage-room teleport all fire. **anno1700 90→67**, **RtC 143→142** (a Translatin-
shaped self-referential action there too); no other game changed, all a5 unit tests
pass, baselines re-blessed.

---

## Specific overrides not resolved on SetTasks-Execute / nested overrides  ✅ DONE (2026-06-29)

**Nested overrides (the trunk case): ✅ FIXED.** `get trunk` now shows FD's "It's as
heavy as sin..." (`GetAWooden` → `TakeObjectFromLocation` → `TakeObjects`).
`run_general`'s override-children loop was extracted into a recursive
`execute_task_with_overrides(run, parent, refs, depth, out)` (a5run.cpp): when a
passing Specific override child runs, it is re-entered so its *own* nested children
fire in its place (FD's `AttemptToExecuteSubTask` → recursive `AttemptToExecuteTask`).
Also landed the per-index ref fix: a new `ref_info_for_name` resolves `%object2%` to
`ReferencedObject2` (not the first `ReferencedObject`), and `bind_reference` no longer
clobbers the bare `ReferencedObject` when binding a higher-index reference.
**JacarandaJim 446→438.**

**SetTasks-Execute overrides (the powder case) + cannon puzzle: ✅ FIXED.** The
`SetTasks Execute` handler now dispatches through `execute_task_with_overrides`
(refs via `refs_from_bindings()`), so Anno's `get powder` re-dispatch fires
`PutSomeDry` — FD's "skull" message, moving gunpowder into the held skull.  The
downstream cannon chain (`LoadingCan1` "pour powder in cannon", the fuse/fire steps)
gates on `Player Must BeHoldingObject Gunpowder1`; that was the blocker, because
`PutSomeDry` puts the gunpowder *inside* the skull, not in the player's hands.  Root
fix: **`BeHoldingObject` is now transitive** — `a5restr.cpp char_holds_object`
recurses through held containers/supporters (InObject/OnObject → recurse on the
parent; HeldByCharacter terminates; WornByCharacter does NOT count), mirroring
`clsCharacter.IsHoldingObject` (clsCharacter.vb:895).  So gunpowder-in-held-skull
satisfies the holding check and the whole load-the-cannon sequence plays through.
**anno1700 stays at 67** (powder message now correct, cannon puzzle plays to the
door-blast + LeBeuf's Quarters), **GrandpasRanch 39→35**, no regressions.

**Symptom (RESOLVED):** ~~`get trunk`~~ ✅; ~~`get powder`~~ ✅; ~~cannon puzzle
"you're not holding any gunpowder"~~ ✅.

**Root cause — task execution that bypasses Specific-override children.** FD's
`AttemptToExecuteTask` (clsUserSession.vb:723) *always* resolves a task's Specific
override children (via `AttemptToExecuteSubTask` → recursive `AttemptToExecuteTask`,
vb:1104). Scarier only does so in `run_general` for the *directly command-matched*
parent. Two paths skip it, both running the flat `run_task`:
  1. **`SetTasks Execute <task>`** (`run_action`, a5run.cpp ~2235) runs the target
     via `run_task` with no child-override check. Anno's `get powder` matches
     `TakeObjects` → its `TakeObjectsFromOthersLazy` override does
     `SetTasks Execute TakeObjectsFromOthers (%objects%|%ParentOf%)`; the Priority-5
     child `PutSomeDry` (Specifics Gunpowder1+Keg, the "skull" message) overrides
     `TakeObjectsFromOthers` but never fires.
  2. **Nested overrides.** `GetAWooden` (the "heavy as sin" trunk message) is a
     Specific override of `TakeObjectFromLocation`, which is *itself* a Specific
     override of `TakeObjects`. `run_general` finds `TakeObjectFromLocation` as a
     child of `TakeObjects` and runs it via `run_task`, but never looks for
     `TakeObjectFromLocation`'s *own* children, so the grandchild `GetAWooden` is
     missed.

**Fix status:** DONE.  `execute_task_with_overrides` (recursive), per-index
`ref_info_for_name`/`ReferencedObjectN`, the `bind_reference` clobber fix, the
`SetTasks Execute` call site flipped to `execute_task_with_overrides`, and the
transitive `BeHoldingObject` (`char_holds_object`) that unblocked the cannon puzzle
are all live and verified green across the corpus + unit tests.

---

## Text-type Specific overrides not matched (broad; conversation/keyword tasks)  ✅ DONE (2026-06-29)

**Symptom (GrandpasRanch, JacarandaJim):** Grandpa's `say kennedy` gave the generic
"Molly … Sugar, sugar, sugar" response instead of the `vnl_SayKennedy` override's
"… Cherries! … saying her secret nickname Kennedy triggered a different response"
(which sets the score + `vnl_Secretmess`).  Same shape anywhere a `%text%`-keyed
Specific overrides a general task (SAY, password/keyword puzzles, conversation).

**Root cause:** `collect_refs`/`ref_info_for_name` resolved only
Object/Character/Direction/Number/Location references to a key; a **Text** reference
got `key = NULL`.  `refs_match_specifics` then compared a Text Specific's key
(e.g. `kennedy`) against `NULL` ⇒ never matched, so Text-type Specific overrides
were dead.

**Fix:** `ref_info_for_name` now maps a `text` reference to its bound
`ReferencedText` (resolve_refine binds `%text%` to the typed text), and
`refs_match_specifics` compares a Text Specific **case-insensitively** (mirroring
FD's `RefsMatchSpecifics`, which lower-cases both sides, clsUserSession.vb:634).
Entity-key Specifics stay exact.  **GrandpasRanch 35→… (see below); JacarandaJim
438→124** — JJ's conversation/keyword responses are almost all text-keyed Specifics,
so this was the single biggest systematic win.  Corpus green, all unit tests pass.

---

## SetTasks-Execute literal-key arg corrupted by auto-capitalisation  ✅ DONE (2026-06-29)

**Symptom (GrandpasRanch — unblocked the whole endgame):** `turn dial` produced no
output; `853` → "I did not understand the word 853"; the box never opened, so no
map → Buster puzzle / dig / win all failed (~16 hunks).

**Root cause:** `turn dial` → `TurnObjectOver`'s override `vnl_TurnDialSe` runs
`SetTasks Execute SetObjectTo (vnl_Dial)`.  `eval_arg_to_key` (a5run.cpp) evaluated
the **bare literal key** `vnl_Dial` by running it through `a5text_process`, whose
display pipeline **auto-capitalises the first letter** → `Vnl_Dial`.  That corrupted
key failed `a5state_object_index`, so `SetObjectTo`'s `ReferencedObject Must Exist`
restriction failed and (FD runs override children only inside `If bPass Then`, so) the
`vnl_SetDial` override — which prints the "Which number?" prompt and sets
`vnl_Typenumber=1` — never fired.  Without that flag the bare `853` (matched by
`vnl_SetDialNum`'s `%number%` command, gated on `vnl_Typenumber==1`) had no active
task and fell through to NotUnderstood.

**Fix:** `eval_arg_to_key` now returns a `%`-free arg **verbatim** (it is already an
entity key; only `%ref%`/`%fn[..]%` args need evaluation).  `turn dial` now drives the
full dial→box→map→Buster→dig→**WIN** chain (Scarier scores 15/15).  **GrandpasRanch
28→12**; no other game regressed (other `SetTasks Execute (...)` callers pass `%`-args).

---

## `%Player%.Parent` returned empty for a character  ✅ DONE (2026-06-29)

**Symptom (GrandpasRanch):** `(getting off  first)` (double space) where FD shows
`(getting off the chair first)` — the template `(getting off %Player%.Parent.Name
first)` rendered an empty `%Player%.Parent`.

**Root cause:** the a5expr character `Parent` branch called `parent_key`, which only
handles *objects* (`a5state_object_index`); for the Player it returned NULL → "".

**Fix:** `clsCharacter.Parent` is `Location.Key` (clsCharacter.vb:189), so the
character branch now returns the object the character is on/in (`char_onobj`) when
seated/contained, else the location.  GrandpasRanch 12→11; no regressions.

---

## Look-task actions skipped on `SetTasks Execute Look` (Grandpa tutorial lines)  ✅ DONE (2026-06-29)

**Symptom (GrandpasRanch):** FD's location-gated "Tutorial: …" lines (e.g. the
opening "The tutorial is here to teach new players…") were missing.  They are the
CompletionMessage of System task `vnl_TutorialSt`, which Grandpa's **`Look` task
runs from its `<Actions>`** (`SetTasks Execute vnl_TutorialSt`); the name-entry task
`vnl_NameTyped` reveals the first room via `SetTasks Execute Look`.

**Root cause:** Scarier's `SetTasks Execute Look` handler called `emit_look` (room
view only) and returned, skipping the Look task's `<Actions>` — whereas FD's
`ExecuteTask(Look)` is a full `AttemptToExecuteTask` that also runs them.

**Fix:** the `Execute Look` handler now runs the Look task's `<Actions>` after
`emit_look`, matching `run_task`'s Look branch.  **No-op for every other corpus
game — only Grandpa's `Look` carries actions** (anno/RtC etc. have none, so the
earlier room-render-through-pipeline regression does not recur).  GrandpasRanch
39→35→28 across this + the text-specific fix above.

---

## P2 — Room-rendering omissions (broad; biggest hunk inflator)

### 2a — exit lists  ✅ DONE (2026-06-29)
`Exits are north and down.` / `An exit leads south.` are now appended to every
location view. Implemented in `a5text_view_location` (`a5text.cpp`): when the
adventure's `<ShowExits>` is on, render the player's `.Exits.Count` /
`.Exits.List` (the existing a5expr direction-list path — DirectionsEnum order,
restriction-unchecked routes, lower-cased "a, b and c" join), `>1` ⇒ "Exits are
….", `==1` ⇒ "An exit leads ….", `0` ⇒ nothing — mirroring
`clsLocation.ViewLocation`. Added `a5_adventure_t.show_exits` (parsed in
`a5model.cpp`, default true) plus an `a5xml_bool` helper matching
`FileIO.GetBool` (only `True`/`1`/`-1`/`Vrai`); RtC has `<ShowExits>False</>` and
must stay exit-less (caught a regression when the bool was mis-parsed). Grandpa's
exits now match line-for-line; Jacaranda 439→433. Re-blessed.

### 2b — movement confirmations + status line  ✅ DONE (2026-06-29)
- **Root cause (confirmed): `run_general` ran only ONE Specific override.** The
  "standard-library merge" hypothesis was wrong (FD loads no library). Both lines
  are emitted by the game's own data: `You move <dir>.` and `Date: … Time: …` come
  from the **Specific override task `Beforeplay`** (a `BeforeTextAndActions`
  override of the stock `PlayerMovement` general task — empty-key `<Specific>
  Direction</>` that matches any direction; its CompletionMessage is the move
  confirmation, its action `Execute ts_tasCheckTime` prints the date/time). It is
  **not** a turn-event — Amazon.xml ~11824 is `ts_tasCheckTime`'s own message.
  Amazon also stacks a per-location/direction travel-time override (`Delay57`,
  `Delay45`, …; sets `MinutesPerTurn` + `Execute ts_tasIncrement`, no output) on
  nearly every move, so a typical move matches **two** `BeforeTextAndActions`
  children. `run_general` (`a5run.cpp`) ran the first (`Delay*`) and then
  unconditionally `break`ed (`/* one override is enough for v1 */`), dropping
  `Beforeplay` — so no `You move` / `Date:` line.
- **Fix:** `run_general` now ports FrankenDrift's child-override loop
  (`clsUserSession.AttemptToExecuteSubTask`, clsUserSession.vb:1056-1160): it runs
  **all** matching `Before*` overrides in priority order, continuing past a passing
  child that produced no output (or `ContinueAlways`) and stopping after one that
  did (FD's `bContinue` rule, vb:911-936; restriction-fail-with-output still claims
  the turn in HighestPriorityTask mode). So `Delay*` (no output) runs and falls
  through to `Beforeplay` (output) every move. Verified: trace shows
  `Delay57 → Beforeplay → PlayerMovement`, and `You move <dir>.` + `Date:` now emit.
- **Plus a division-rounding fix (`a5arith.cpp`):** the date/time arithmetic
  `ts_varHour += (%Minute%-30)/60` needs ADRIFT's `/` semantics —
  `Math.Round(a/b, MidpointRounding.AwayFromZero)` in double (clsVariable.vb:811),
  not C integer truncation. Without it the clock carried the wrong hour (Scarier
  showed 14:22 where FD shows 15:22). a5arith now evaluates in double and rounds at
  `/` (away from zero), `fmod` for `mod`; the Date/Time values now match FD exactly
  (12:35, 15:22, 15:53, …). a5arithtest extended with the rounding cases.

**Residual (separate items, not 2b):** Amazon still shows the *startup* `Date:
12:04` offset (StartGame's `Execute Look` → `Beforeplay1` override; Scarier's
`SetTasks Execute Look` is a direct room-render shortcut that skips the override),
and it dies at the **bear cave** (`shoot bear` parser/combat gap) so the
playthrough stops at ~1/3 of FD's. JacarandaJim now **completes** instead of dying
at turn 272, exposing the unimplemented `%Array[index]%` text-function. Corpus
re-blessed (AxeOfKolt 48→42, JacarandaJim/Amazon at their full-traversal budgets).

---

## P3 — Noun & pronoun resolution

### 3a — pronoun resolution `it`/`them`/`him`/`her`  ✅ DONE (2026-06-29)
**Was:** `get it` after an examine → "Sorry, I'm not sure which object you are
trying to take." where FD resolves to the just-referenced object. **Jacaranda
Jim (35×)**, cascaded hard.
**Fixed** by porting FrankenDrift's two-part mechanism:
- `clsUserSession.GrabIt` → `grab_it` (`a5run.cpp`): each turn, after the input
  words are known, scan in-scope (visible then seen) objects' names and
  characters' proper-name/descriptors; an unambiguous single match per pronoun
  class records that entity's display name in new `a5_state_t.s_it`/`s_them`/
  `s_him`/`s_her`.  Objects → definite full name; characters → definite known
  name; "them" only considers plural objects (Article=="some"); characters split
  into him/her/it by the `Gender` property.  >1 match is narrowed by a prefix
  word in the input (FD's second pass).  Empty classes default to FD's
  "Absolutely Nothing"/"No male"/"No female".
- `clsUserSession.EvaluateInput` substitution → `resolve_pronouns` (`a5run.cpp`,
  called from `a5run_input` after `update_seen`, skipped for amb clarifiers):
  if the input contains the whole word it/them/him/her, echo FD's "(name)" line
  and textually replace the word with the stored (lower-cased) name *before*
  parsing, so the ordinary noun resolver handles the substituted full name.
  Then `grab_it` recomputes the referents for next turn.  `replace_word` ports
  ReplaceWord (whole-word, interior/leading/trailing/whole).  All four fields
  save/restored (`ItRef`/`ThemRef`/`HimRef`/`HerRef`).
**Result:** Jacaranda 432→151 (the get/wear-it chain resolves; the residue is
downstream event/template/NPC bugs, not pronouns).  No other corpus script uses
pronouns, so the rest are unchanged.  a5runtest + a5savetest pass; re-blessed.

### 3b — object aliases not matched  ✅ DONE (2026-06-29)
**Was:** `examine id` → "You see no such thing." while FD resolves `id` → "ID
pass". **Revenge of the Space Pirates** (sibling nouns `lighter`/`pass` resolve,
so it's alias-specific).
**Root cause:** `name_match` (`a5run.cpp`) mirrors FD's
`(article )?(prefix_i )?...(name)` regex but consumed the prefix words
**greedily, without backtracking**.  The ID pass object has prefix `ID` *and*
name `id`; "examine id" consumed "id" as the (optional) prefix and left nothing
for the name → no match, where .NET's regex backtracks and matches "id" as the
noun.
**Fixed:** `name_match` now backtracks via `name_match_prefix` (tries
skip-vs-consume for each prefix word, both article branches) + `name_match_tail`.
Strictly a superset of the old matches (= exactly the regex semantics), so more
conformant with no false positives.  AxeOfKolt 54→50, MagneticMoon 13→10, Revenge
8→7, RunBronwynn 47→45, JacarandaJim 151→147.  All a5 unit tests pass.

### 3c — CharHereDesc blank suppression  ✅ DONE (2026-06-29)
**Was:** Revenge's room view appended "Customs Official is here." (10+ times)
where FD's prose already says "A customs official stands here." and FD omits the
auto present-line.
**Root cause:** the Customs Official has a `<Property><Key>CharHereDesc</Key></>`
with **no `<Value>`** — a blank override.  FD's `clsLocation.ViewLocation` checks
`ch.HasProperty("CharHereDesc")` (== `ContainsKey`), so a blank value still
overrides the default and `IsHereDesc` returns `""` ⇒ suppressed.  Scarier's
`char_here_desc` gated on `p->value_node != NULL`, falling through to the default
"<Name> is here.".
**Fixed:** `char_here_desc` (`a5text.cpp`) now treats a present-but-value-less
property as an empty (suppressing) override.  SpectreOfCastleCoris 68→44, Revenge
23→8, Amazon 42→34.  No regressions.

### End-of-game banner pSpace  ✅ DONE (2026-06-29) — see top of file.

---

## P11 — Equal-priority task selection: passing task must override a failing-with-output one  ✅ DONE (2026-06-29)

**Was:** **Anno 1700** desynced on its FIRST conversation. `say hello` (alone with
the receptionist, not yet conversing) → Scarier "I'm not sure who you are referring
to." where FD prints `(to a young woman)` and fires her greeting. From that one
miss the whole 335-hunk transcript cascaded (the "announce arrival" flag never set,
every later room/inventory step drifted).

**Root cause — equal-priority task ordering in HighestPriorityTask mode.** The
stock library ships two tasks for `say %text%` at the *same* Priority (50064):
- `Say` — restriction `%Player% Must BeInConversationWith AnyCharacter`, fail
  **message** "I'm not sure who you are referring to." (fires only mid-conversation).
- `SayLazy` — restrictions `MustNot BeInConversationWith AnyCharacter` **and**
  `Must BeAloneWith AnyCharacter`, **no** fail message; its CompletionMessage is
  `(to %CharacterName[%AloneWithChar%]%)` and it delegates to `SayToCharacter`
  with `%AloneWithChar%`.  They are complementary: when alone-and-not-conversing,
  `Say` fails *with output*, `SayLazy` *passes*.

  FrankenDrift's `GetGeneralTask` (clsUserSession.vb:6065-6088) stops at the first
  command-matching task that fails-with-output **or** passes (`GoTo FoundTask`),
  so in HighestPriorityTask mode (Anno's default — no `<TaskExecution>`) it only
  reaches `SayLazy` because .NET's **unstable** `List(Of TaskKey).Sort` happens to
  order `SayLazy` ahead of the XML-earlier `Say`.  Scarier sorts with
  `std::stable_sort`, preserving XML order, so it hit `Say` first, emitted its
  message, and claimed the turn.

**Fixed** in `a5run.cpp scan_tasks`: rather than replicate .NET introsort's
tie-break (fragile), Scarier now defers the HighestPriorityTask fail-claim the same
way the HighestPriorityPassingTask path already does — record the first
failing-with-output task (+ its `fail_priority`) and keep scanning.  A later
**equal-priority** task that returns `RR_OK` runs and overrides it; the moment the
scan descends to a **strictly lower** priority (higher value) with no pass found,
it returns 0 and the caller emits the recorded message (HighestPriorityTask still
blocks all genuinely-lower tasks).  Deterministic and principled: a passing task
beats an equal-priority failing one regardless of model order.

**Result:** Anno 1700 **335 → 134** (−60%); the conversation works and the cascade
is gone (residual = downstream object-property template gaps, e.g.
`%closet.openstatus%`).  **No other game regressed** (the corpus has no case where
FD's sort intentionally shows an equal-priority fail message over a pass).  All a5
unit tests pass; baseline re-blessed.

---

## P4 — Shop `buy` / purchasable-object visibility  ✅ DONE (2026-06-29)

**Was:** at the Amazon trading post `buy ammo`/`buy provisions`/`buy matches`/
`buy torch` → "Sorry, I'm not sure which object you're referring to. (objects
must be seen)" where FrankenDrift purchases. **Not** a noun-resolution / "must be
seen" gate bug at all — the buy task *matched*; the failing message is the game's
own `ReferencedObject Must HaveBeenSeenByCharacter` restriction.

**Root cause — missing object-group property inheritance.** The four for-sale
objects (Ammunition/Provisions/Matches/Flashlight) carry **no** `StaticOrDynamic`
property in their own `<Object>` blocks (verified via `test/a5dump …Amazon.blorb`).
Their `StaticOrDynamic=Dynamic` **and** `BuyableItem` come from an Objects-type
**group `BuyableItems`** whose `<Property>` list FrankenDrift inherits onto member
objects (`clsItemWithProperties.htblProperties` layers `htblInheritedProperties`,
clsItem.vb:272-345). Scarier didn't implement that, so each item stayed
`StaticOrDynamic`-absent ⇒ `compute_objloc` treated it static (no `StaticLocation`
either) ⇒ `A5_OWHERE_NONE`/Hidden ⇒ never marked seen ⇒ the HaveBeenSeen
restriction failed (and `HaveProperty BuyableItem` would have failed too).

**Fixed** by `a5_apply_group_properties` (`a5model.cpp`, run after `a5_load_groups`
in `a5model_from_doc`): each Objects-type group's parsed `<Property>` list
(new `a5_group_t.props`/`n_props`, via the existing `a5_collect_props`) is folded
into its member objects' static `props` (FD precedence: inherited overrides the
object's own value if both present, else appended via `realloc`; groups iterate in
load order so the last property-group wins). Because every object-property read
goes through `a5_prop_find(o->props,…)` (`obj_prop`/`obj_has_prop`, a5restr
`HaveProperty`, a5text), no call-site changes were needed. The merge runs before
`a5state_new`, so the for-sale items start `Dynamic` at `Location3` (seen on
entry) and pass `BuyableItem`. Static-only keys here (never mutated at runtime),
so the runtime override layer (`a5state_entity_prop`, e.g. `Purchased`) is
untouched.

**Scope note (in code):** inheritance is resolved once at load; runtime add/remove
from a group does not re-trigger it (FD recomputes via `ResetInherited`) — not
exercised by the corpus.

**Result:** all four buys now purchase; Amazon 44→41, **no other game regressed**
(faithful to FD). Re-blessed. Residual to a full `*** You have won ***` MATCH is
the **P2b** Date/Time + "You move" status lines (separate turn-event/move-confirm
mechanism), a provisions completion-message line-join, the title-centring nit, and
a downstream "too dark" altar divergence.

---

## P5 — Return to Camelot divergence  ⚠️ ORIGINAL DIAGNOSIS DISPROVEN (2026-06-29)

**The 5.0.26 bracket-correction is NOT the cause.** Two empirical findings:

1. **FD headless does NOT apply the correction.** The auto-correct is gated on
   `Glue.AskYesNoQuestion`, whose headless impl (`Program.cs:74`) returns *false*
   unless the *next script line* is literally `yes`/`no` — and RtC's first script
   line is a comment, so it peeks that, doesn't dequeue, and returns false ⇒
   `bCorrectBracketSequences=false`, no `CorrectBracketSequence`. The question
   *prose* is still emitted (FileIO.vb:635 `EmitHtml` runs before the queue
   check), which is why FD's transcript shows the "Adventure Upgrade / There was a
   logic correction…" lines — but the restriction logic is **unchanged**. Scarier
   already ports the runtime `EvaluateRestrictionBlock` (`a5restr.cpp eval_block`
   + `normalise_block`), so the two evaluate restrictions identically.

2. **`x desk` works in isolation in Scarier** (prints the desk description). The
   full-walkthrough "I don't understand what you want to do with the desk." is a
   **downstream cascade**: it only appears after the exact prefix
   `open top drawer → get folders → get gun → x gun → close top drawer` (drop *any*
   one and `x desk` resolves fine); plain `wait`×N does *not* reproduce it. So
   some event/task fired by that specific command run leaves "desk" no longer
   resolving as an object (the trace shows `ExamineObjects` dropping out, only the
   `*`-catch-all `Task236` and `ExamineCharacter` matching). This is an
   event/state-timing bug, **not** restriction combination.

**Remaining real RtC gaps:** (a) the auto-correct question prose (Scarier has no
yes/no prompt-emit path at load; minor, top-of-transcript). (b) the `x desk`
resolution cascade above — find what that 5-command prefix changes in object
visibility/`obj_seen` or fires as an event. **Re-scope before working RtC.**

---

## Direction restriction always passed + ProperName/SetProperty expression  ✅ DONE (2026-06-29)

**Was:** **Grandpa's Ranch** desynced on the FIRST move — `n` (north into the
house) printed "You enter the car and sit down…" and teleported the player into
the car. Two unrelated root causes, both broad:

1. **`Direction` restrictions always returned 1 (pass).** `pass_single`
   (`a5restr.cpp`) had no `Direction` branch — it fell through to the
   `result = 1` "Item/Direction: Phase 3-4" stub. Grandpa's `vnl_GoCarFirst`
   PlayerMovement override is gated on `BeLocation Location1 AND (Direction
   BeWest OR Direction BeIn)`; since both direction checks "passed", moving
   *north* fired the car-entry override.
   **Fixed** by porting `clsUserSession.vb:5161` (`r = sKey1 == ReferencedDirection`).
   The `<Direction>` element text is `"<Must|MustNot> Be<Dir>"` (FileIO.vb:611-615
   strips the leading `Be`); parse_spec's generic key1/op split mishandles it, so
   the new branch re-tokenises `r->raw`, strips `Be`, and compares against the
   bound `ReferencedDirection` (canonical names already match DirectionsEnum:
   "West"/"In"/…), returning directly (must_not handled inline — parse_spec
   can't see the leading-token `MustNot`). **Broad impact: LostChildren
   354→6, GrandpasRanch 125→67, MagneticMoon 10→6, DieFeuerfaust 19→16.**

2. **Player's typed-in name rendered as "0".** Grandpa's name task does
   `SetProperty %Player% CharacterProperName PCASE(%text%)` and templates use the
   OO chain `Player.ProperName`. Two gaps: (a) `SetProperty` stored the value
   verbatim (`PCASE(%text%)`) instead of evaluating it — fixed in `a5run.cpp` by
   running `a5text_eval_expression` on any value containing a `%reference%`
   (mirrors `EvaluateExpression`; plain keys/state values have no `%` and stay
   raw, matching FD's Nothing→raw fallback). (b) the `.ProperName` OO step had no
   handler so it hit the generic-property fallback for key `ProperName` (the real
   key is `CharacterProperName`) → "0"; added a `ProperName` case in
   `a5expr.cpp`'s char branch and made `character_display_name` (`a5text.cpp`)
   prefer the runtime `CharacterProperName` override, both mirroring
   `clsCharacter.ProperName` (override → model Name → "Anonymous").

**Result:** Grandpa moves north correctly and greets "Hi Pete". Budgets
re-blessed (above). No regressions; all a5 unit tests pass. **LostChildren
residual (6):** object-resolution edge cases (`take slates/seashells/shells` →
FD "not sure which object"/"no seashells to give"; an "as it is not locked"
line) — separate noun-resolution item, not direction/name.

---

## P6 — Object pickup via examine + light-source darkness  ✅ DONE (2026-06-29)

**Was:** the shovel revealed by `x shelves` was "never taken" so `dig` failed.
The shovel examine itself was fine — the player never *reached* its room. Three
chained root causes, each broad:

1. **`%object%.Parent` was NULL for an object lying in a location.**
   `parent_key` (`a5expr.cpp`) returned the container key only for
   on/in/held/worn/part placements, NULL for `A5_OWHERE_LOCATION`/`LOCGROUP`.
   FD's `clsObject.Parent` (clsObject.vb:663) returns `Location.Key` for *every*
   placed case incl. InLocation. So the stock take task's restriction
   `%objects%.Parent.Takefix.Sum=0` (detects "already carrying": parent==Player
   has Takefix=1) evaluated wrong → `get flashlight` produced no output and the
   flashlight was never taken. **Fixed** by adding LOCATION/LOCGROUP to
   `parent_key` (st->obj[oi].key already holds the location key there).

2. **A Text property whose value is a rich `<Description>` (value_node) rendered
   as "0".** `flashlight on` prints `%object%.WhenOn`; the OO resolver's generic
   property branch (`a5expr.cpp`, char/obj) only read the scalar
   `a5state_entity_prop` value (NULL for a value_node prop) → fell through to
   "0". **Fixed** by rendering `pr->value_node` via `a5text_eval_description`
   when the scalar is absent (mirrors the existing `%PropertyValue%` path).

3. **No runtime object-group membership.** The flashlight joins the
   `LightSources` group via an `AddObjectToGroup` action in a
   `BeforeTextAndActions` Specific override of `SwitchObjectOn` (and leaves it on
   SwitchObjectOff). `AddObjectToGroup`/`RemoveObjectFromGroup` actions were
   unhandled and group membership was static, so the darkness override's
   `MustNot BeInSameLocationAsObject LightSources` never saw the lit torch → the
   tunnel/storage stayed "too dark". **Fixed** with a runtime membership layer
   (`a5state_object_in_group`/`a5state_set_object_in_group`, `a5state.cpp`),
   stored on the existing property-override store under a synthetic
   `@InGroup:<group>` key (so it saves/restores for free); the two actions wired
   in `run_action`; `a5restr` `BeInGroup` (objects) and the
   `BeInSameLocationAsObject` group expansion now consult it.

**Result:** flashlight taken + turned on, darkness lifts, `x shelves` yields the
shovel. **Grandpa's Ranch 67→45**; no regressions; all a5 unit tests pass
(incl. save). Re-blessed.

**Still open (Grandpa residual 45):** ✅ **all resolved — GrandpasRanch is now a
full MATCH (0/0).** (a) the **dial numeric-input prompt** was fixed by the
SetTasks-Execute literal-key fix (the `vnl_Dial` auto-cap item below: `turn dial`
→ box → map → Buster → dig → WIN). (b) the **bull-capture event chain** played
once Buster's `MoveCharacter ToLocation`/`ToSameLocationAs` was implemented; the
final two render-ordering hunks (`open gate` "You open the gate." +
`go west` bull list-description) were fixed by the BeforeActionsOnly swap and the
deferred room view (see the ⭐ item at the top of this file).

---

## P7 — NPC presence list omits a character  ✅ DONE (2026-06-29)

**Was:** Magnetic Moon's control room "X, Y and Z are here." list omitted
"Captain Rosenloev" that FD lists.  Root cause: Rosenloev's `CharacterLocation`
is `On Object` (seated on Chair1), but `a5state_new` only decoded the
`At Location` case — on/in-object characters got `char_loc == NULL`, and the
renderer's present-NPC loop tested `streq(char_loc[i], lockey)`, so he was
dropped.  FD's `clsLocation.CharactersVisibleAtLocation` uses
`clsCharacter.IsVisibleAtLocation`/`BoundVisible`, which resolves a seated
character through the carrier object to its room.

**Fixed:** `a5state_new` now decodes `On Object`/`In Object` into
`char_onobj`/`char_in` (keys `CharOnWhat`/`CharInsideWhat`), and the renderer
calls the new `a5state_character_at_location` (mirrors `IsVisibleAtLocation`: an
`At Location` char compares its location key; an on/in-object char is present
wherever its carrier object's container chain reaches).  `On Character`/`Hidden`
remain not-at-any-location (unchanged).  Known limitation: a char inside a
*closed opaque* container is still reported present (FD's `BoundVisible` hides
it); unused by the corpus.

**Result:** MagneticMoon 30→27; no regressions; all a5 unit tests pass.
Re-blessed.

---

## P8 — Y/N prompt text not printed

**Symptom:** Scarier enters the yes/no state but doesn't print the question
("Do You Wish To Explore The Ship First? (Y/N)"). **Starship Quest, Magnetic Moon.**

**Locus:** the task/event that emits the prompt + the yes/no input state in
`a5run.cpp`; the prompt text is likely a task output that is being suppressed.

**Fix:** emit the prompt text when entering the Y/N wait state.

---

## P9 — Parser error wording + direction capitalization

### 9a — wrong "not sure which object" instead of the game's message  ✅ DONE (2026-06-29)
**Was:** Scarier "Sorry, I'm not sure which object you are trying to examine." vs
FD "You see no such thing." (Lost Children, and the same shape across most games).
**Root cause (NOT a wording/string difference):** task *selection*. When a
command's object/character reference resolves to nothing, Scarier's
`resolve_refine` returned `RR_NOREF` for **every** command-matching task and the
`scan_tasks` no-ref fallback picked the **first** one in priority order — which
for `examine X` was the low-priority `ExCharsObj` (`...%character%{{'}s}
%objects%`, message "...not sure which object you are trying to examine.")
instead of the high-priority `ExamineObjects` (message "You see no such thing.").
FrankenDrift instead only lets a no-reference command match a task at all on its
**second-chance** pass, and only if that task has a `Must Exist` restriction of
the unresolved reference's type (`InputMatchesObject`/`Character`'s
`bSecondChance AndAlso task.HasObjectExistRestriction` gate, clsUserSession
5404/5507; `clsTask.HasObjectExistRestriction`/`HasCharacterExistRestriction`).
ExamineObjects has `ReferencedObject Must Exist`; ExCharsObj/ExamineCharacter do
not (only HaveSeen/Visible), so only ExamineObjects revives → its message wins.
**Fixed:** new `a5restr_has_exist(restrictions, 'o'|'c')`; in `a5run.cpp`
`resolve_refine`, a reference that names nothing now becomes the `RR_NOREF`
candidate **only** when the task has a matching `Must Exist` restriction —
otherwise the task returns `RR_NOMATCH` (does not match), exactly like FD's
first-pass `DoesntMatch`.  Applied at all three no-match sites (singular
%object%, generic object/character, and the plural %objects% path).
**Result:** no regressions, broad drop — AxeOfKolt 72→64, Spectre 96→69,
StarshipQuest 16→9, MagneticMoon 32→30, Revenge 53→49, DieFeuerfaust 40→26,
LostChildren 383→361, RunBronwynn 49→47, Jacaranda 433→432.  All a5 unit tests
pass; baselines re-blessed.

### 9b — direction capitalization (NOT A BUG — was a desync artifact)
`SouthEast` (Scarier) vs `Southeast` (FD) at one Lost Children line looked
systematic but is not: FD itself emits `You walk SouthEast.` (the
`DirectionsEnum.ToString` form) for **most** compound-direction moves and
`Southeast` only rarely (1 of 3 in Lost Children), via a different/odd path.
Scarier already matches FD's dominant `SouthEast` form; the single mismatched
line is a transcript desync (the two engines were in different rooms), not a
casing bug.  No fix warranted.

---

## P10b — OO property chain inside a text function's arguments  ✅ DONE (2026-06-29)

**Was:** Anno 1700 leaked `the tall closet is currently closet.openstatus.` for the
template `%PCase[%object%.Name]% is currently %LCase[%object%.OpenStatus]%.` — the
inner OO property chain (`%object%.OpenStatus`) was substituted to its entity *key*
+ literal `.OpenStatus`, then the outer `%LCase[...]%` lower-cased that whole raw
string to `closet.openstatus` instead of the property *value* `closed`.

**Root cause — pass ordering.** `process_inner` (a5text.cpp) runs the whole
`replace_functions` pass *then* the OO pass (`a5expr_replace`).  A function whose
**argument** contains an OO chain (`%LCase[%object%.OpenStatus]%`) consumed and
transformed that chain during the function pass, before the OO pass ever saw it.
FrankenDrift avoids this because its `ReplaceFunctions(sArgs)` recursion (Global.vb
:2046) runs its *own* trailing `ReplaceOO` on the args, so `Closet.OpenStatus`
becomes `Closed` before `LCase` applies.

**Fixed** in `a5text.cpp replace_functions` (`[`-args branch): after expanding a
function's args, run `a5expr_replace` on them (guarded by `strchr(args,'.')`) so any
`<key>.<chain>` resolves to its value before the outer function transforms it.
`a5expr_replace` is a no-op on argument strings without a real entity `key.chain`,
so bare-key args (`%TheObject[Closet]%`, `%PropertyValue[a, b]%`) are untouched.

**Result:** `The tall closet is currently closed.` / `The kitchen stove is currently
closed.` now render correctly (both `%PCase[.Name]%` and `%LCase[.OpenStatus]%`).
Anno 1700 134 → 131; no other game regressed; all a5 unit tests pass. (Residual
Anno gaps are object-resolution specifics: `open closet` binds the "closet door"
where FD binds the "tall closet", and several push/specific-override puzzle steps.)

---

## P10 — Template / function expansion gaps  ✅ DONE (2026-06-29)

**Was:** Jacaranda Jim leaked a literal `%AlanRemarks[%AlanRemarkIndex%]%` — a
nested array-index variable reference not evaluated. `AlanRemarks` (Variable2) is
a **Text array variable** (`<ArrayLength>14</ArrayLength>`, 14 newline-separated
remarks in `<InitialValue>`); it is read-only, indexed 1-based by the index
variable `AlanRemarkIndex`, which a System task sets via `RAND(1,14)`.

**Root cause:** `replace_functions` already split `%AlanRemarks[%AlanRemarkIndex%]%`
into name=`AlanRemarks` + function-expanded args=`"7"` and called
`eval_function(st, "AlanRemarks", args)`, but `eval_function`'s variable lookup
was gated on `args == NULL`, so an indexed variable fell through to the verbatim
`return NULL` (token left as-is).

**Fixed** in `a5text.cpp eval_function`: an `args != NULL` branch now matches the
variable by name and, for a Text array, walks the single newline-separated
`var_text` string to the 1-based index'th line (mirroring ADRIFT's
`clsVariable` string array; out-of-range ⇒ empty). Numeric arrays (unused by the
corpus) fall back to the scalar value. Verified: the token now renders a real
member ("Alan burps." = line 2, correct 1-based).

**Result:** the literal leak is gone. Hunk count unchanged (Jacaranda still 449):
the remark *line* still diverges because `RAND(1,14)` draws a **different index**
in each engine's RNG stream (and `FD_RNG=xoshiro` makes Jacaranda diverge
*earlier*, not later — measured 1125 changed lines — so it does not align here).
Correctness improvement with no hunk change. No regressions; a5runtest/a5savetest/
a5arithtest pass.

---

## Harness follow-ups

- **FrankenDrift cache truncation  ✅ FIXED (2026-06-29).** `a5_groundtruth.sh`
  cached *any* non-empty FD output, so a dotnet run killed mid-game under load
  (OOM/signal — the `dotnet … || true` masked the non-zero exit) poisoned
  `test/.fd_cache` with a stunted transcript; the next run silently diffed against
  it (seen as Amazon "6" while the real full-FD diff was 64 — the truncated FD had
  died at the jaguar just like Scarier).  The FD headless has **no** wait/timeout
  loop (the old "10s wait-loop" note was wrong); the truncation is purely a killed
  process.  Fix: `fd_is_complete` now requires dotnet exit 0 **and** either every
  script command echoed (`^> ` count ≥ non-comment script lines) or an end banner
  (`*** You have won/lost ***` / restart prompt); `run_fd` retries up to 3× (the
  truncation is load-transient) and a partial run is **never** cached, only warned
  about on stderr.  When in doubt about a surprising hunk count, clear the cache
  (`rm test/.fd_cache/*.txt`) and re-run — and rebuild a5run_dump (`make … a5run`
  *does* relink, but a no-op `make` after a stash/branch swap can leave a stale
  binary that fakes "no change").
- After each fix, re-run `test/run_a5_walkthroughs.sh`, lower the game's baseline
  in the runner MAP, and capture a golden `_expected.txt` when it hits 0.
- **Two RNG modes are now tracked  ✅ DONE (2026-06-30).** The runner MAP carries
  two budgets per game (`name|game|vanilla|xoshiro`) and each game is diffed under
  both FD's stock `System.Random` (vanilla) and `FD_RNG=xoshiro` (FD drawing
  Scarier's xoshiro128** stream, so RAND text aligns and the diff is a full
  every-line check). A xoshiro count *below* vanilla = the residual was pure RNG
  noise; *equal* = a real RNG-independent bug. STATUS combines both modes; a
  regression in *either* fails the suite (non-zero exit). `XOSHIRO=0` skips the
  dotnet-heavy xoshiro pass. Captured xoshiro baselines: **StoneOfWisdom 0**
  (vanilla 2 — troll `OneOf` noise), **JacarandaJim 256** (vanilla 271 — the
  champagne-teleport `MoveCharacter ToLocationGroup Group7` random-room tail
  partly aligns); every other corpus game is identical in both columns, confirming
  those residuals are genuine logic divergences, not RNG. Vanilla remains the
  primary baseline (matches the shipped ADRIFT Runner); xoshiro is the
  second axis that catches draw-order regressions (xoshiro rising while vanilla
  stays flat) and proves RAND-text fixes like the SoW `OneOf`.
- **Stone of Wisdom** now plays deep into the game in both engines (173→44→5→**2**
  after the steel-door unlock + Pamba-trade + bare-`all` narrowing + the
  window-counter/OneOf fixes); a **perfect MATCH (0)** under `FD_RNG=xoshiro`. The
  vanilla residual 2 is only the troll-approach `OneOf` drawing a different element
  from FD's unaligned System.Random stream — the documented RNG caveat, not a bug.
