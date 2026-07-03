# TODO: ADRIFT 5 conformance bugs surfaced by the walkthrough corpus

## ⭐ Text-array index expression `%var[RAND(1,6)]%` + reflexive self-examine fallback (`examine me` → "yourself") — surfaced smoke-testing 7 new corpus games  ✅ DONE (2026-07-03)

**Two general engine gaps, both corpus-safe, found by opening-turn smoke-diffing
the 7 game files present in `test/adrift5-games/` but not yet in the walkthrough
MAP** (Halloween, MI, MuseumHeist, October31st, TheFortressOfFear, Tingalan, XXR).
A generic 4-command probe (`look`/`examine me`/`inventory`/`wait`) was diffed
against FrankenDrift under `FD_RNG=xoshiro`. **6 of the 7 are byte-exact on the
opening** (ready to wire in once walkthroughs exist); **Tingalan surfaced two real
bugs** (2 hunks → 1 after the fixes; the last is a documented RNG-class residual,
below).

**Fix 1 — array-variable index may be an expression, not just a literal
(`a5text.cpp` `eval_function`).** Tingalan's blocked-command message is
`%notallowed[RAND(1,6)]%`, where `notallowed` is a **Text variable with
`ArrayLength=6`** (6 newline-separated elements). Scarier's `%VariableName[index]%`
branch parsed `index` with `strtol` only and, when that didn't consume the whole
arg (here `RAND(1,6)`), `break`'d and left the raw `%notallowed[RAND(1,6)]%` token
in the output. FD runs the bracket contents through `EvaluateExpression`. Fix: when
the index isn't exactly an integer literal, evaluate it via the existing
`a5text_eval_expression` (`RAND(1,6)` → `a5_eval_sexpr` `rand` draw → an index),
then index the array. The plain-literal fast path (`%var[3]%`) is preserved
untouched, so every corpus game that indexes by a constant is byte-identical.

**Fix 2 — the empty-description examine fallback is reflexive when you examine
yourself (`a5text.cpp` DisplayCharacter fallback).** `examine me` on a player with
no `<Description>` renders the canned `%CharacterName% see[//s] nothing interesting
about %CharacterName[key, target]%.`. Scarier showed "…about **you**."; FD shows
"…about **yourself**.". Root cause (FD `clsCharacter.Name`, clsCharacter.vb:351-354):
FD upgrades **Objective → Reflective** when the same character key was already
rendered **Subjective** earlier this turn (its `PronounKeys` list). The leading bare
`%CharacterName%` renders the viewpoint subjectively ("You"), so when the examined
character *is* that viewpoint the target form becomes reflexive. Scarier has no
turn-wide `PronounKeys`, so instead of that broad machinery the fallback now emits a
**`reflective` qualifier** (→ "yourself") in exactly the subject==object case
(examined key == the bare `%CharacterName%`'s resolved subject = `ctx_char`/player);
every other target stays Objective. This is strictly narrower than FD (it only fixes
the one subject/object collision the fallback itself produces) so it can't regress
any other character-name render.

**Why corpus-safe / verified.** Whole corpus unchanged in **both** RNG modes
(`test/run_a5_walkthroughs.sh`: 24 goldens still byte-exact, StoneOfWisdom 2/0,
JacarandaJim 99/0, SixSilverBullets 18/0, LostLabyrinthOfLazaitch 8/0, BugHunt 0/1 —
all at baseline). All a5 unit tests pass (`run/arith/parse/dis/walk/obj/save` + Six
Silver Bullets golden), `make sanitize` is clean, and a direct ASan/UBSan Tingalan
run (exercising both fixes) is clean. No golden examines the player at an
empty-description object, and no golden indexes a text array by a non-literal, which
is why both new paths were dormant on the corpus.

**Remaining Tingalan residual (NOT a bug in these fixes — deep per-turn RNG-event
desync, documented-class).** The last smoke hunk is the specific array element
picked: Scarier's `RAND(1,6)` for the `wait` block draws index 3, FD index 5.
Under `FD_RNG=xoshiro` FD makes ~15 RNG draws **per turn** (a repeating
`RAND(1,9)/RAND(1,3)/RAND(1,150)/RAND(1,6)/RAND(1,2)` batch — a per-turn random
atmosphere/event subsystem) where Scarier draws once, so the shared xoshiro stream
is desynced by the time the visible draw happens. The opening text is nonetheless
byte-identical because those extra draws don't surface as visible text. This is the
same RNG-stream-alignment class as JacarandaJim/SixSilverBullets and is not worth
chasing for a **scriptless** game (no walkthrough, can't reach a full MATCH anyway);
recorded here so the finding isn't lost. If Tingalan ever gets a walkthrough, the
per-turn event RAND batch is the thing to align (instrument `a5rand_between`
`A5_TRACE_RAND=1` vs FD `FD_RNG_TRACE=1`).

## ⭐ Aggregate same-restriction per-item *fail* messages on the plural path (so author ALRs can match) — AoS `put all in bag` coin hunk → full MATCH (1|1 → 0|0)  ✅ DONE (2026-07-03)

**AoS 1|1 → 0|0 (byte-exact vs FrankenDrift in BOTH RNG modes; golden
`test/AoS_expected.txt` committed).** The `put all in bag` coin hunk is gone and
no other corpus game moved off baseline in either mode; a5 unit tests
(`run/arith/parse/dis/walk/obj/save` + Six Silver Bullets golden) pass and
`make sanitize` + a direct ASan/UBSan AoS/AxeOfKolt/DDF/TBN run are clean.

**What was implemented (matching FD's `htblResponsesFail` template keying).** The
restriction-fail path now aggregates same-restriction siblings instead of emitting
a separate singular message per item. `resp_add_fail` (a5run_action.cpp) keys fail
entries by the failing restriction's `<Message>` **node pointer** (the stable twin
of FD keying `htblResponsesFail` on `sRestrictionText = restx.oMessage.ToString`,
the raw template — clsUserSession.vb:5182/1301-1307): a second item failing the
SAME restriction node merges its `%objects%` key into the existing entry
(`obj_keys` grows, `nmut++`) rather than adding a new one. At `resp_flush`, an entry
with `obj_keys.size() > 1` re-renders its template ONCE over the merged set (rebind
`%objects%` to the pipe, like the pass-side `AggregateOutput` merge), so
`%TheObjects[%objects%]%` produces the aggregate list — AoS renders the single
`"You are not carrying the gold gonks and the silver ginks."`, which the game's
`cl_YouAreNotC1` `TextOverride` (that exact OldText, empty NewText) blanks via the
existing per-fragment `replace_alrs`, leaving no output and no stray pSpace. The
pass-cancels-fail check was generalised to drop a merged fail only when **every**
merged item also passed (FD's vb:812-834 all-refs-match loop).

**Why it's corpus-safe (the shared-path risk did not materialise).** A single-item
fail (`obj_keys.size() == 1`, the overwhelming common case) keeps its
eagerly-rendered singular text and the old text-dedup, so every non-aggregating
fail is byte-identical to the previous `resp_add_text` path. Node-pointer keying is
strictly *tighter* than FD's string keying (it never over-merges two distinct
restrictions that happen to share template text). The only behavioural change is
when ≥2 items fail the *same* restriction node in one plural command — and any such
case that FD merges but Scarier used to split would already have been a divergence,
so the byte-exact corpus proved none existed outside AoS. Confirmed: full corpus
(`test/run_a5_walkthroughs.sh`) unchanged in both modes, AoS 1|1→0|0.

**Historical root-cause notes (pre-fix), retained for context.** Root cause was
fully pinned (see
A5_WALKTHROUGH_FINDINGS.md AoS row + the DONE FailOverride entry below): it is an
**author suppression ALR that only matches FrankenDrift's *merged aggregate* fail
string**, which Scarier never produces.

**What FD does (and Scarier doesn't).** Both engines put `cl_Pouch` in the worn bag
first, which un-holds the nested coins (`cl_Coins` gold gonks, `cl_Ginks1` silver
ginks); both then re-fail the coins on the general `PutObjectsInOther`
`Must BeHeldByCharacter %Player%` restriction. FD buffers *fail* responses in
`htblResponsesFail` **keyed by the raw message template**
(`%CharacterName% [am/are/is] not carrying %TheObjects[%objects%]%.`), so the two coin
fails **merge into one entry** whose `%objects%` refs aggregate to {cl_Coins,cl_Ginks1}.
At flush it renders the single string **"You are not carrying the gold gonks and the
silver ginks."**, and the AoS author's `TextOverride` ALR `cl_YouAreNotC1` (that exact
OldText, empty NewText) blanks it in `Display`→`ReplaceALRs`. Scarier instead emits the
two coin fails as **separate singular** messages ("…the gold gonks." / "…the silver
ginks."); it *does* run ALRs (`replace_alrs`, a5text.cpp), but neither singular string
`strstr`-contains the merged-form OldText, so nothing is suppressed.

**The fix (what would make this hunk go to 0).** Scarier's plural fail path must mirror
FD's `htblResponsesFail` **template-keyed aggregation**: buffer each failing item's fail
message by its *unrendered* template + reference-name, folding same-template siblings
into one entry (accumulating their `%objects%` items), then render the merged template
**once** through `%TheObjects[%objects%]%` (so the aggregate list "the gold gonks and
the silver ginks" is produced) and run it through `replace_alrs`/Display. This is the
fail-side twin of the pass-side `AggregateOutput` merge already implemented on the
plural path (the `resp_map` `%objects%` merge in `run_general`).

**Where it lives / why it's risky.** `run_general` (a5run_action.cpp) +
`resolve_plural`/`resp_map` — the **shared** plural refine/execute/response path that
every plural-`%objects%` command in the corpus flows through. The passing goldens
`AxeOfKolt`, `ThingsThatGoBumpInTheNight`, `LostLabyrinthOfLazaitch`,
`DwarfOfDirewoodForest`, and `TreasureHuntInTheAmazon` all exercise this path
(`drop all`, `get all`, multi-object puts) and are **byte-exact/xoshiro-0 today**.
Merging fail messages changes when a per-item fail message is shown, how many blank
lines/paragraph slots it reserves (cf. the Tribute pre-ALR `bHasOutput` paragraph-slot
rule, DONE entry), and interacts with the pass-cancels-fail flush scope (TBN DONE
entry) and the FailOverride-on-collapse-to-one rule (AoS DONE entry below). Any of
those could regress a golden.

**Prereqs before attempting.**
- Read the AoS-FailOverride DONE entry (below) and the TBN pass-cancels-fail scope +
  Tribute empty-ALR-paragraph-slot DONE entries first — they constrain the flush order.
- Reproduce with `FD_RNG=xoshiro test/a5_groundtruth.sh "test/adrift5-games/AoS v.4.blorb" test/AoS_walkthrough.txt`
  (single hunk at transcript line ~600). Dump the module with `A5_DUMP_XML=/tmp/aos.xml`
  and grep `cl_YouAreNotC1` / the three coin `<TextOverride>`s.
- Gate the change so a *singular* command (`put gonks in bag`, no "all") is untouched
  and keeps its own singular fail message (only the `all`/multi path aggregates).
- **Acceptance = whole corpus unchanged in BOTH modes** (`test/run_a5_walkthroughs.sh`)
  AND AoS 1→0. If any other game moves off baseline, revert — this is one cosmetic hunk.

**Recommendation.** Not worth chasing until/unless the same fail-aggregation gap
surfaces on a game that *doesn't* MATCH for another reason (i.e. where it would actually
unblock a win or a large hunk drop). One cosmetic line behind a game-specific author ALR
does not justify the shared-path risk.

## ⭐ `HasSeenObject` must be per-character across a BECOME viewpoint switch (BugHunt `read sign` → "not sure which object", 2→1)  ✅ DONE (2026-07-03)

**BugHuntOnMenelaus `read sign` at South Road: Scarier "You can't see the sign."
where FD shows "Sorry, I'm not sure which object you are trying to read." (xoshiro
residual, transcript line 397).** The `ReadObjects` general task gates on, in order,
`ReferencedObject Must Exist` → `Must HaveBeenSeenByCharacter %Player%` (both →
"not sure which object") → `Must BeVisibleToCharacter %Player%` (→ "can't see the
%object%"). Both engines bind "sign" → the static `cl_Sign1` (the only object named
"sign", off in the elevator lobby `cl_Location28`). FD fails at `HaveBeenSeen`
(the *current* viewpoint never saw it) → "not sure"; Scarier passed `HaveBeenSeen`
and fell to `BeVisible` → "can't see".

**Root cause — a single global player-centric seen set leaked across BECOME.** In
FD `HasSeenObject/-Location/-Character` are **per-character** (clsCharacter);
`PrepareForNextTurn` marks, for every character, the objects *it* can currently
see. The `read sign` command runs as **Corporal Jones** (`Become jones`), but the
sign was seen earlier by **Davey**, who explored the building while *he* was the
player. Scarier had one `obj_seen`/`char_seen`/`loc_seen` array that a
`ToSwitchWith`/BECOME never swapped, so Davey's sighting persisted into Jones's
turn and `HaveBeenSeenByCharacter %Player%` wrongly passed. (BugHunt is the only
corpus game that switches player, which is why this never surfaced before.)

**Fix.** Added per-character snapshot stashes `seen_stash_obj/char/loc`
(`[n_characters]`) + `seen_active_ci` to `a5_state_t` (a5state.cpp). `a5state_switch_seen`
copies the outgoing player's active arrays into its slot and swaps the incoming
player's slot in (a NULL slot = a character that has never been the player → empty
set, then the normal per-turn `update_seen` marks its current surroundings). Wired
into the `ToSwitchWith` BECOME handler (a5run_action.cpp) right after `player_key`
retargets. The active arrays and all call sites are **unchanged** — a lone-"Player"
game leaves every stash slot NULL, so the corpus stays byte-identical. **Save/restore
is now per-character too** (answering the review question): the native writer emits a
`<SeenChar>` block (Key + sparse ObjSeen/CharSeen/LocSeen) for each non-active
viewpoint's stash, the reader repopulates it, and `restore_reset` frees the stash +
re-homes the active arrays on the restored player (mirrors FD's
per-`clsCharacterState.lSeenKeys`). Verified: whole corpus unchanged in both modes;
BugHunt xoshiro 2→1 and golden regenerated; ASan/UBSan-clean over the BECOME run;
`A5_SAVE_AT` save/restore self-check byte-identical at save-points 20/35/60 (incl.
saving mid-Davey while his set holds the sign, then confirming Jones's later read
still diverges correctly) and on non-BECOME games (AoS/FBA/AxeOfKolt).

**Remaining BugHunt xoshiro 1 (cosmetic, NOT fixed — shared room-view path risk).**
The last residual is a mid-transcript **blank line** (diff `65a66`): on arrival at
the shuttle FD emits a paragraph break between the `SetTasks Execute Look` room view
and the `cl_AqulianSpe` `LocationTrigger` System task's completion message ("As you
and your marines disembark…"), where Scarier space-joins (`pSpace`). Pinned to a
FrankenDrift chunk trace: FD's buffer reads `…Foley are here.\n<font color =
FDD017>\nAs you…` — i.e. the Execute-Look room view is followed by a `\n` **separator**
before the drained System task's message (the message itself starts `<font>\n`, giving
the doubled newline once `<font>` is stripped). Scarier's room view ends `are here.`
so `emit_completion`'s `sb_pspace` inserts two spaces, not a newline. This is NOT a
universal room-view trailing newline (cf. the JJ police-cell "…out of the cell.    Alan
appears…" case, which space-joins byte-exact in both engines) — it is specific to the
`qTasksToRun` drain emitting a PASS completion after an Execute-Look view. A fix would
have to change when the shared room-view / drain path inserts a paragraph break vs a
pSpace, with real regression risk across every game's movement turn, for one cosmetic
blank line. Same judgment as the AoS coin hunk above: not worth the shared-path risk.

## ⭐ A task could modify `Score` more than once (FBA leash re-scored via a second tie command)  ✅ DONE (2026-07-03)

**Symptom (FBA, surfaced during the max-score derivation).** After `make a leash`
(+5), typing `tie rope to dog` re-awarded **another +5** in Scarier (380 → 385) where
FrankenDrift stays 380. Both engines show the identical "…you've made a leash!" text;
only the score differs — so it is a pure scoring divergence, invisible in the transcript
diff (and doubly hidden here by the game's `Score >= MaxScore` "maximum 500" win banner).

**Root cause.** Two DISTINCT non-repeatable tasks both `SetTasks Execute cl_MakeLeash`
(the Repeatable=1 System task that owns the `IncVariable Score = "5"`):
`cl_TieRopeToD1` (commands `make a leash` / `tie rope to dog`) and `cl_TieDogWith`
(the Specific override of `cl_TieObjectT2` `tie %object% to %character%`, rope→Paddy).
`make a leash` runs the first; the later `tie rope to dog` — with `cl_TieRopeToD1`
already complete — resolves to `cl_TieDogWith`, which Execute's `cl_MakeLeash` a second
time. `cl_MakeLeash` being Repeatable=1, Scarier re-ran its `IncVariable Score` → +5.

**What FD does (instrumented to confirm).** `clsUserSession.ExecuteSingleAction`
(vb:2144) gates every `Score` change behind a per-task **`clsTask.Scored`** flag:
`If var.Key <> "Score" OrElse (task IsNot Nothing AndAlso Not task.Scored) Then …
If var.Key = "Score" Then task.Scored = True`. `Scored` is set on the first Score
change, **never reset**, and persisted in the save (`<Scored>`, FileIO.vb:112/935). A
temporary `Console.Error.WriteLine` at that gate printed, for the two `cl_MakeLeash`
runs: `alreadyScored=False -> True` (first, applies) then `alreadyScored=True -> False`
(second, suppressed). So a task adds to Score **at most once, ever** — real-ADRIFT
anti-farming behaviour. Scarier had no such guard.

**Fix.** Added `char *task_scored[n_tasks]` to `a5_state_t` (alloc/free in a5state.cpp,
reset on restart, serialized as `<Scored>` in save/restore in a5run.cpp — mirroring
FD's `<Completed>`). `run_task` records the task whose `<Actions>` are running in
`run->cur_score_ti` (saved/restored around both its action loops, so a `SetTasks
Execute` sub-task nests correctly — the owner of `cl_MakeLeash`'s action is
`cl_MakeLeash` itself, matching FD's `task` param). In `run_action`'s Set/Inc/Dec
handler, a change to the `Score` variable is suppressed when
`st->task_scored[run->cur_score_ti]` is already set, and sets it otherwise; non-Score
variables and the (rare) no-owner path (`cur_score_ti == -1`) are never gated. FBA leash
now 380 like FD; **whole corpus unchanged / still at baseline** (no game relied on a
task re-scoring), ASan/UBSan-clean, a5save test green.

## ⭐ Event/System/walk task swallowed its restriction-fail `<Message>` (FBA butcher's-stall leash → full MATCH)  ✅ DONE (2026-07-03)

**FinnsBigAdventure 0|1 → 0|0.** FBA's only RNG-independent hunk: at loc-92 the
System task `cl_AtButcherS` (a `<LocationTrigger>`) fires as the player walks in.
Its restrictions gate whether the dog steals meat (pass → the theft
CompletionMessage that drags the player out the gate) — the fourth restriction
`cl_RopeTiedPa Must BeEqualTo 0` carries a `<Message>` ("As you walk past the
butcher's stall, Paddy — being very hungry — tries to rush over … You pull on his
leash to stop him and he gives up the attempt."). When the rope is tied that
restriction **fails**, and FD shows its leash message; Scarier showed nothing.

**Root cause.** `attempt_event_task_impl` (a5run_events.cpp) returned **silently**
on any restriction failure (`if (!a5restr_pass(...)) return;`). But FD's
`AttemptToExecuteTask` for an event/System/walk task (`bCalledFromEvent=True,
bChildTask=False` — the LocationTrigger drain at clsUserSession.vb:3423, the event
`ExecuteTask` subevent at clsEvent.vb:389, and the walk task at clsCharacter.vb:1613
all pass these) still **evaluates responses**: a failing restriction sets
`sMessage = sRestrictionText` (vb:1246) → `AddResponse` buffers it in
`htblResponsesFail` → the end-of-task Display loop shows it. The `AddResponse`
`bHasOutput` gate drops the (overwhelmingly common) messageless failing System
task, so only a restriction with real fail text surfaces.

**Fix.** On restriction failure, emit `st->restriction_text` (the Message node
`a5restr_pass` already set as a side effect — Scarier's exact analogue of FD's
`sRestrictionText`), gated by `msg_has_output`, with a leading `sb_pspace`.

**The RNG footgun (caught + fixed before it shipped).** The first attempt used
`a5restr_fail_message`, which **re-evaluates the whole restriction block** to
recover the failing node. That re-drew any `RAND()`-valued restriction a SECOND
time — SixSilverBullets' per-turn `TimeTrapsT` System task has
`Roller Must BeEqualTo 'RAND(1,16)'`, so the extra draw desynced the whole xoshiro
stream (SSB xoshiro 0→6, and a cascade into a premature-death divergence). Reading
the cached `st->restriction_text` (set during the single `a5restr_pass` call, just
like FD sets `sRestrictionText` once inside `PassRestrictions`) avoids the second
evaluation entirely. SSB back to 0, whole corpus otherwise byte-unchanged in both
modes; FBA golden regenerated + xoshiro budget reblessed 1→0.

## ⭐ `put all in <container>` parent `<FailOverride>` on a single un-baggable item (AoS 3→1)  ✅ DONE (2026-07-03)

**AoS `put all in bag` showed "makes a mess" where FD shows "You are not carrying
anything." (2 hunks, transcript 1001/1775).** The earlier note filed this as a
"downstream `give food` NPC food-location divergence" — **that was wrong**. Proven by
probing: `give food` (transcript ~1340) fails *identically* in both engines and never
moves the food; the food's location is byte-identical throughout (`i` + `search bag`
after each `put all in bag` match FD exactly). It is a pure **message-selection**
divergence.

**Mechanism.** On the 2nd/3rd `put all in bag` the pouch/whetstone are already in the
bag, so the only held-loose item is the wrapped `cl_Food`, which fails `PutObjectI`'s
`MustNot BeObject cl_Food` gate. Both engines route the command through the general
`PutObjectsInOther` (Scarier's main scan only considers General tasks; the `PutObjectI`
/ `cl_PutAllInBa` specifics are its Override children). FD's `AttemptToExecuteTask`
finalize (clsUserSession.vb:789) shows the parent's `<FailOverride>` whenever
`htblResponsesPass.Count=0 AndAlso FailOverride<>"" AndAlso ContainsWord(sInput,"all")`,
discarding the child overrides' per-item fail messages. AoS's `PutObjectsInOther`
FailOverride *is* "You are not carrying anything." (XML — coincidentally the same text
`cl_PutAllInBa`'s restriction uses; the message comes from the parent, not that child).

**Why Scarier missed it.** `run_general`'s response-map path is gated on
`n_ref_items>1`. An `all` that collapses to a **single** surviving item (only the food)
fell to the *direct* path, where the child override's fail message went straight to
`out` and no FailOverride was ever consulted. A genuine singular `put food in bag` (no
"all") also uses the direct path and *correctly* keeps "makes a mess".

**Fix** (`a5run_action.cpp run_general`): an `all` command (`%objects%` ref text
contains the word "all") whose matched parent has a non-NULL `fail_override` now routes
through the response map even for one item, so the child fail messages buffer; after
`execute_task_with_overrides`, if **no** child override recorded a pass, the buffered
fail entries are cleared and replaced with the parent's rendered FailOverride. The
`>1`-item and non-`all` paths are untouched. Whole corpus unchanged in both modes
(AxeOfKolt/TBN/LostLabyrinth/Dwarf/Amazon/StoneOfWisdom all still 0/baseline);
**AoS 3→1** (MAP re-blessed). Remaining AoS hunk is the nested-pouch coin `put all in
bag` cosmetic — **root-caused 2026-07-03 as an author-ALR suppression, not an
iteration-ordering/refine bug** (the earlier note was wrong). Both engines include the
nested coins in the `all` set and both re-fail them (`cl_Pouch` bagged first un-holds
the coins → general `PutObjectsInOther` `BeHeldByCharacter` fails). FD merges the two
fails by message template (`AggregateOutput`) into "You are not carrying the gold gonks
and the silver ginks.", which the AoS author's suppression ALR `cl_YouAreNotC1`
(TextOverride, empty NewText) blanks; Scarier emits two *singular* fails ("…the gold
gonks." / "…the silver ginks.") that don't contain the merged ALR OldText, so its ALR
pass (`replace_alrs`) can't match. Verified by FD instrumentation + `A5_DUMP_XML`.
Fix would need Scarier's plural fail path to aggregate same-template per-item fails into
the merged `%TheObjects[%objects%]%` form before ALR/Display — shared plural response
path (run_general/resolve_plural), too much regression risk for one cosmetic hunk.
See A5_WALKTHROUGH_FINDINGS.md (AoS row), not chased.

## 📄 DwarfOfDirewoodForest: UNWINNABLE on Version 9; the older DDF build **WINS 250/250** (`light rope`, not `light fuse`)  ✅ RESOLVED (2026-07-03)

**FINAL UPDATE (2026-07-03, same day, third take): the DDF build IS winnable —
maximum 250/250 in 237 turns, verified in BOTH FrankenDrift and Scarier
(byte-identical under FD_RNG=xoshiro).** The earlier "arsenal escape is a
forced death" conclusion below was wrong: `cl_Fdd017Syst1` (the West-Side-of-
Field capture) has FOUR restrictions, and the fourth is **`cl_LightFuse MustNot
BeComplete`** — the death is explicitly DISARMED once the fuse-lighting
*specific task* has completed. The trap is that the game has THREE ways to
light the fuse and only ONE of them is that task:

- `light rope`  → General `LightObjec` + Specific override **`cl_LightFuse`**
  (keyed on the laid-rope object `cl_FuseO`) → completes `cl_LightFuse` →
  death disarmed. **This is the only safe phrasing.**
- `light fuse` / `light oil` → the separate General **`cl_StartFire`**
  (`[light] {the} [oil/fuse]` — this is why "fuse" parses at all; the object
  is only named "rope").
- `press lever` → Specific **`cl_LightFuse1`** on the tinderbox lever.

All three run `Execute cl_StartFireS` and start the identical 12-turn
`cl_FuseBurnin1` explosion event, but the latter two leave `cl_LightFuse`
incomplete, so walking the author's own escape route (e,e,n,n,n through
`cl_Location53`) into the field is a guaranteed capture — an **author bug**
(the death task should have keyed on the burning-fuse variable, not one of
three sibling tasks). The prior derivation used `light fuse` and concluded
"forced death"; with `light rope` the built-in `WLKTHRGH` escape works as
written, the blast fires `cl_KnockedOve` (outside `cl_InnerBlast`) on turn 12,
`cl_ArsenalDes=1`, and the endgame (retrieve axe from the vegetation, smash
the beer-store door, dismiss the lone guard 1/2/3, `unlock doors`) wins.

Wired as `test/DwarfOfDirewoodForestDDF_walkthrough.txt` + golden vs
`test/adrift5-games/DDF.blorb` (MAP `0|0`; the built-in walkthrough needed
exactly three corrections: `get rope`, `put rope in oil`, `light rope`).
Two Scarier bugs surfaced and fixed on the way (see the ⭐ entries dated
2026-07-03: plural-slot specific-override matching, walk-message expression
processing) plus the intro-trailing-`<cls>` paragraph-break fix. V9 remains
unwinnable (the `{*}` mis-bind below still gates By Guard Room).


**Final, empirically-verified conclusion (after two wrong intermediate takes,
both recorded below).** The game cannot be completed on the shipped Version 9,
in any engine — but NOT because the map is "sealed" (it isn't: you *can* move by
dragging a corpse). The real dead-end is that the only northward progress toward
the win requires either a blocked bare move or a fatal drag:

- The win (`cl_YouHaveWon`) needs `cl_ArsenalDes=1` (all four `cl_UnlockCell*`
  triggers), set only in the arsenal, reachable only **NORTH** through
  Bend-in-Tunnel `cl_Location12` (the author's own `WLKTHRGH` goes `n` from By
  Guard Room, then to the arsenal via a wagon).
- At By Guard Room (`cl_Location11`) the `{*}` gate (`cl_NullAtStar`, pri 44074,
  PreventOverriding) eats **every** empty-handed command — verified in FD: `n`,
  `north`, `go north`, `walk/run/creep/climb north`, `u`, even `look` all return
  "Please press O then press Enter." Only two tasks beat it: `cl_CreepSouth`
  (5173, → the dead-end Guard Room) and the general drag `cl_DragCharac1` (40485).
- Drag is the ONLY thing that moves you (its guard override `cl_Guard2` runs
  `MoveCharacter Player InDirection`; other objects just print "…will not help you
  in this game."). But entering `cl_Location12` **or** the Dungeons
  `cl_Location10` with the corpse visible fires an unconditional death — System
  tasks `cl_WithGuardA` / `cl_WithGuardA1`: `Player BeAtLocation 12/10` **AND**
  `cl_Guard2 Must BeVisibleToCharacter Player` → soldiers/officer "cut you down".
  Verified in FD: `drag guard n` from By Guard Room → "You Have Lost" at
  Bend-in-Tunnel.
- So you can only shuffle the corpse **WEST** (11→14, away from the soldiers — no
  death), a dead-end branch that does not reach the arsenal. North (bare) is
  blocked; north (drag) is fatal; there is no `creep north` and no second corpse
  before this point. ⇒ **Bend-in-Tunnel is unreachable alive ⇒ the arsenal is
  unreachable ⇒ no win task can fire.**

This is engine-independent (authored task priority + a location-triggered death),
so it holds in Scarier, FrankenDrift AND the real ADRIFT 5 Runner. It is almost
certainly a **shipped bug**: `cl_NullAtStar` is Larry Horsfield's disclaimer/menu
null-task (its "Please press O" message only makes sense on a menu; the other
three `{*}` copies are correctly on StartOptions/Prologue/Instructions). Mis-bound
to `cl_Location11`, it breaks the author's own bare-move-north route. The corpus
copy (SHA1 `bdc9422d…`) is byte-identical to the adrift.co "Version 9" download
(game 1639), so there is no fixed public build. Corpus status unaffected: Dwarf
stays `0|0` (all three engines agree, drag and death included).

**REGRESSION PINNED (2026-07-03): the `{*}` gate was ADDED in the 2022-08-07
update; the older build clears By Guard Room but has ITS OWN endgame death-trap.**
A player-supplied older `.blorb` (`DDF.blorb`, SHA1 `2fb6f724…`, game date
**2022-06-07**, 56 265 XML lines vs our 56 374) has only the THREE correct menu
`{*}` tasks — **`cl_NullAtStar` does not exist**, and nothing `{*}`-gates
`cl_Location11`. Verified in FD on DDF: at By Guard Room `> n` → "You move North."
into Bend-in-Tunnel, and entering the dungeons is safe (the blue-kilt/badge
disguise → guards "ignore you"; no death). So the shipped adrift.co **Version 9
(2022-08-07) is a regression** that broke the By-Guard-Room passage.

<details><summary>Superseded 2026-07-03 second take — "DDF dies at the arsenal escape" (WRONG: misread cl_Fdd017Syst1's restrictions — it also requires `cl_LightFuse MustNot BeComplete`; see the FINAL UPDATE above)</summary>

**But DDF is NOT cleanly winnable either — different fatal bug.** Attempted a
full drag-/version-aware derivation through FD. Two things surfaced: (1) the
game's built-in `WLKTHRGH` matches NEITHER build — it says `get fuse`/`put fuse
on oil` where both parsers only know the **rope** object (the "fuse" is the
length of rope; `light fuse` works only because that verb-pattern lists "fuse").
Fixing `get fuse`→`get rope`, `put fuse on oil`→`put rope in oil` reached
**195/250**. (2) Then the **arsenal escape is a forced death**: after
`light fuse` the boards can't be re-closed (`close boards` → "You should not
replace the boards now you have poured the oil"), the game forces you East into
the field, and **`cl_Location53` (West Side of Field) is an unconditional death
while the boards are open** (`cl_Fdd017Syst1`, gated only on `cl_MoveBoards`
complete; the explosion does NOT clear it). Every field route funnels through 53
(SW-Corner's only forward exit is N→53; NE/E invalid, W blocked). So a clean win
could not be derived on DDF. The built-in `WLKTHRGH`'s `e,e,n,n,n…` escape and
its "fuse" wording mean it targets a THIRD/earlier build where the boards were
closeable (or 53 wasn't lethal) — neither DDF nor V9 is that build. Net: **both
public builds are unwinnable, in different places** (V9 at By Guard Room, DDF at
the arsenal escape); best reached is 195/250 on DDF.

</details>

**EXHAUSTIVE DDF↔V9 DATA DIFF (2026-07-03) — the regression is ONE mis-typed
location key.** Full `diff` of the two deobfuscated XMLs is just 17 hunks; every
one is accounted for:
- **The break:** the 2022-08-07 update added a disclaimer/start-options screen —
  new location **`cl_Disclaimer`** ("A Note For The Player… press O to Continue to
  the Start Options Page"), new task **`cl_PAtStartOp`** (`[o/0]`, correctly
  `BeAtLocation cl_Disclaimer`), and new task **`cl_NullAtStar`** (`{*}`, pri
  44074, PreventOverriding, description "Null At **Disclaimer**", message "Please
  press O then press Enter."). The `[o/0]` handler was bound right; the `{*}`
  catch-all was **copy-paste-bound to `cl_Location11` (By Guard Room) instead of
  `cl_Disclaimer`.** That single wrong key is the entire regression. Player start
  also moved `StartOptio`→`cl_Disclaimer`, plus the group/visibility wiring for
  the three new elements. **The fix is literally `cl_Location11` → `cl_Disclaimer`
  on `cl_NullAtStar`'s one restriction.**
- **Cosmetic:** intro/ending credits (`cl_Task4`, `cl_EndgameLes`) added "Coming
  Soon: Episode 3: FINN'S BIG ADVENTURE" and de-"Coming Soon"-ed the Euripides
  line; `LastUpdated`/`Elapsed`/timestamps bumped.
- **NOT in the diff:** the arsenal / boards / `cl_Location53` / fuse code is
  **byte-identical** in both builds — so the arsenal-escape death-trap exists
  unchanged in V9 too (just unreachable there), and it predates both builds. The
  built-in `WLKTHRGH` (also byte-identical across builds, SHA1 `1b0ee578…`) still
  says "fuse" (renamed to the "rope" object before DDF) and uses the 53-lethal
  escape, so it was authored for a version older than DDF and never updated.

<details><summary>Two superseded intermediate takes — kept for the record (I was wrong twice)</summary>

**Take 1 (WRONG mechanism, right outcome):** ~~the `{*}` gate *seals* the map;
the arsenal is reachable only via cl_Location11→West which is blocked.~~ Wrong —
dragging a corpse (`cl_DragCharac1`, pri 40485) beats the gate and moves the
player, so the room is not sealed; you can go west.

**Take 2 (WRONG outcome):** ~~since drag bypasses the gate, the game is "likely
winnable, TBD".~~ Wrong — the drag-aware playthrough shows the NORTH route (the
only path to the arsenal) is a death-trap: `drag guard n` kills you at
Bend-in-Tunnel, and empty-handed north is `{*}`-blocked. Net result is the same
as Take 1 (unwinnable) but for the correct reason above.

</details>

## ⭐ `MoveCharacter … InsideObject / OntoObject / ToParentLocation` were unhandled no-ops (FBA custodian-niche stealth never hides the player)  ✅ DONE (2026-07-02)

Surfaced deriving FinnsBigAdventure's catacombs. To evade the custodian you
`hide in niche` (task `cl_LayInNiche`, action `MoveCharacter Character Player
InsideObject cl_Niche1`), then two `wait`s while he shuffles past: the System
task `cl_CustodianG` (Player `BeAtLocation 40` **AND** `BeInsideObject cl_Niche1`)
fires "…he passes the niche…" +5 and sets `cl_CoastIsCle=1`; the competing
higher-priority `cl_CaughtByCu` (Player at 40 **and NOT** inside `cl_Niche1`)
fires the fake-ending "Gotcha, you little git!" loss. In Scarier the player was
never actually placed inside the niche, so `cl_CaughtByCu` always won → guaranteed
loss where FrankenDrift plays "custodian goes past".

**Root cause** (`a5run_action.cpp`, the `MoveCharacter` action): the handler had
cases for `ToLocation` / `ToLocationGroup` / `InDirection` /
`ToStandingOn|ToSittingOn|ToLyingOn` / `ToSameLocationAs` / `ToSwitchWith`, but
**none for `InsideObject`, `OntoObject`, or `ToParentLocation`** — those
`MoveCharacterToEnum` forms fell through to the "best-effort no-op" tail. So the
niche-hide silently did nothing (the completion message still printed, masking
it), the player's `char_onobj`/`char_in` stayed unset, and next turn
`BeInsideObject cl_Niche1` read false.

**Fix**: added `InsideObject`/`OntoObject` (set `char_onobj[ci]=k2`,
`char_in[ci] = InsideObject?1:0` — the character stays at its current location,
mirroring `clsCharacterLocation.ExistsWhere InsideObject/OnObject`) and
`ToParentLocation` (clears `char_onobj`+`char_in` back to the floor, same
location — FBA's `out` of the niche). `BeInsideObject`/`BeOnObject` in
`a5restr.cpp` already read `char_onobj`+`char_in`, so they now round-trip.
Verified byte-identical to FD across the whole hide→wait→wait→out→loot sequence.
**Corpus-clean**: all 25 games at baseline (vanilla), no new source files.
Rebuild `make -f Makefile.headless a5run`.

## ⭐ "Which X?" disambiguation prompt lower-cased the candidate list before capitalising (ToProper bStrict=False)  ✅ DONE (2026-07-02)

Surfaced by Marooned On Mazoomah's ambiguous `cut door with laser` command,
which prompts `Which door?  The PC door or the entry door.`. Scarier rendered it
`the pc door` — the ADRIFT 5 disambiguation prompt (`DisplayAmbiguityQuestion`)
lower-cased the *entire* candidate list before upper-casing the first letter,
mangling the object name's internal casing.

**Root cause / fix** (`a5run.cpp`): FrankenDrift's `clsUserSession` builds the
prompt through `ToProper`, which is called with its default `bStrict=False` —
capitalise only the FIRST character and leave the rest of the string as-is.
Scarier ran an extra lower-case-everything pass first. Dropped that pass so only
the first character is upper-cased, matching the reference. RNG-independent; the
walkthrough corpus stays green (a5 vanilla pass, all games at/below baseline; the
self-contained a5 harnesses all pass).

## ⭐ Event-fired task on the plural path fired its completion controls even when its restrictions FAILED (FBA handfire never re-extinguishes)  ✅ DONE (2026-07-02)

Surfaced while deriving FinnsBigAdventure against FrankenDrift. In FBA the ball
of handfire "winks out" whenever you walk into a lit area — driven by a Length-1
repeating TurnBased event `cl_HandfireOu` (started by the `cl_Lumino` conjure
task) whose SubEvent runs the System task `cl_HandfireOu1` each turn; that task
hides the handfire when the player is NOT in a `DarkLocations`-group room, and
the event's `Stop Completion cl_HandfireOu1` control stops it once it fires. FD
re-extinguishes every time you re-conjure and step into light; Scarier
extinguished only the FIRST time, so the handfire persisted into the dungeon.

**Root cause** (`attempt_event_task_impl`, a5run_events.cpp): the plural path
(`n_ref_items > 1`, taken when a `get X and Y` command leaves ≥2 leftover
`%objects%` references — the same path that makes Amazon's `get ammo and rifle`
tick `ts_tasIncrement` twice) called `ev_on_task_completed` **unconditionally**,
even when the task's restrictions failed for every item and `run_task` never
ran. The single-item path correctly `return`s on restriction failure (no
completion, no controls). So when a plural equip command (`get toy soldiers and
toy warriors`, etc.) coincided with the handfire event's SubEvent executing
`cl_HandfireOu1` in the dark Your Room (restriction fails → no extinguish), the
spurious "completion" fired the `Stop Completion cl_HandfireOu1` control and
**stopped the handfire event mid-turn** (status → FINISHED with timer=1; the
repeating-restart only fires on a natural timer==0 end, so it never restarted).
From then on the handfire never re-extinguished. FD only fires EventControls on a
genuine `bPass`, so its event kept ticking and extinguished at the lit "Near Door
of Your Room".

**Fix**: gate the plural path's `task_done`/`ev_on_task_completed` on an
`any_ran` flag set only when an item actually passes restrictions and runs —
mirroring the single-item path. No-restriction event tasks (`ts_tasIncrement`)
keep `any_ran=1`, so Amazon's double-tick is unchanged. All 25 corpus games stay
at baseline (Amazon still MATCH 0), unit tests pass. Diagnosed via `A5_TRACE_RUN`
/ `A5_TRACE_RESTR` plus temporary `ev_lstart/ev_lstop/ev_increment` event-status
instrumentation. Scarier now byte-matches FD's 3 "winks out" in the FBA
walkthrough.



**STATUS (2026-07-02): no OPEN conformance bugs.** All 24 wired games are at
their best-known state (see the MAP in `test/run_a5_walkthroughs.sh`): 19 are
golden-backed **0|0 MATCH** in both RNG modes; the 5 DIVERGE rows are
explained-and-bounded, not open bugs — StoneOfWisdom 2|0, JacarandaJim 99|0,
SixSilverBullets 18|0 and LostLabyrinthOfLazaitch 8|0 are vanilla-only
System.Random-vs-xoshiro RAND text picks (their xoshiro columns, the real
conformance metric, are clean), and BugHuntOnMenelaus 0|23 is the documented
FD gap (Scarier wins 100/100 where FD is blocked; the 23 hunks are FD falling
short of Scarier's own winning golden, which backs its vanilla column). New
work comes from wiring more games — see `TODO_a5_walkthrough_wiring.md`
(FinnsBigAdventure derivation in progress, Magor/Xanix need blind
play-to-win, 5 games have no walkthrough material).

Entries below are a reverse-chronological log; each keeps the knowledge it was
written with, so an older entry's "residual/deferred/OPEN" notes may be
superseded by a newer entry above it.

## ⭐ Tribute → full MATCH: a game ALR that blanks a message must keep its paragraph slot (pre-ALR `bHasOutput`)  ✅ DONE (2026-07-02)

**Tribute 2|2 → 0|0** (full MATCH both RNG modes, golden
`test/Tribute_expected.txt`; budget re-blessed). Tribute (Kenneth Pedersen,
"Return to the City of Secrets") plays its built-in `WALKTHROUGH` (extracted
verbatim, zero corrections) byte-for-byte to `*** You have won ***`, 100/100,
143 turns. No corpus game moved in either mode (all 19 other MATCH games hold
0/0; StoneOfWisdom 2/0, JacarandaJim 99/0, SixSilverBullets 18/0,
LostLabyrinth 8/0, BugHunt 0/23 at their exact baselines), all a5 unit tests
pass (run/arith/parse/dis/walk/obj/save + Six Silver Bullets golden), and
`make sanitize` is clean.

**The original "override drops the note's blank slot" theory was WRONG — the
engine was never reserving anything.** The blank line FD emits between
`> get gem` and the grab response (mirror gem / boxes gem) is **authored**: the
game defines TextOverrides (ALRs) `FromTheEno` = OldText
`(from the enormous mirror)` and `FromTheBox` = OldText
`(from the boxes in the tunnel)`, both with **empty NewText** — the author
deliberately suppresses the `TakeObjectsFromOthersLazy` auto-note
`(from %objects%.Parent.Name)` for exactly these two gems (whose custom
override text says "through the mirror" already). Confirmed by instrumenting
FD's `AddResponse`/Display loop: the note IS stored and Display'd as
`<c>(from the enormous mirror)</c><!--…-->`; `Display()` (clsUserSession.vb:308)
runs `ReplaceALRs` which deletes the inner text, and the surviving
markup/comment/newline skeleton pSpace-joins the output — leaving the empty
paragraph = the blank line. The gems taken via *un*-blanked notes (latrine
hole, column hole, …) print theirs normally — also through Specific overrides,
which is what disproved the override theory.

**Root cause in Scarier — the "has output" test ran post-ALR.** Scarier applies
ALRs per-fragment inside `a5text_process`, so `emit_completion`'s
`msg_has_output` saw the already-blanked note (whitespace-only) and dropped the
whole message, paragraph slot and all. FD's `bHasOutput` runs on the stored
**pre-ALR** text (non-empty), and the ALR only blanks it inside `Display()`.

**Fixed** (`a5text.{cpp,h}`, `a5run_action.cpp`): `process_inner` gained an
optional out-param reporting whether the text had visible content BEFORE the
ALR pass (`process_inner_ex`), surfaced as `a5text_describe_ex`.
`emit_completion` (direct path) uses it: a message whose plain render is
whitespace-only but which had pre-ALR ink is emitted verbatim (pSpace + the
whitespace remainder), preserving the paragraph slot exactly as FD's output
stream does. Blast radius = "a completion message with real text that a game
ALR maps to nothing" — every other message has pre-ALR ink ⇔ post-ALR ink, and
ALR-less games skip the branch entirely. (The resp-map/plural path keeps its
old behaviour — no corpus game exercises an ALR-blanked note there; extend the
same pre-ALR test to `resp_add_text`/`resp_flush` if one ever does.)

## ⭐ BugHuntOnMenelaus → wired as a FULL WIN: `MoveCharacter ToSwitchWith` / BECOME player-switching was a no-op  ✅ DONE (2026-07-02)

`BECOME <character>` (ADRIFT's multi-protagonist mechanism) is a
`MoveCharacter Character %Player% ToSwitchWith <char>` action. Scarier's
MoveCharacter handled `ToLocation`/`InDirection`/`ToSameLocationAs`/… but had **no
`ToSwitchWith` case**, so every BECOME silently did nothing. In Bug Hunt On
Menelaus that meant the viewpoint never left Captain Erlin's cottage basement:
`become boone` printed "You are now playing as Sergeant Boone." but left the
player in the basement, and every subsequent marine command ("Sw", "open
dumpster", …) failed against the wrong room. The game was unplayable past the
first kill, and the whole transcript desynced from FrankenDrift.

**FD's semantics (`clsUserSession.vb`, MoveCharacterToEnum.ToSwitchWith):** when
either character in the swap is the current player, DON'T move anyone — change
*which character IS the player* (`Adventure.Player = htblCharacters(target)`,
flip CharacterType Player↔NonPlayer, preserve Perspective, transfer pronoun
descriptors). Because each character keeps its own location, the player viewpoint,
`%Player%` resolution and scope all follow the new character automatically; the
old player stays put, now an NPC. (When neither is the player, the two characters
actually exchange locations.)

**Root obstacle:** Scarier hard-coded the player key as the literal string
`"Player"` in ~30 sites (`a5state_player_location`, `act_key`, `resolve_key`, the
text engine's `%Player%`/`%CharacterName%`, restriction character resolution,
event/walk gates, the "X is here" present-list, …). FD's `Adventure.Player` is a
*mutable* pointer, so faithfully supporting BECOME needs a dynamic current-player
key.

**Fix:** added `a5_state_t.player_key` (init = the Type=Player character's key,
which is the literal `"Player"` in every corpus game) + `a5state_player_key()`.
Every *semantic* "who is the player" resolution now routes through it; the literal
model-key `"Player"` (referring to the player-slot character explicitly) and the
seed-perspective read are left alone. `MoveCharacter` gained the `ToSwitchWith`
branch that retargets `player_key` (and fires the player-move location triggers /
conversation-clear / seen-marking). `char_perspective` now keys 2nd person on
`c->key == a5state_player_key(st)` instead of the static `Type==NonPlayer` test —
identical for every game without BECOME (the sole Type=Player char *is* the
current player), but it makes a switched-in marine narrate as "you pick up the
pass", not "Lance-Corporal Davey pick up the pass".

**Result:** Scarier plays the full game to `*** CONGRATULATIONS! *** …the maximum
100 points!` (all 6 Meneltra, 69 turns) — including Lance-Corporal Davey's
3rd-floor Meneltra, past a pass-gated corridor. All 20 golden games stay
byte-identical — zero regressions. Files: `a5state.{h,cpp}`, `a5run_action.cpp`,
`a5text.cpp`, `a5restr.cpp`, `a5run_events.cpp`. (Also added an
`A5_DUMP_XML=<path>` env in `a5model_load` that writes the deobfuscated/inflated
game XML — handy for auditing task actions like this one.)

**FrankenDrift fix (2026-07-02): FD originally could NOT finish this game — it was
wired `0|23` as "Scarier surpasses FD".** After the elevator, Davey's N/S move
into the pass-gated corridor died with "Sorry, I didn't understand that command.",
capping FD at 4/6 kills / 65/100. This is the **known ADRIFT Runner v5.0.35 bug,
fixed in v5.0.36** (the game's own hint thread, intfiction.org/t/63289, has a
player hitting exactly this and being told to upgrade). Traced FD's headless
engine: `cl_PlayerMove1`'s bracket sequence indexes past its loaded restriction
list, so `EvaluateRestrictionBlock` hands `PassSingleRestriction` a `Nothing`;
`restx.Copy` threw a `NullReferenceException`, and `GetGeneralTask`'s `Try/Catch`
swallowed it and returned no task → "didn't understand" → movement silently
failed. **Fixed in FrankenDrift** (`FrankenDrift.Adrift/clsUserSession.vb`,
`PassSingleRestriction`): `If restx Is Nothing Then Return False` — a null
restriction fails closed (exactly what the old catch fell through to), without the
crash that aborted the whole task-selection pass. FD now walks the corridor and
**wins 100/100** too. Corpus re-verified: every other game byte-identical at
baseline; BugHunt xoshiro budget `23 → 2` (re-blessed in the MAP), the residual 2
being a minor Scarier-vs-FD `read sign` parser divergence, newly reachable only
because FD now completes the game.

## ⭐ DwarfOfDirewoodForest → wired 0|0 MATCH: plural `%objects%` bind clobbered the singular `%object%` container reference  ✅ DONE (2026-07-02)

The "hide-in-beard" bug from `TODO_a5_walkthrough_wiring.md`: `hide droppings,
knife and key in beard` → Scarier "You can't hide anything in that!" where FD
runs the beard-specific override ("Ok, you push the dire vulture droppings, the
sheath knife and the metal key into your beard where it is hidden from
sight."), and the whole cell escape cascaded from there (turn ~15 on). Single-
object `hide knife in beard` worked; only the multi-object list failed.

**Root cause — the plural bind aliased onto `ReferencedObject`.** Scarier's
`bind_reference("objects", ...)` bound the item under BOTH `ReferencedObjects`
AND the singular `ReferencedObject` alias (deliberately, for override-key
matching on plural-only commands). But `cl_HideObject`'s command is
`[hide/push] %objects% [in{side}/under/be{low/neath}] %object%` — TWO object
references — and its restriction `ReferencedObject Must HaveProperty
cl_CanBeHidde` names the *container* (`%object%`, the beard). During
`resolve_plural`'s per-item Applicable/final probes, each `bind_reference
("objects", item)` clobbered `ReferencedObject` (beard → droppings/knife/key),
so the property restriction was tested against the ITEMS, every item failed,
and the task returned RR_FAIL with restriction 7's message — the Specific
overrides (`cl_HideObjInB1`/`B2`) never got a chance. FD keeps the two slots
distinct: `GetReference("ReferencedObject")` resolves ONLY to the reference
whose `ReferenceMatch` is `"object1"`, never to the `"objects"` plural
(clsUserSession.vb:3990), while `"ReferencedObjects"` resolves to the first
object-type reference.

**Fixed** (`a5run_ref.cpp` + `a5state.{h,cpp}`): `resolve_refine` sets
`st->ref_objects_suppress_singular` when the matched command carries BOTH a
genuine plural `%objects%` and a separate singular `%object%`/`%object1%` (or
`%item%`) reference; `bind_reference` then skips the singular aliasing for
`"objects"` binds (and does not mark `ref_object1_plural`, so `%object%` text
tokens correctly render the container, like FD). Plural-only commands (`take
%objects%`) keep the old aliasing — conservative, and the whole corpus depends
on it. Reset by `a5state_clear_refs` per match attempt.

**Result:** the full 236-command native `WLKTHRGH` script is a **0|0 MATCH in
both RNG modes** (golden `test/DwarfOfDirewoodForest_expected.txt`, MAP line
added). NOTE this is *conformance*, not a win: FD (ground truth) still cannot
leave "By Guard Room" on the return visit — the `{*}` catch-all
(`cl_NullAtStar`, PreventOverriding) preempts the room's ordinary movement, and
so do FD and Scarier, spending the last ~166 commands identically answering
"Please press O then press Enter.". The pre-trap first act (cell escape, guard
kill/loot/drag, `creep south`) is real gameplay coverage.

**Verified faithful against the real ADRIFT 5 runner (jcwild/ADRIFT-5,
`clsUserSession.vb`).** An earlier version of this note guessed the real runner
"evaluates movement first" and that adopting a movement-before-`{*}` precedence
would reach the win — that is **wrong**. `GetGeneralTask` iterates tasks in
**ascending `Priority`** (lower number first) and, under the default
`HighestPriorityTask` mode, the first matching task that passes wins outright
(`GoTo FoundTask`); movement is a plain library task with no special ordering.
In this game the priorities are `cl_NullAtStar` **44074** (`{*}` → regex
`(.*?)?`, restricted only to `Player Must BeAtLocation cl_Location11`) vs the
movement tasks `MovePlayer`/`GetOffBeforeMoving`/`PlayerMovement`
**50235/50240/50242** — so `{*}` sorts *before* movement and blocks it, in the
real runner too. It is authored as an intentional gate: the room's specific
tasks all have low priorities (kill guard 5108, get kilt 5169, look-at-guard
4036, and the intended bypass **`cl_CreepSouth` = `creep/tiptoe south`, 5173**),
all `< 44074`, so they beat `{*}`, while generic `go <dir>` (~50240) cannot —
you must *creep* past the guards, not walk. The `{*}` restriction carries no
"guards alive"/first-visit condition, so it blocks generic movement there
forever (a faithful authoring quirk). Reaching the win is therefore a
walkthrough-script matter (`creep south` on the return visit), **not** an engine
precedence bug — no golden re-derivation is warranted.

No corpus game moved (all baselines hold in both modes); all a5 unit tests
pass.

## ⭐ TheEuripidesEnigma → full MATCH: identity-ALR hang + runtime ExplicitlyExclude listing filter + per-command pass-text dedup + map-path DisplayOnce retire  ✅ DONE (2026-07-02)

Surfaced by wiring **TheEuripidesEnigma** (native `WLKTHRGH` solution, full
400-pt win under FD; see `TODO_a5_walkthrough_wiring.md` for the two script
corrections). The corrected script drove Scarier deep into the game, where it
**wedged for >60 s of CPU** entering the north-end storeroom (`cl_Location51`).

**Root cause — exponential ALR recursion on identity ALRs.** The game defines
~25 `TextOverride`s whose `OldText` is a 25-asterisk banner line
(`*************************`) and whose `NewText` is the *same* 25 asterisks
(they exist only to stop ADRIFT mangling the banner rules). A per-turn event's
completion message contains such a banner. Scarier's `replace_alrs`
(`a5text.cpp`) computed `val = process_inner(raw, depth+1)` for every matching
ALR **before** testing its self-reference guard (`!streq(cur, val)`), so
processing "*****(25)" re-matched all ~25 identity ALRs, each recursing
`process_inner` again — branching ~25-fold per level down to the depth-8 cap
(≈25⁸ ≈ 10¹¹ calls). FrankenDrift never hangs because `ReplaceALRs`
(Global.vb:532) does `If sText = sALR Then Exit For` — it bails out of the whole
ALR loop *before* recursing when the entire current text already equals the
(unprocessed) replacement. At the top level `cur` is the full message (≠ the
asterisks) so nothing changes; one level down `cur` has been reduced to exactly
"*****(25)" == `raw`, so the guard fires and terminates.

**Fixed** by porting that guard: `replace_alrs` now computes `raw`
(= `a5text_eval_description(new_text)`, FD's `alr.NewText.ToString`) and, if
`streq(cur, raw)`, `break`s out of the ALR loop *before* the `process_inner`
recursion. Scarier finishes the full win in ~1.8 s; ASan/UBSan-clean; **no corpus
game moved** (all 20 other games hold their exact baselines in both RNG modes,
all a5 unit tests pass incl. the Six Silver Bullets golden).

**Residual 11|11 → 0|0 (full MATCH both modes, golden
`test/TheEuripidesEnigma_expected.txt`; budget re-blessed).** No corpus game
moved in either mode (all 15 other MATCH games hold 0/0; the RNG-bound DIVERGE
games at their exact baselines), all a5 unit tests + `make sanitize` pass, and
the full 400-pt replay is ASan/UBSan-clean to `*** CONGRATULATIONS! ***`.
Three more root causes — and NONE of them was a "prose mentions the object"
listing heuristic (the original guess); FD has no such thing:

1. **The room-view listing filters must read the RUNTIME SetProperty layer**
   (7 of the 11 hunks — the whole "object in prose still auto-lists" family).
   ADRIFT's per-object **`ExplicitlyExclude`** property ("explicitly exclude
   from location descriptions", clsLocation.ObjectsInLocation: a dynamic object
   is listed iff `Not ob.ExplicitlyExclude`; a static one iff
   `ob.ExplicitlyList`) is what suppresses the boom box / drone: their tasks
   run `SetProperty cl_Box1/cl_Drone ExplicitlyExclude <Selected>` exactly when
   the room prose takes over describing them (and `<Unselected>` when it
   stops). Scarier's `view_location_impl` checked the property with
   `a5_prop_find` on the **static model only** — the same
   runtime-override-precedence class as the Amazon `%PropertyValue%` fix.
   **Fixed** (`a5text.cpp object_has_prop_rt`): the runtime `st->ov` override
   wins (`<Unselected>` = removed), else static presence (`a5_prop_find !=
   NULL` — NOT `a5state_entity_prop`'s value, which is NULL for a value-less
   SelectionOnly prop and briefly regressed the #3-crawler listing).

2. **Per-command pass-response TEXT dedup on the direct path** (the extra
   crawler cut-scene block + the doubled cliff-face description). FD keys
   every response of one `AttemptToExecuteTask` by its text
   (htblResponsesPass), so `press on` — whose `cl_PressOnBut` Executes BOTH
   `cl_ToCrawler11` and `cl_ToCrawler12` with *identical* journey paragraphs,
   each ending in `Execute Look` — shows the journey once and the room view
   once. **Fixed** by a `pass_seen` text set on the `exec_resp_scope` (the TBN
   fail-response scope, same lifetime = run_general / attempt_event_task):
   `emit_completion` and the direct-path Look render drop an exact-duplicate
   text, keeping the first occurrence's position. (ev_seen already did this
   for event chains; this extends it to the command path, where FD's response
   map always applied.)

3. **A non-aggregate "Before" completion on the response-map path never
   retired its `<DisplayOnce>` segments** (the squeeze "wording variant" — it
   was no variant: the crevice override's CompletionMessage has a DisplayOnce
   first paragraph "You have to turn sideways but…" then the default "You turn
   sideways and…", and Scarier re-showed the first-time text on every later
   squeeze). FD renders `task.CompletionMessage.ToString` with
   bTestingOutput=False BEFORE the actions (clsUserSession.vb:1167), marking
   DisplayOnce segments even though the response text is pinned/re-rendered
   later; Scarier's map path only rendered via `render_comp_test`
   (marking off). **Fixed** in `run_task`: after the pre-action test render
   (so `resp_pre` keeps the first-time segment), a non-aggregate comp gets a
   marking `a5text_eval_description` pass — segment selection only, no
   `%function%` replacement, so no RNG draws. An aggregate comp keeps the old
   behaviour (its segment choice must stay live for the flush re-render).

## ⭐ LostLabyrinthOfLazaitch 403|403 → 8|0 (xoshiro FULL MATCH): location seen-tracking + ReferencedObjects singular alias + CharHereDesc name-casing + FD's Look message dance  ✅ DONE (2026-07-02)

The 403-hunk diverge collapsed to **vanilla 8 / xoshiro 0** — xoshiro mode (the
every-line conformance check) is now byte-exact through the full 520-point win.
The residual 8 vanilla hunks are pure System.Random-vs-xoshiro `<# OneOf #>`
picks in the riding-room views (the same RNG-bound class as SixSilverBullets
18/0 and JacarandaJim 99/0), so no vanilla golden. Whole corpus re-verified
(18 games MATCH 0/0 both modes incl. AxeOfKolt + GrandpasRanch, DIVERGE games
at their exact baselines), all a5 unit tests + `make sanitize` pass, and the
full walkthrough replays ASan/UBSan-clean to `*** CONGRATULATIONS! ***`.
Four root causes, none of them the TODO's original suspects (the nested
`#A#A(#O#O#)` bracket sequence and the quoted `BeContain` literal were both
fine):

1. **Location seen-tracking was an always-true stub.** `pass_location`'s
   `HaveBeenSeenByCharacter` returned 1 unconditionally, so the FahrenLayb
   teleport child's `Location94 MustNot HaveBeenSeenByCharacter Player`
   *always failed*, printing its restriction message ("You say the words
   "Fahren Layburn" but nothing happens.") instead of teleporting — the whole
   village endgame desynced from there. (The general `SayFahren1` task and its
   bracket sequence had matched fine all along; the failure was inside the
   specific child.) Ported FD's `clsCharacter.HasSeenLocation`: new
   player-centric `st->loc_seen[n_locations]` (a5state), set for the start
   location at init (clsUserSession.vb:222) and on every player move
   (clsCharacter.Move) — MoveCharacter action paths + an update_seen catch-all
   — plus save/restore (`LocSeen` sparse elements) and the real restriction
   evaluation (non-player observer falls back to "seen", same compromise as the
   object handler). This also fixed bug 3 below for free: the "small cottage" /
   "you see a drawing" room-description segments were gated on
   `HaveBeenSeenByCharacter` restrictions.

2. **`sheath sword`: restriction key `ReferencedObjects` didn't resolve from a
   singular `%object%` capture.** FD's `GetReference("ReferencedObjects")`
   carries NO ReferenceMatch condition (clsUserSession.vb:3990): it answers from
   the first Object-type reference whatever its slot, so PutObjInSc's
   `ReferencedObjects Must BeObject Sword` passes on `sheath sword` (command
   `[sheath] %object%`). Fixed in `resolve_key` (a5restr.cpp) as a lookup-time
   fallback ReferencedObjects→ReferencedObject (and Characters analogue).
   **FOOTGUN:** binding the alias at capture time in `bind_reference` instead
   regressed AxeOfKolt (`drop chainmail` succeeded on an out-of-scope object) —
   failed match attempts earlier in the turn leak a stale alias into a later
   task's restrictions. Resolve at lookup, never bind.

3. **Un-gated room-desc segments** — same root cause as 1 (HaveBeenSeen gates).

4. **`white stallion` vs `White Stallion` + the riding `<# OneOf #>` draw
   stream.** Two ViewLocation conformance gaps (a5text/a5run_action):
   - FD name-cases a character's `CharHereDesc` through the `##CHARNAME##`
     round-trip for EVERY group, single characters included
     (clsLocation.vb:160-171: `ReplaceIgnoreCase(desc, Name, "##CHARNAME##")`
     then `.Replace("##CHARNAME##", .List)`), so authored lowercase "white
     stallion" renders as the character Name "White Stallion". Scarier kept
     single-character descriptions verbatim.
   - **FD's Before + AggregateOutput message dance for the Look task**
     (clsUserSession.vb:1164-1207): the message is test-rendered
     (bTestingOutput=True) BEFORE and AFTER the task's actions — *each render
     draws for any `<# OneOf #>`/RAND in the room view* — with the response slot
     reserved before the actions; if the two renders differ (a random pick
     moved) the response is PINNED to the first render, else the raw aggregate
     message survives to be re-rendered once more at final Display (a third
     draw). Scarier's Look bypass rendered once, so the xoshiro draw streams
     desynced on every riding-room view (`OneOf("riding ","riding Molly ",…)`).
     Ported the dance into run_task's Look branch (run->look_pinned carries a
     pinned view to the defer_look splice; resp path reserves its entry index
     pre-actions — that ordering is what keeps Grandpa's tutorial lines after
     the room view). Static room views render draw- and output-identically, so
     the rest of the corpus is untouched.
   Diagnosed by instrumenting both RNGs (`A5_TRACE_RAND=1` in a5rand.cpp,
   `FD_RNG_TRACE=1`/`FD_RNG_STACK=1` in FrankenDrift.Headless XoshiroRandom)
   and diffing the draw streams; the general non-Look Before-dance draw parity
   (random text in ordinary task messages) is NOT ported — no corpus game
   exercises it (they'd already mismatch if one did). If a future game diverges
   with off-by-N OneOf picks in task completion text, port the same dance to
   run_task's ordinary comp paths.

## ⭐ ThingsThatGoBumpInTheNight → full MATCH: BeExactText refine/final two-phase dance + SetTasks-Execute reference pass-through + Execute'd-task fail messages (pass-cancels-fail scope)  ✅ DONE (2026-07-02)

**TBN 8|8 → 0|0** (full MATCH both modes, golden
`test/ThingsThatGoBumpInTheNight_expected.txt`; budget re-blessed) — the game
now plays its native 310-turn WALKTHROUGH byte-for-byte to
`*** CONGRATULATIONS! ***`. No corpus game moved in either mode (all 14 other
MATCH games hold 0/0 — incl. GrandpasRanch, whose golden was the specific
regression the fail-message work had to clear; StoneOfWisdom 2/0, JacarandaJim
99/0, SixSilverBullets 18/0, LostLabyrinth 8/0, Euripides 11/11 unchanged at
their baselines), all a5 unit tests pass (run/arith/parse/dis/walk/obj/save +
Six Silver Bullets golden), `make sanitize` clean, and the full TBN replay is
ASan/UBSan-clean. Four faithful root causes, all in the `BeExactText` /
`SetTasks Execute` response model:

1. **The `drop all` fatal was FD's two-phase `BeExactText` dance, not the
   narrowing tiers.** The library's `RemoveBeforeDrop` helper (P53773,
   ContinueAlways) gates itself with `ReferencedObjects MustNot BeExactText All`
   (restriction 5, NO message). FD's per-item Applicable refine binds a *fresh*
   `clsSingleItem` whose `sCommandReference` is never set
   (clsUserSession.vb:5766), so `BeExactText` always evaluates False there —
   the MustNot PASSES during the refine (the worn/contained items survive) —
   while the *final* post-refine `PassRestrictions` sees the surviving item's
   original `"all"` command reference, fails restriction 5 **silently**
   (`sRestrictionText = ""`), and GetGeneralTask falls through to the real
   `DropObjects` task (P53781), whose `#A#A#A#A#` chain (exists / dynamic /
   not-inside / not-on / held) narrows the bare `all` to exactly the six loose
   held items. Scarier's `resolve_plural` bound the typed text (`$text` alias)
   during the refine too, so **every** item failed restriction 5, the
   none-passed reset blew the reference back up to all 25 seen objects, and the
   fail-message re-evaluation (bound to the notebook, which is inside the bag)
   short-circuited to restriction 4's "You are not holding %objects%.Name."
   **Fixed** in `a5run_ref.cpp resolve_plural`: (a) the Applicable probe binds
   an EMPTY typed text (FD's fresh item); (b) the final choose-and-check pass
   iterates the **refined** item set (`cur`, FD's NewReferencesWorking) instead
   of the original parse — identical for any task without BeExactText (the
   probes re-derive the same set), so the blast radius is exactly the
   BeExactText tasks.

2. **`SetTasks Execute <task> (%objects%)` must pass a same-name reference
   STRAIGHT THROUGH.** FD's SetTasks handler (clsUserSession.vb:2188
   `sParam = sRef`) hands the live `clsNewReference` — items AND their
   `sCommandReference` ("all") — to the executed task, and `ExecuteSubTasks`
   threads each item's command text into the per-item restriction pass. So on
   `get all`, the take chain's `cl_TakeObject` helper (first restriction
   `MustNot BeExactText All`) is a silent per-item no-op, and `TakeObjects`'
   `(#all A #not-inside A #not-on A #not-held A #not-worn) O #not-all` block
   excludes the held bag's / worn jerkin's contents. Scarier's Execute
   arg-binding did `bind_reference(rname, val, val)` — the `$text` became the
   entity KEY — so the gates misfired and `get all` pulled the notebook/pencil/
   fossy/coins OUT of carried containers (3 of the residual hunks, one root).
   **Fixed** in `a5run_action.cpp`: an argument naming the target task's own
   reference skips the rebind entirely (the live binding, key + `$text`, flows
   through); a computed argument (`%ParentOf[..]%`, literal key) binds an
   EMPTY typed text (FD's UserDefinedRef items carry no sCommandReference).

3. **An Execute'd task whose restrictions fail WITH a message must show it.**
   FD's `ExecuteTask` is a full `AttemptToExecuteTask`: the failing
   restriction's `sRestrictionText` is AddResponse'd (bPass=False) into
   `htblResponsesFail`. Scarier's `SetTasks Execute` gate returned silently, so
   the dark ringbolt-room `get dirt` printed nothing where FD emits
   `It is too dark to find the dirt.` (the LightSources restriction message in
   the take chain — cl_TakeCharac passes with no output, both its Execute'd
   take tasks fail on it). **Fixed** with a new per-command
   `exec_resp_scope` (a5run_internal.h; installed in `run_general` and
   `attempt_event_task`, mirroring htblResponsesPass/Fail's lifetime): the
   Execute-fail message is taken from `st->restriction_text` (already set by
   the failing `a5restr_pass` — NO re-evaluation, so no RNG re-draws), deduped
   by text, and **buffered to an end-of-scope flush**.

4. **The flush applies FD's pass-cancels-fail rule** (clsUserSession.vb:804-849:
   a fail response is displayed only when NO pass response carries the same
   reference items — in either order, since FD cancels at Display time). The
   scope records the bound object keys of every completion message emitted with
   output; a buffered fail whose objects all passed is dropped. This is what
   keeps (a) TBN's `get grapnel` clean — the take override passes for the
   hooked grapnel *and hides it*, then the follow-up `Execute TakeObjects`
   fails Visible on that same object (fail after pass) — and (b) Grandpa's
   Ranch `get flashlight` — `cl_TakeObject2` fails "The flashlight is not on
   or inside another object!" BEFORE `TakeObjectFromLocation` passes (fail
   before pass; this was the one corpus regression the eager first version
   hit). When the plural/movement resp_map is active it handles fail entries
   itself (same dedup + item-cancel machinery).

**FOOTGUN kept for later:** FD's `BeExactText` reads the *per-item*
`sCommandReference`, so `drop all except dagger` items still carry `"all"`;
Scarier's `$text` holds the whole reference capture (`"all except dagger"`),
which would evaluate differently. No corpus game exercises it; if one ever
does, thread per-item command texts through `match_objects`.

## ⭐ CallOfTheShaman → full MATCH: `%turns%`/`%Turns%` built-in + markup-aware leading-cap (room-view cap on marked-up buffer; boundary re-cap drops line-start rules)  ✅ DONE (2026-07-02)

**CallOfTheShaman 3→0** (full MATCH both RNG modes, golden
`test/CallOfTheShaman_expected.txt`; budget re-blessed 3|3 → 0|0). No other corpus
game moved in either mode (all 13 MATCH games hold 0/0, incl. anno1700 — the specific
room-view-cap regression risk this fix had to clear; StoneOfWisdom 2/0, JacarandaJim
99/0, SixSilverBullets 18/0 unchanged at their RNG-bound baselines), all a5 unit tests
pass (run/arith/parse/dis/walk/obj/save + Six Silver Bullets golden) and the Shaman
run pipeline is ASan/UBSan-clean. The game plays byte-for-byte through the full
265-point *** CONGRATULATIONS! *** win. The 3 residual hunks (identical in both RNG
modes ⇒ RNG-independent real bugs) were all in the last command's banner/credits:

1. **`%turns%`/`%Turns%` built-in was entirely unimplemented.** FD substitutes it via
   `ReplaceIgnoreCase(sText, "%turns%", Adventure.Turns.ToString)` (Global.vb:1763), so
   `%turns%`/`%Turns%`/`%TURNS%` all resolve. Scarier's `eval_function` (a5text.cpp) had
   no `turns` case at all, so the CONGRATULATIONS text printed the literal `%Turns%`
   where FD renders `151`. **Fixed** by adding a `ci_eq(name,"turns")` case (ci_eq folds
   case, so the capital alias resolves for free). Value = **`st->turns - 1`**: FD's
   Runner bumps `Adventure.Turns` *after* `UserSession.Process()` returns
   (FrankenDrift.Headless/Program.cs:206), so a task's own output sees the pre-command
   count; Scarier bumps `st->turns` at the *top* of `a5run_input`, so the matching value
   is `turns-1` — the same offset `emit_endgame`'s score line already uses. (The
   earlier TODO note guessed a "case-sensitive built-in lookup"; in fact there was no
   built-in at all.)

2. **The credits URLs `Https://.../Http://...` were wrongly leading-capped** (2 hunks).
   The credits lines are `<del>https://lazzah.itch.io` etc. FD runs its
   `CapAfterFullStop` regex `^(?<cap>[a-z])|\n(?<cap>[a-z])|[a-z][.!?] ( )?(?<cap>[a-z])`
   on **still-marked-up** text (Global.vb:539, inside ReplaceALRs at Display), so the
   `<del>` tag's `>` sits immediately before `https` and the `\n(?<cap>...)` alternative
   does **not** fire. Scarier's `a5text_process` (per-fragment) already caps correctly
   on marked-up text (it leaves `<del>https` alone), but Scarier's **Display-boundary
   re-cap** `a5text_display_alr` (needed for ALR games — Shaman has 16 ALRs) runs on
   **plain** text where `<del>` has been stripped, so `\n`+`https` looked like a line
   start → `Https`. The boundary cap can't be dropped: its second ALR round needs
   capitalised input (dropping it regressed anno1700/Amazon/JJ content via ALR
   matching), and its `^`/`\n` line-start rule is what capitalises genuine room-view
   NPC "is here" lines (anno's `The cook and the kitchen maid are here.`, built with
   lowercase articles and joined after a `\n`). **Fixed** in two pieces:
   - **`view_location_impl` (a5text.cpp)** now caps the assembled room view on its
     **still-marked-up** buffer (before `a5text_render_plain`), exactly as FD's
     Display-time cap runs over the whole marked-up turn text. This is where the NPC
     list gets its genuine `\n`-start capital (no intervening tag ⇒ caps), so it no
     longer depends on the boundary.
   - **`a5text_display_alr`** now calls `auto_capitalise_ex(a1, /*allow_line_start=*/0)`
     — the boundary re-cap keeps only the real sentence-punctuation rule (`. `/`! `/`? `,
     which no tag can hide) and drops the `^`/`\n` line-start rules (already applied
     per-fragment on marked-up text). So a stripped formatting tag can no longer expose
     a line-leading word to a spurious cap. Non-ALR games always matched with their
     room-view line starts already upper-cased, so the new room-view cap is a no-op for
     them; the whole blast radius is "an ALR game whose plain-text Display boundary has a
     line-leading lowercase word that FD left alone behind a stripped tag".

## ⭐ SixSilverBullets xoshiro → full MATCH: `RAND()` restriction RHS draws a random + runtime location-group membership + room-view `pSpace`  ✅ DONE (2026-06-30)

**SixSilverBullets xoshiro 10→0** (full every-line MATCH under FD_RNG=xoshiro;
budget re-blessed 20|10 → 18|0) — no corpus game moved in either mode (all 12 MATCH
games hold 0/0; StoneOfWisdom 2/0, JacarandaJim 99/0 unchanged), all a5 unit tests
pass (run/arith/parse/dis/walk/obj/save + the Six Silver Bullets golden) and the
SSB run pipeline is ASan/UBSan-clean. SSB is RNG-bound at vanilla (now 18, like
JacarandaJim's 99): FD's stock System.Random and Scarier's xoshiro128** pick
different chime/bomb flavour text, so the xoshiro column is the real-logic metric
— and it is now clean. The game plays byte-for-byte through to `*** You have won
***` (it previously diverged from ~the Green-Agent encounter onward and the player
was **sniped to a `*** You have lost ***`** roughly two-thirds of the way in).

**The bug was an RNG-stream desync** in the game's per-turn time-trap subsystem.
Three independent root causes, found by instrumenting BOTH engines' RNG draws
(a temporary `A5_TRACE_RNG` in `a5rand_between` / FD's `Global.Random`) and
diffing the consumed stream — the first divergence pinned the cause exactly:

1. **A `RAND(min,max)` expression on a restriction's right-hand side was never
   evaluated as a draw.** SSB's turn ticker (`Maintainen` TurnBased event) runs
   the System task `TimeTrapsT` every turn; its gating restriction
   `Roller Must BeEqualTo 'RAND(1,16)'` draws a fresh `RAND(1,16)` each evaluation
   (FD's `PassSingleRestriction` → `EvaluateExpression` → `clsVariable.SetToExpression`
   → `Global.Random(1,16)`, clsUserSession.vb:4486). Scarier's `num_value`
   (`a5restr.cpp`) did `strtol("'RAND(1,16)'")` = **0**, so `Roller == 0` always
   failed (the chime never fired) AND, fatally, **no random was drawn** — so from
   the first such turn Scarier's `Roller` stream ran ~31 draws behind FD's, and the
   `SniperHits`/bomb/night events (all gated on `Roller`) mistimed, sniping the
   player. **Fixed**: `num_value` strips a surrounding single-quote wrapper and, on
   a `RAND(` token, parses `(lo,hi)` and draws via `a5rand_between` — exactly
   mirroring FD's expression path. With this the two engines' real-draw streams are
   **identical** (75 vs 75, zero diff). The bracket-sequence evaluator already
   short-circuits like FD, so the draw happens iff the prior AND-terms pass —
   matching FD's draw count, not just its values.

2. **`BeWithinLocationGroup` ignored the runtime group-membership override.**
   `TimeTraps1` (the "A bell tolls…" task) fires when the player is in the
   `TimeTraps` location group with `Roller<=2`, then runs
   `RemoveLocationFromGroup LocationOf %Player% FromGroup TimeTraps` so a
   once-tolled room never re-tolls. Scarier's `BeWithinLocationGroup` restriction
   (`a5restr.cpp`) checked only the **static** model members, so the override the
   Remove sets was invisible and The Hotel re-tolled the bell every turn of the
   Green-Agent encounter (3 spurious tolls). **Fixed** by routing it through
   `a5state_object_in_group` (runtime override wins, then static) — the same layer
   `AddLocationToGroup`/`RemoveLocationFromGroup` write and that JacarandaJim's
   dark-locations feature already uses.

3. **`LocationOf %Player%` in the group actions never resolved `%Player%`.** The
   `AddLocationToGroup`/`RemoveLocationFromGroup` handler (`a5run.cpp`) looked up a
   character literally named `"%Player%"` (none exists; the key is `Player`), so the
   Remove collected an empty location set and removed nothing. **Fixed** by passing
   the source key through `act_key` (which maps `%Player%`/`%objectN%` → the entity
   key; a group key is returned verbatim). Needed together with (2) for the Remove
   to take effect.

4. **Room-view internal joins must use `pSpace`, not the sentence-aware
   `add_space`.** FD's `clsLocation.ViewLocation` joins the look-text, each NPC
   "is here" line and the exit list with `pSpace(sView)` (Global.vb:572), which
   appends two spaces **unless the buffer ends in a newline** — it does *not* check
   for a trailing space or sentence punctuation. Scarier used `add_space` there,
   which suppresses the join when the text already ends in a space. SSB's Hotel
   long description ends `"…grim and gray. "` (a model trailing space), so FD
   rendered `"gray.   The Purple Agent is here."` (3 spaces) where Scarier rendered
   1. **Fixed** by replacing the three `add_space` guards in `view_location_impl`
   (`a5text.cpp`) with the newline-only `pSpace` test already used for the
   object-listing joins. Most descriptions end in `.` (where `add_space` and
   `pSpace` agree), so no other corpus game moved.

## ⭐ TreasureHuntInTheAmazon → full MATCH: System `<RunImmediately>` startup tasks + the title's centring space (markup-aware `bHasOutput`)  ✅ DONE (2026-06-30)

**TreasureHuntInTheAmazon 1→0** (full MATCH, both modes; golden
`test/TreasureHuntInTheAmazon_expected.txt` committed) — no other corpus game
moved in either mode (all 12 MATCH goldens hold 0/0, incl. the self-snapshot
SixSilverBullets 20/10 and the movement-heavy GrandpasRanch; StoneOfWisdom 2/0,
JacarandaJim 99/0 unchanged at baseline), all a5 unit tests pass
(run/arith/parse/dis/walk/obj/save + Six Silver Bullets golden) and the whole
run/dump pipeline is ASan/UBSan-clean. This closes the LAST Amazon residual — the
game now plays byte-for-byte through to `*** You have won ***`.

**Symptom.** The centred title rendered flush-left — `Treasure Hunt in the
Amazon` where FD has `   Treasure Hunt in the Amazon` (three leading spaces). The
TODO had deferred this as a "fragile `<center>`-width render detail", but it is
neither console-width-dependent nor a `<center>` artifact.

**Root cause — two coupled gaps.** (1) **Scarier never ran the System tasks
flagged `<RunImmediately>`.** FD's clsUserSession init loop (vb:209-216) runs every
`System AndAlso RunImmediately` task once at game start, BEFORE displaying the
title (vb:226 `Display("<c>" & Adventure.Title & "</c>")`). Their (uncommitted)
output accumulates in `sOutputText`, so the title's `Display` does
`pSpace(sOutputText) & "<c>Title</c>"` and merges them. Amazon's **`PlayTune1`**
(System, RunImmediately, restriction `Musicon = 1`) is the title-music task; its
completion message is **`<audio play src="...Forest-Drama.mp3"> `** (a media tag
plus ONE literal trailing space). So the merged buffer is
`<audio...> ` + pSpace's two spaces + `<c>Title</c>` = **three** leading spaces
once the tags strip. (2) **The audio task's whitespace counted as output in FD but
not in Scarier.** FD's `bHasOutput` (clsUserSession.vb:1272) inspects the message
with markup STILL EMBEDDED: `StripCarats("<audio...> ")` leaves `" "`, which is
non-empty ⇒ output. Scarier's `emit_completion` tested the already-plain-rendered
text with `msg_has_output`, which treats a lone space as nothing and dropped it.

**Fixed (faithful, tightly scoped) in three pieces:**
1. **`<RunImmediately>` parsed** onto `a5_task_t.run_immediately`
   (`a5model.cpp`/`.h`, via `a5xml_bool`).
2. **`run_immediate_tasks` (`a5run.cpp`)** runs every System+RunImmediately task in
   file order at the top of `a5run_intro`, before the title — mirroring FD's init
   loop (skip done-&&-non-repeatable, restriction-gate, `run_task`). Events are not
   yet initialised here (FD starts them after the intro), so no event-completion
   hooks fire. Amazon's `ts_tasInitialise` (sets the day-period var) and
   `StartAllEv` (no actions) run too but produce no visible output and draw no RNG
   — verified by the whole corpus (incl. the just-fixed clock alignment) staying
   put. The **centred title is now emitted by `a5run_intro` itself** (FD's
   `Display("<c>"&Title&"</c>")`), pSpace-joined to the RunImmediately output; the
   dump harness no longer prints the title separately. An empty title still emits
   nothing (RtC).
3. **`fd_has_output` (`a5run.cpp`)** is a faithful port of FD's `bHasOutput` on the
   markup-bearing message (StripCarats-non-empty ⇒ output; else a known
   formatting/media tag or an ALR key ⇒ output). It is used by `emit_completion`
   **only while the `run->immediate_emit` flag is set** (during
   `run_immediate_tasks`); every per-turn completion message keeps the proven
   `msg_has_output` (plain-whitespace) test, so the blast radius is exactly "the
   startup RunImmediately tasks". A first global swap to `fd_has_output` regressed
   8 golden games (whitespace-only mid-turn messages started leaking spaces/blank
   lines) — hence the flag-gating.

**Verification.** Two engines instrumented to confirm the source of the three
spaces: FD's `EmitHtml` receives `<audio...>   <c>Treasure Hunt in the Amazon</c>`
(audio task's 1 space + pSpace's 2). The raw model `<Title>` field has no leading
spaces in either engine; the spaces are purely the PlayTune1 + pSpace + title
merge. Golden committed; Amazon budget re-blessed 1→0.

## ⭐ Event-fired task iterates the leftover plural reference: `get ammo and rifle` ticks `ts_tasIncrement` twice (Amazon off-by-one clock drift)  ✅ DONE (2026-06-30)

**TreasureHuntInTheAmazon 31→1** (both modes; residual 1 = the title's 3-space
`<center>` centring, the separate cosmetic detail below) — **no corpus game moved
in either mode** (all 11 MATCH games hold 0/0; SixSilverBullets' turn timer
UNCHANGED at 20/10, the specific regression risk the TODO flagged; StoneOfWisdom
2/0, JacarandaJim 99/0 unchanged), all a5 unit tests pass
(run/arith/parse/dis/walk/obj/save + Six Silver Bullets golden), budget re-blessed
31→1. This closes the long-deferred "off-by-one-minute clock drift" residual — all
30 drifting `Date:`/time lines (day-5 `By Totem` onward, `10:59`→`11:00`, through
the win-banner `…returned to the camp …Time: 11:01`) now match.

**Root cause — the response-aggregation Display loop leaves `NewReferences` =
the LAST displayed message's reference, and an event-fired task ExecuteSubTasks-
iterates it once PER item.** Confirmed by instrumenting BOTH engines (a temporary
`FD_DBG_TICK` in FD's `AttemptToExecuteTask` logging `CopyNewRefs(NewReferences)`'s
item count; `A5_DBG_TICK` in Scarier logging the leftover `st->ref_items` at
`ev_tick_all`). FD's `ts_tasIncrement` (`IncVariable ts_varMinute = %MinutesPerTurn%`)
is fired each turn by the `ts_evtCallIncrementTime` TurnBased event as a subevent
`ExecuteTask`, which runs through the *same* `AttemptToExecuteTask` as a command:
it does `InReferences = task.CopyNewRefs(NewReferences)` (clsUserSession.vb:756)
then `ExecuteSubTasks` iterates `InReferences` **one item at a time**
(vb:702/714 → `AttemptToExecuteSubTask` per item). The key: after the *command's*
own `AttemptToExecuteTask`, the response Display loop has set `NewReferences = refs`
for each shown message (vb:868) — so the global `NewReferences` ends as the **last
displayed message's** reference. For the plural `get ammo and rifle` the
AggregateOutput take emits ONE merged message ("…the ammunition and the rifle")
whose reference still holds **both** items, so the turn event's `ts_tasIncrement`
runs **twice** → +2 minutes. For `get crown and bottle` the crown (override "(from
the pedestal)") and bottle ("pick up") are **two separate** messages, so the last
leaves a **1-item** reference → +1. Every other Amazon turn leaves 0/1 leftover
items → +1 (instrumentation confirmed `get ammo and rifle` is the ONLY turn in the
whole walkthrough with leftover>1, in BOTH engines). Scarier ticked +1 on the ammo
turn; FD +2 — so from that turn on every clock reading was exactly 1 minute behind.
This is precisely the "per-turn response-aggregation" connection: Scarier's
`resp_flush` *already* leaves `st->ref_items` equal to FD's post-Display
`NewReferences` (verified: 2 items {Ammunition,Rifle} after `get ammo and rifle`,
1 item {BottleOfWh} after `get crown and bottle`) — the only missing piece was the
per-item event-task iteration.

**Fixed** in `a5run.cpp attempt_event_task`: when the leftover reference
(`st->n_ref_items`) holds **>1** items, run the event-fired task once per item
(mirroring run_general's per-item single-reference bind = FD's
`AttemptToExecuteSubTask` `ReDim NewReferences`), each with its own restriction
check; `task_done`/`ev_on_task_completed` fire once at the end (one
`AttemptToExecuteTask`). A 0/1-item leftover keeps the original single, refs-
cleared run, so the overwhelmingly common case (and SixSilverBullets' timer) stays
byte-identical — the entire blast radius is "an event subevent fires in the same
turn a multi-item `%objects%` command's final response reference still holds N>1
items". The inline movement `SetTasks Execute ts_tasIncrement` (the big +30/+45/+66
travel jumps, depth 0) is unaffected — it runs via the direct action path, not
`attempt_event_task`.

**Residual (Amazon 1): the title's 3-space `<center>` centring** — `Treasure Hunt
in the Amazon` (flush-left) vs FD's `   Treasure Hunt in the Amazon`. The 3 spaces
are NOT from the `<Title>` model field (no leading space) nor the headless EmitHtml
(which drops `<center>`/`<c>`); they originate in the Introduction's
`<cls><center><font size=40><c>Treasure Hunt…` first line, where FD's render path
produces a centred (space-padded) title. A cosmetic `<center>`-width render detail,
1 hunk; deferred (fragile to reproduce, depends on a console width).

## ⭐ Per-turn response aggregation on the movement path: `Execute Look` fires `Beforeplay1`, double-`Date:` dedups (Amazon)  ✅ DONE (2026-06-30)

**TreasureHuntInTheAmazon 34→31** (both modes) — no corpus game moved in either
mode (all 12 MATCH goldens hold 0/0, incl. movement-heavy GrandpasRanch;
StoneOfWisdom 2/0, JacarandaJim 99/0, SixSilverBullets 20/10 all unchanged), all
a5 unit tests pass (run/arith/parse/dis/walk/obj/save + Six Silver Bullets
golden), budget re-blessed 34→31.

**The four missing `Date:` lines are all fixed** (the startup `12:04`, the day-3
`cut thicket` `13:59`, the totem carriers-flee re-look `10:54`, the `pour whiskey`
tunnel `14:26`). The residual 31 = the title's 3-space centring (1, separate
`<center>` issue) + the off-by-one-minute drift (30, the *separate* deferred
sub-bug below: the plural `get ammo and rifle` ticks +2 in FD where Scarier ticks
+1 — a per-item event-tick quirk, untouched by this work).

**Root cause — `SetTasks Execute Look` deliberately did NOT fire Look's
`Beforeplay1` override, because without a per-turn message dedup it doubled the
date on every move.** FD's `ExecuteTask(Look)` is a full `AttemptToExecuteTask`
that runs Look's Specific overrides; Amazon's `Beforeplay1` (a Look override)
runs `Execute ts_tasCheckTime`, emitting the `Date:` line. The 4 missing lines
are exactly the `Execute Look` invocations *without* a movement `Beforeplay`
(startup `StartGame`, the cut-thicket/pour-whiskey teleports, the carriers-flee
event re-look) — single Date, no double. But a *movement* turn runs the date
twice (PlayerMovement's `Beforeplay` override AND its `Execute Look` ->
`Beforeplay1`), which FD collapses via its per-turn response dedup
(`htblResponses` keyed by message text, clsUserSession.vb:783).

**Fixed in three faithful pieces (`a5run.cpp`):**
1. **`SetTasks Execute Look` now routes through `execute_task_with_overrides(Look)`**
   — a real `AttemptToExecuteTask(Look)` that fires Look's Before overrides
   (`Beforeplay1` -> the Date) before the room view, and still runs Look's own
   actions (Grandpa's `vnl_TutorialSt` chain). So every `Execute Look` shows the
   date, fixing the 4 standalone cases on the direct path.
2. **`run_general` installs the per-command response map for a *movement* command
   (a `%direction%` reference), not just a plural `%objects%` command.** The map
   dedups the move's two identical `Date:` lines into one (FD's htblResponses).
   Deliberately narrow: routing *all* single commands through the map's
   pass-then-fail reorder perturbs conversation / multi-restriction byte-ordering
   (anno1700's `(to a young woman)`, Grandpa's password) — those have no same-turn
   duplicate and stay on the proven direct path.
3. **The AggregateOutput deferral (store the comp, re-render at flush) is gated on
   being inside a plural iteration** (`resp_add_comp`: `aggregate &&
   !cur_item.empty()`). A movement command's `Beforeplay` "You move north." and
   `ts_tasCheckTime`'s "Date:" render *eagerly* and dedup on text — re-rendering at
   flush is fragile (the `move[//s]` verb conjugation reads transient per-turn
   subject context that has changed by the command's end, so it came out empty).
   Plus a deferred-look `resp_entry` (the room view renders at flush = final
   state, so a move's After children still relocate NPCs correctly) and a
   reference-snapshot on aggregate entries (FD's `NewReferences` reassigned at
   Display) for robust plural `%object2%`/`%character%` resolution.

**Residual (Amazon 31): the off-by-one-minute drift (day 5+, the `By Totem` turn
onward) — UNCHANGED, still deferred.** This is the genuinely separate FD quirk
fully diagnosed below: a plural command whose items stay in one aggregated
reference (`get ammo and rifle`) ticks `ts_varMinute` once *per item* in
`AttemptToExecuteTask`'s event-fired increment, so FD advances +2 where Scarier
advances +1. Faithfully replicating it means making Scarier's event-tick path
iterate the leftover plural `ref_items` per-item (re-running ReduceHung/sleep/etc.
per item) — invasive and risky for other games, so still deferred. Plus the
title's leading-space centring (a `<center>`/`<centre>` render detail).

## ⭐ `%PropertyValue[entity,prop]%` must read the runtime SetProperty override (Amazon iron-key door, broad)  ✅ DONE (2026-06-30)

**TreasureHuntInTheAmazon 42→34** (the whole back half of the game now plays
through to the win) — no other corpus game moved in either mode (all 11 MATCH
games hold 0/0; StoneOfWisdom/JacarandaJim/SixSilverBullets unchanged at
baseline), all a5 unit tests pass (run/arith/parse/dis/walk/obj/save + Six Silver
Bullets golden), budget re-blessed 42→34.

**Symptom.** The *second* sturdy door (the cave exit, unlocked with the iron key)
never opened: `unlock door` produced **no output** and `open door` → "You can't
open the sturdy door as it is locked!", where FD shows **"(using the iron key)" /
"You unlock the sturdy door with the iron key."** The player was then trapped, so
every command from that point (north through the temple → waterfall → camp → win)
diverged — ~the entire remainder of the transcript.

**Root cause — `%PropertyValue[entity,prop]%` read only the *static* model
value, ignoring the runtime SetProperty override layer.** There is a single
`Door1` object (LockKey statically `Key1`, the silver key). On entering Location45
the `CarriersFl1` location trigger runs `MoveObject Key1 ToLocation Hidden` +
`SetProperty Door1 LockKey Key3` (the iron key), retargeting the door's key. The
lazy-unlock chain `UnlockObjectLazy` → `Execute UnlockObWithKey
(%object1%|%PropertyValue[%object1%,LockKey]%)` therefore must resolve the key via
`%PropertyValue[Door1,LockKey]%` = **Key3** at runtime. Scarier's handler
(`a5text.cpp`) did `a5_prop_find (o->props, …)` straight off the static model, so
it returned **Key1** (the silver key, now hidden) — the chain auto-filled a key
the player doesn't hold and silently no-op'd. **Fixed** by checking the runtime
override store (`st->ov`, the same layer `a5state_entity_prop` reads) before the
static lookup; the static `value_node` path still serves text properties. Same
runtime-override-precedence class as the earlier "Object `HaveProperty` ignored
the runtime SetProperty layer" fix — `PropertyValue` was the remaining read site
still on the static-only path.

**Residual (Amazon 34): all `Date:`/time lines — deferred, fully diagnosed
(since DONE 2026-06-30, see the two entries above; Amazon is 0|0).** Two
coupled manifestations, both in the `ts_*` time subsystem; both need the
high-risk per-turn response-aggregation work flagged below ("Per-command response
aggregation … single/movement path"):
  1. **Missing extra `Date:` display lines** (startup `12:04`; the day-3 cut-thicket
     line; the totem carriers-flee re-look; etc.). FD's `Execute Look` runs the
     `Look` override `Beforeplay1` → `Execute ts_tasCheckTime`, emitting a `Date:`
     line that Scarier deliberately suppresses (the documented NOTE in
     `a5run.cpp`'s `SetTasks Execute Look`), because without FD's per-turn
     identical-message dedup, routing it would *double* every move's date.
  2. **Off-by-one-minute drift from day 5 (the `By Totem` turn) onward.** Pinned
     down precisely (instrumented both engines): it is the **plural command `get
     ammo and rifle`** that ticks the clock **+2** where Scarier ticks +1. **It is
     a genuine FD quirk**: when the turn-increment event (`ts_evtCallIncrementTime`
     → subevent `ExecuteTask ts_tasIncrement`) fires in `TurnBasedStuff` *during a
     plural turn*, `AttemptToExecuteTask` builds the increment task's
     `InReferences` via `CopyNewRefs(NewReferences)` — which copies the **leftover
     `%objects%` reference still holding both items** — so `ExecuteSubTasks`
     iterates and runs `ts_tasIncrement`'s `IncVariable ts_varMinute` **once per
     item** (clsUserSession.vb:766 + 711). A plural command whose items all stay in
     one aggregated reference (`get ammo and rifle`) thus advances time by N; one
     whose items split across an override + the general task (`get crown and
     bottle`) leaves a 1-item leftover ref and stays +1. Faithfully replicating
     this means making Scarier's event-fired tasks (`ev_tick_all` → the subevent
     task run) iterate over the leftover plural `ref_items` and execute their
     actions per-item — invasive (touches the whole event-tick path; would also
     re-run `ReduceHung`/`ts_tasSleep`/etc. per-item, matching FD) and risky for
     other games, so deferred. Also note line 1: FD indents the title with three
     leading spaces (`<center>` render); Scarier emits it flush-left.

## ⭐ Return to Camelot → full MATCH: `DisplayOnce` "True" parse + empty-keyword `ContainsWord` + ambiguity-clarifier "does not clarify" + auto-correct prompt prose  ✅ DONE (2026-06-30)

**RtC 21→0** (full MATCH, golden `test/RtC_expected.txt`) — no other corpus game
moved in either mode (all 11 MATCH games now hold 0/0; Amazon/StoneOfWisdom/
JacarandaJim/SixSilverBullets unchanged at baseline), all a5 unit tests pass
(run/arith/parse/dis/walk/obj/save + Six Silver Bullets golden), budget re-blessed
21→0. Four independent root causes, three of them broad:

1. **`<DisplayOnce>` was parsed by a literal `"1"` compare, but RtC serialises
   `True`.** `eval_desc_into` (`a5text.cpp`) did `streq(DisplayOnce, "1")`, so
   RtC's `<DisplayOnce>True</DisplayOnce>` first-visit description segments read
   `once=0` and never short-circuited — `clsDescription.ToString` returns
   immediately after a not-yet-shown DisplayOnce segment (Global.vb:3939), so the
   first-visit segment must win over later `StartDescriptionWithThis` segments.
   With `once=0` the segment didn't terminate and a later (e.g. `Variable5=1`)
   segment overrode it, so every first-visit room/object showed its *subsequent*
   text (corridor guard "The dead body…" instead of "Everything looks normal at
   the first glance…", the dying-guard scene, the rose garden/stairway/tent
   first-visit prose, the slammed-door commotion event). **Fixed** by routing the
   read through `a5xml_bool` (FileIO.GetBool: True/1/-1/Vrai), exported from
   `a5model.cpp` via `a5xml.h`. Same boolean-parse class as the earlier RtC
   `a5xml_bool` sweep — DisplayOnce was the one field still on the `"1"` compare.
   **RtC 19→10** on its own.

2. **`conv_contains_word` treated an empty keyword as match-everything.** FD's
   `ContainsWord` (clsUserSession.vb:3885) splits with VB `Split(x," ")`, which
   KEEPS empty tokens, so an empty-keyword Ask/Tell topic (`<Keywords/>`) splits
   to the single check-word `""`, found only when the subject also has an empty
   token — a real subject like "freeze" never matches it. Scarier used `split_ws`
   (drops empties), so the empty check-list returned "matched all" and RtC's
   keyword-less "ask about igor" Topic6 stole `ask hagrid about freeze` from the
   keyworded Topic7 — Hagrid never handed over the leather pouch of freeze-powder,
   cascading into the whole powder→rose→inventory chain. **Fixed** by mirroring VB
   `Split` (split on a single space, keep empties) in `conv_contains_word`.
   **RtC 10→1**.

3. **A pending-ambiguity clarifier that matched none of the candidates re-asked
   "Which X?" instead of "Sorry, that does not clarify the ambiguity."** FD keeps
   `sAmbTask` set while trying the clarifier as a fresh command (GetGeneralTask
   sets sAmbTask only when it is Nothing, and the second-chance noref pass runs
   only when sAmbTask Is Nothing), so a fresh ambiguity/noref CANNOT override the
   remembered one — only a passing/failing-with-output task claims the turn; then
   `DisplayAmbiguityQuestion` re-runs on the remembered reference narrowed by
   `PossibleKeys`: Count 0 → "Sorry, that does not clarify the ambiguity."
   (clsUserSession.vb:2780), Count >1 → "Which X?". `ask rose for food` after
   "Which rose?" narrows {the roses, the beautiful rose} to 0 (neither matches
   "ask"/"for"/"food") → "Sorry…". Scarier let the re-parsed clarifier raise a
   *fresh* "Which rose?" ambiguity. **Fixed** in `a5run_input`: a `resolving_amb`
   flag captures the narrow count; after `scan_tasks`/`have_fail` (the only paths
   that can claim the turn), the fresh-command ambiguity/cantsee/noref paths are
   suppressed and the remembered ambiguity's DisplayAmbiguityQuestion result is
   emitted (Count 0 → "Sorry…"; Count >1 → re-ask). **RtC 1→0**.

4. **The "Adventure Upgrade" auto-correct prompt prose was never emitted.** FD
   asks once at load (FileIO.vb:634) when the file version < 5.0.26 and a task
   carries an `#A#O#` (AND-then-OR) BracketSequence; the headless ground truth
   answers it only when the next script line is literally yes/no, so RtC (first
   line a real command) gets a NO — the correction is **not** applied (Scarier and
   FD-no read the BracketSequence verbatim, so restriction logic already matched),
   but FD still prints the question. **Fixed** in `a5run_intro`: when
   `atof(version) < 5.000026` and any task's BracketSequence contains `#A#O#`,
   emit the two-paragraph "Adventure Upgrade" prompt before the intro (never
   applying CorrectBracketSequence, matching FD's NO). The dump harness also now
   skips an empty title line (FD emits nothing for RtC's empty title; the blank
   used to be absorbed by the intro's leading blanks under `cat -s`). **RtC 21→19**.

## ⭐ JacarandaJim xoshiro → full MATCH: 0-exit room-view trailing pSpace + the location-darkness feature (group props + ShortLocationDescription)  ✅ DONE (2026-06-30)

**JacarandaJim xoshiro 2→0** (full MATCH under FD_RNG=xoshiro) — no corpus game
moved in either mode (all 10 MATCH games hold 0/0; RtC/Amazon/StoneOfWisdom/
SixSilverBullets unchanged at baseline), all a5 unit tests pass
(run/arith/parse/dis/walk/obj/save + Six Silver Bullets golden), xoshiro budget
re-blessed 2→0. (JJ's vanilla column stays at 99 — RNG-irreducible: at the stock
System.Random the champagne "go for a walk" and the Group7 follower rooms draw a
different stream than Scarier's xoshiro128**, so vanilla can't MATCH; the xoshiro
column is the real-logic-bug metric and is now clean.) Two independent root causes:

1. **The room view's trailing two-space pSpace was skipped when a room has NO
   exits.** `clsLocation.ViewLocation` calls `pSpace(sView)` **unconditionally**
   (clsLocation.vb:177) *before* the `iExitCount` check, so a 0-exit room ends
   with two dangling trailing spaces; a same-turn event message then joins onto it
   with another pSpace → **four** spaces. Scarier's `view_location_impl`
   (`a5text.cpp`) only emitted the pSpace *inside* the `n >= 1` branch, so the
   police-cell `look` (no exits) → "...way out of the cell.  Alan appears…" (2
   spaces) where FD has 4. **Fixed** by hoisting the `add_space`/"  " emission out
   of the `n >= 1` branch to before it, matching vb:177. (The dangling spaces are
   only ever visible when something follows the view on the same turn — a
   turn-event message; a view that ends a turn has them stripped by the
   trailing-whitespace normalisation, so no other game moved.)

2. **The location-darkness feature was entirely unimplemented** (the deferred
   multi-part feature from this file's old JJ residual). The ADRIFT standard
   library models darkness as a **`ShortLocationDescription` location property**
   (a `Description` whose dark alternate "Everything is dark" is
   `StartDescriptionWithThis` + restricted `%Player% MustNot
   BeInSameLocationAsObject LightSources`) assigned to a **`DarkLocations`
   Locations-group**; `clsLocation.ShortDescription` (vb:62) appends that
   property's alternates onto the base short desc so a dark member's NAME becomes
   "Everything is dark". JJ's day/night events run `AddLocationToGroup
   EverywhereInGroup Group2 ToGroup DarkLocations` at dusk (matching Remove at
   dawn), so at night every outdoor location (incl. Location13, where the
   champagne sleep lands) inherits the dark name. **Implemented faithfully in four
   pieces:**
   - **Runtime location-group membership** reuses the generic
     `a5state_object_in_group`/`set` override store (`@InGroup:<grp>`), keyed by
     the location key — saves/restores for free.
   - **`AddLocationToGroup`/`RemoveLocationFromGroup` actions** (`a5run.cpp
     run_action`): the `Location`/`LocationOf`/`EverywhereInGroup`/
     `EverywhereWithProperty` selectors (clsUserSession.vb:1917) build the location
     set, then set/clear each location's membership in the target group.
   - **Location group-property inheritance** (`a5state_location_group_prop`,
     `a5state.cpp`): scans every Locations-type group whose runtime-or-static
     membership includes the location and returns the last `propkey` match (FD's
     later-property-group-wins).
   - **Combined short-description render** (`a5text.cpp`): `eval_desc_into`
     factored out of `a5text_eval_description` threads ToString's
     `first`/`default_text`/DisplayOnce-terminate state across several `<Description>`
     wrappers, so `a5text_location_short` concatenates the base `<ShortDescription>`
     with the inherited `ShortLocationDescription` property's segments exactly as
     FD's `oShortDesc.Copy` + appended `StringData`. All short-NAME read sites are
     routed through it (`%LocationName%`, the `%DisplayLocation%`/look room name,
     and the `.ShortDescription`/`.Name` OO reads) — non-dark locations resolve a
     NULL property and evaluate base-only (byte-identical to before), so the blast
     radius is exactly "a location that is a member of a Locations-group carrying
     ShortLocationDescription, with the dark restriction passing".

   Now the champagne teleport renders FD's "…to find myself everything is dark."
   (the `%LCase[%LocationName%]%`) under the "Everything is dark" room header,
   both resolved through the inherited dark property.

## ⭐ XML text content must normalise line endings (CR-LF / lone CR → LF), per XML 1.0 §2.11 (broad)  ✅ DONE (2026-06-30)

**JacarandaJim 3→2 (xoshiro)**, SixSilverBullets snapshot re-blessed (CR-LF→LF in
the title art, now matching FD byte-for-byte) — no corpus game moved in either
mode (all 10 MATCH games hold 0/0), all a5 unit tests pass
(run/arith/parse/dis/walk/obj/save + the re-captured Six Silver Bullets golden).

**Symptom.** A stray carriage return leaked into the rendered transcript:
`...Alan looks very pleased with himself.\r  It beings to rain.` where FD has
`...himself.  It beings to rain.` (the `\r` displays as a spurious line break in
the diff). Exactly one mid-line `\r` survived in the whole JJ transcript.

**Root cause — Scarier's hand-rolled XML parser never applied XML 1.0 §2.11
line-ending normalisation.** The model field that holds Alan's random walk
messages is a **CR-LF-separated list** stored verbatim in the XML text node
(`...looks smug.\r\nAlan looks very pleased with himself.\r\nAlan scratches...`).
A conformant XML parser (.NET's `XmlReader`, which FrankenDrift relies on)
translates every `\r\n` and stand-alone `\r` to a single `\n` *before* handing
the value to the application, so when FD splits the list on `\n` each option is
clean. Scarier kept the `\r`, so the chosen option was `"...himself.\r"`; its
trailing `\r` is not `\n`, so `sb_pspace` (FD's `pSpace`, which only special-cases
a trailing `\n`) appended its `"  "` *after* the `\r`, leaking it through. (FD's
headless `EmitHtml` also does a `\r\n`→`\n` pass, but that is redundant after the
XML-layer normalisation — a lone `\r` followed by spaces would otherwise survive
it too.)

**Fixed** in `a5xml.cpp a5_decode_entities` (the in-place pass already run over
every element's text content): a `\r` is now emitted as `\n`, collapsing a
following `\n` so `\r\n` becomes a single `\n` — matching `XmlReader`. Output
never grows, so the in-place rewrite stays valid. This also strips the (harmless,
trailing-whitespace-stripped) `\r`s that Scarier used to emit elsewhere; the only
visible golden affected was the Six Silver Bullets self-snapshot, whose title
ASCII-art carried `\r\n`s that **FD renders as plain `\n`** (FD transcript has 0
CRs) — the snapshot was re-captured to the now-FD-matching LF form (diff with CRs
stripped is byte-identical, confirming the change is purely CR-LF→LF).

**Residual (JacarandaJim xoshiro 2): ✅ BOTH FIXED 2026-06-30 — see the new top
entry "JacarandaJim xoshiro → full MATCH". JJ xoshiro is now 0.** (a) the police-cell `look` renders
`...way out of the cell.  Alan appears...` (2 spaces) where FD has 4 — a
room-view trailing-space detail (the description text ends cleanly at "cell." in
the model, so FD's extra 2 spaces come from the ViewLocation/event join, not the
text). (b) the champagne "go for a walk" teleport: **both engines land in the
SAME room** (Location13, the marketplace — identical RNG pick, identical body
text), but FD renders its short name as "Everything is dark" where Scarier shows
"Outside the Vegetable shop". **This is NOT an RNG/move bug — it's an unimplemented
location-group-property + darkness feature** (fully diagnosed below; deferred as a
multi-part feature, low payoff since JJ is RNG-bound at vanilla 99 and can't MATCH):

  - The ADRIFT standard library models darkness via a **`ShortLocationDescription`
    location property** whose value is a `Description` restricted to `%Player%
    MustNot BeInSameLocationAsObject LightSources`, text "Everything is dark".
    FD's `clsLocation.ShortDescription` getter (clsLocation.vb:62) appends that
    property's alternates onto the base short desc, so when the player has no
    light the dark alternate wins (the room NAME becomes "Everything is dark";
    the LONG description is unaffected — there is no matching
    `LongLocationDescription`, hence FD's full marketplace body under the dark
    header).
  - The property is assigned to a **group** (`DarkLocations`), and ADRIFT
    **group properties apply to every member**. Location13 is not a static
    member, but a day/night event runs `AddLocationToGroup EverywhereInGroup
    Group2 ToGroup DarkLocations` (with a dawn `RemoveLocationFromGroup`), so at
    night every outdoor (Group2) location — including Location13 — is in
    `DarkLocations` and inherits the dark-name property. The champagne sleep
    happens at night, so Location13 is dark.
  - **Scarier implements none of this**: `AddLocationToGroup`/
    `RemoveLocationFromGroup` are unimplemented (`a5run.cpp:3256` is a stub
    comment), there is no runtime **location**-group membership (only the
    object-group layer from the P6 fix), location `HasProperty`/`GetProperty`
    does not consult group-assigned properties, and the location short-desc
    renderer (`a5text.cpp` `view_location_impl`, reads `ShortDescription`
    straight off the XML node) has no `ShortLocationDescription`/
    `LongLocationDescription` override. Faithful port = (1) runtime location-group
    membership (mirror `a5state_object_in_group`), (2) the two
    Add/RemoveLocationToGroup actions incl. the `EverywhereInGroup` selector,
    (3) location group-property inheritance, (4) the `ShortLocationDescription`/
    `LongLocationDescription` append-and-evaluate in the short/long-desc render
    (mirror clsLocation.vb:62/79). Touches the location-render + property core, so
    verify against all 10 MATCH goldens.

## ⭐ Run Bronwynn Run → full MATCH: `%objects%` reference model — single-noun-matches-many ambiguity, and-form hard-fail, second-chance ambiguity yields  ✅ DONE (2026-06-30)

**RunBronwynnRun 3→0** (full MATCH, golden `test/RunBronwynnRun_expected.txt`) —
no other corpus game moved in either mode (10 games MATCH), all a5 unit tests pass
(run/arith/parse/dis/walk/obj/save + Six Silver Bullets golden), budget re-blessed
3→0. Three independent root causes, all in the `%objects%` plural-reference model
and all faithful to FD's `InputMatchesObjects`/`RefineMatchingPossibilitesUsing
Restrictions`/`GetGeneralTask`:

1. **A single noun in an "X and Y" list that name-matches several objects collapses
   into ONE ambiguous Item (Count>1) — `resolve_plural` lost it.** FD's
   `InputMatchesObjects` (clsUserSession.vb:5305) parses `%objects%` two ways: the
   *bare-plural* path (`all`, `all <plural>`, a plural-noun match → `bPlural=True`)
   spreads each object into its own single-possibility Item, but the *"X and Y" /
   comma* path matches each chunk with the **singular** `InputMatchesObject`
   (`bPlural=False`), which appends every name-match of that chunk to the **same**
   Item (vb:5466) — so a chunk matching >1 object is a Count>1 ambiguity. `wear
   leathers and boots`: the "leathers" chunk matches two "riding leathers" objects →
   Item Count 2. FD refines every Item through Applicable/Visible/Seen, resetting the
   whole reference to its original Items whenever a tier empties them all
   (vb:5825-5959); both leathers are unseen+invisible so every tier empties → the
   reference resets to original → the Count 2 Item survives → `GetGeneralTask` raises
   it as `sAmbTask` → `DisplayAmbiguityQuestion` → **"You can't see any leathers!"**
   (none visible). Scarier's `resolve_plural` (`a5run.cpp`) collapsed each Item to a
   single key via `pick_item_key` before checking restrictions, discarding the
   ambiguity, so it ran the task to a restriction-fail message ("...you're referring
   to."). **Fixed**: `resolve_plural` now mirrors the tiered per-Item refine
   (Applicable→Visible→Seen, reset-to-original on a tier that empties every Item) over
   the candidate sets and, if an Item survives Count>1, returns `RR_AMBIG`/`RR_CANTSEE`
   (the resolved/none-passed machinery below runs only when every Item is unique). The
   bare-plural path stays Count-1-per-Item, so `take all` / `show documents` are
   unaffected.

2. **An "X and Y" list one of whose chunks names NO object must hard-fail the task
   match — Scarier fell through to a single-object resolution + second-chance no-ref.**
   FD's and-form does `If Not InputMatchesObject(chunk, …) Then Return False`
   (vb:5366-5371) — returning False *without* the single-noun second-chance
   `HasObjectExistRestriction` fallback (that fallback lives only on the final
   single-`InputMatchesObject` line, vb:5470, never reached once the and-regex
   matched). So `get fleetwind saddle and fleetwind bridle` (neither chunk names an
   object) → the take task does **not** match → **"Sorry, I didn't understand that
   command."**. Scarier's `match_objects` returned false, and `resolve_refine` then
   re-tried the *whole* string through `resolve_object_candidates` → empty → the
   second-chance no-ref path → the take task's "...trying to take." **Fixed**:
   `match_objects` sets a new `hard_fail` out-param when an and/comma chunk matched
   nothing; `resolve_refine`'s `%objects%` branch returns `RR_NOMATCH` outright on
   `hard_fail` (no `resolve_object_candidates` retry, no no-ref registration). A plain
   single-noun no-match (`get xyzzy`) is unchanged — it still gets the second-chance
   no-ref, as FD's final single-`InputMatchesObject` line does.

3. **A *second-chance*-pass task's sibling ambiguity must YIELD to another task's
   clean no-reference message.** `remove uniform from dummy`: `TakeFromCh1`
   (`%objects% from %character%`) is ambiguous on "uniform" (2 objects) but its
   "dummy" names no **character** — so in FD the task matches only in the
   second-chance (existence) pass, where the ambiguous-Item sets `sAmbTask` but a
   *different* task, `RemoveObjects` (`[remove/take off] %objects%`, "uniform from
   dummy" → one unmatched 0-Item ref), reaches `GetGeneralTask = sNoRefTask` and so
   **wins** (DisplayAmbiguityQuestion fires only when `sTaskKey Is Nothing`,
   vb:3410) → **"Sorry, I'm not sure which object you're referring to."** Scarier
   surfaced `TakeFromCh1`'s ambiguity as a *first-pass* `RR_CANTSEE`
   ("You can't see any uniforms!"), which preempts the no-ref. **Root cause**:
   `resolve_refine`'s post-refine ambiguity loop flagged `amb.second_chance` only via
   the `noref_required` branch; a task with an unmatched *optional* (bracket-truncated)
   reference plus an ambiguous sibling stayed `second_chance=0` (first-pass).
   **Fixed**: the post-refine ambiguity now sets `amb.second_chance = have_noref` —
   any task that matched only because some reference name-matched nothing (exist
   fallback) produces a *second-chance* ambiguity, which `a5run_input` orders **after**
   the clean `sNoRefTask`. A pure first-pass ambiguity (no unmatched ref, e.g. `press
   button`) stays `second_chance=0` and still preempts the no-ref. (`pour oil on me`
   in DieFeuerfaust — `have_noref` with a truncated object2 — becomes second_chance but
   has no competing no-ref, so its "You can't see any oil!" still surfaces; golden
   unchanged.)

## ⭐ SpectreOfCastleCoris + AxeOfKolt → full MATCH: HighestPriorityPassingTask keeps the *highest* failing task + empty-article indefinite leading space  ✅ DONE (2026-06-30)

**SpectreOfCastleCoris 3→0, AxeOfKolt 4→0** (both full MATCH, goldens
`test/SpectreOfCastleCoris_expected.txt` / `test/AxeOfKolt_expected.txt`) — no
other corpus game moved in either mode, SixSilverBullets held at baseline (no
mis-tick), all a5 unit tests pass (run/arith/parse/dis/walk/obj/save + Six Silver
Bullets golden), budgets re-blessed. Two independent root causes:

1. **Indefinite `FullName` of an empty-article object dropped the leading space.**
   FD's `clsObject.FullName` (clsObject.vb:324) renders an Indefinite article as
   `sArticle2 = sArticle & " "` — the space is appended **even when the article is
   empty**, so an empty-`<Article/>` object renders with a leading space. Spectre's
   `Clothes` object has an empty article and prefix "your", so the inventory line's
   `%Player%.Worn(False).List(Indefinite, False)` yields `" your clothes and the
   bottomless bag"` (leading space) → `"You are wearing  your clothes…"` (double
   space after "wearing"). Scarier's `a5text_object_name` (`a5text.cpp`) only
   emitted the article-space for a **non-empty** article, so it rendered a single
   space. **Fixed**: the INDEFINITE branch now always appends the space (the
   article when present, then `' '`), matching FD's `sArticle & " "`.

2. **Under `HighestPriorityPassingTask`, FD surfaces the *highest*-priority failing
   task; Scarier kept the *first* (lowest).** FD's `GetGeneralTask`
   (clsUserSession.vb:6076) reassigns `GetGeneralTask = tas.Key` for **every**
   command-matching task that fails *with output* (`sRestrictionText <> ""`) without
   `GoTo FoundTask` in this mode, so the last (highest-priority) one wins; the
   loop-top `Not (LowPriority AndAlso Priority > iPriorityFail)` guard (vb:5981)
   skips LowPriority tasks once a fail floor is set. Scarier's `scan_tasks`
   (`a5run.cpp`) recorded only the *first* failing-with-output task (the documented
   Axe `say`/`tell` residual). **Fixed** in three faithful pieces:
   - `scan_tasks` loop top: under `hp_passing`, once a fail-with-output is recorded,
     skip a LowPriority task above the fail floor (the iPriorityFail guard). This is
     what keeps **SixSilverBullets'** turn timer aligned — a bare keep-highest
     without it ticked early (bell-toll/sniper events fired prematurely).
   - The RR_FAIL recording overwrites to the highest under `hp_passing` (run->order
     is ascending), keeps the first within the priority band under the default
     HighestPriorityTask. Fixes Spectre `give bottle` ("You can't see the
     linctus!" not "You are not carrying the linctus.") and Axe's `say`/`tell` hunks.
   - The RR_NOREF (second-chance Must-Exist) path likewise overwrites to the
     highest under `hp_passing`, **gated on the new task producing a non-empty
     message** (`noref_has_output`, mirroring FD's `sRestrictionText <> ""`).
     Spectre's `remove bricks` matches both RemoveObjects (P50736, "…you're
     referring to.") and RemoveObje (P50749, "…trying to remove."); FD surfaces the
     latter. The output gate keeps Axe's `examine <unknown noun>` on its "You see
     no such thing." rather than yielding to a higher refless task that fails
     silently and drops the turn to NotUnderstood.

This closes the long-standing Axe `say to <absent char>` / `tell <unseen char>`
residual (the TODO note's predicted bonus) and Spectre's last three hunks.

## ⭐ MagneticMoon → full MATCH: end-game score by KEY + second-chance ambiguity yields to clean no-ref  ✅ DONE (2026-06-30)

**MagneticMoon 2→0** (full MATCH, golden `test/MagneticMoon_expected.txt`); no
other corpus game moved in either mode, all a5 unit tests pass
(run/arith/parse/dis/walk/obj/save), budget re-blessed 2→0. Two independent
root causes:

1. **The end-game score line read the Score variable by NAME, not KEY.** FD's
   `Adventure.Score`/`MaxScore` (clsAdventure.vb:242/260) read
   `htblVariables("Score")` / `htblVariables("MaxScore")` — keyed by the
   variable **Key**. MagneticMoon's score variable has key `Score` but a
   user-facing **Name** `__Points_Scored`, so Scarier's `emit_endgame`
   (`a5run.cpp`) `a5state_var_num_by_name(st, "Score", …)` matched no variable
   and the line always printed **"…you scored 0…"** (the local default) even
   though the variable held 10. **Fixed** by resolving Score/MaxScore through
   `a5state_variable_index` (key lookup) and reading `var_num[idx]`, mirroring
   FD's keyed `htblVariables` access. (The per-task `SetVariable Score …`
   *writes* already used the key index, so the value was correct — only the
   final read was wrong.) MagneticMoon score `0→10`.

2. **A second-chance ambiguity wrongly preempted a sibling task's clean
   no-reference fallback.** `tie wire to girder` (wire is out-of-scope-
   ambiguous → cantsee; girder names nothing) → Scarier surfaced `TieObjectT`
   (`[tie/bind] %object1% [to] %object2%`)'s `%object1%` no-ref message
   "…trying to **tie**.", where FD surfaces the simpler `TieObject1`
   (`[tie/bind/secure] %object%`)'s "…trying to **tie/bind/secure**.". In FD a
   task whose unmatched reference is *required* (`Must Exist` inside the
   evaluated bracket) never matches in the **first** pass; it is found only in
   the **second-chance** (existence) pass, where its *ambiguous sibling*
   (`%object1%`="wire", >1 candidate) sets `sAmbTask` — but a **different**
   task's 0-item Must-Exist failure (`TieObject1`, GetGeneralTask) wins over
   sAmbTask. Scarier's `resolve_refine` returned `RR_NOREF` outright for
   `TieObjectT` (noref_required short-circuit, the DieFeuerfaust `blow dart`
   path), so it became the no-ref winner. **Fixed**: when `noref_required` and
   a *sibling* reference is still ambiguous (>1), `resolve_refine` now returns
   that sibling's `RR_CANTSEE`/`RR_AMBIG` flagged `amb.second_chance` instead
   of `RR_NOREF`; `scan_tasks` lets a genuine first-pass ambiguity replace a
   recorded second-chance one; and `a5run_input` orders a **second-chance**
   ambiguity *after* the clean `sNoRefTask` (so `TieObject1`'s message wins),
   falling back to the ambiguity only when nothing claimed the turn. A
   first-pass ambiguity (`pour oil on me`, whose object2 is *optional*) and a
   clean required no-ref (`blow dart at <absent>`) are both unchanged.

## ⭐ Plural `%objects%` none-pass reset must follow FD's Visible/Seen tiers (RevengeOfTheSpacePirates → full MATCH)  ✅ DONE (2026-06-30)

**RevengeOfTheSpacePirates 1→0** (full MATCH, golden
`test/RevengeOfTheSpacePirates_expected.txt`) — no other corpus game moved, all
a5 unit tests pass (arith/parse/dis/walk/obj/save + Six Silver Bullets golden),
budget re-blessed.

**Symptom.** `show documents` at customs → Scarier **"You will have to take the
travel documents and the documents out of whatever it is in first."**, FD **"…take
the travel documents out…"** (only the in-scope object). The game has two objects
named "documents": `Documents` ("travel documents", held by the player) and
`Documents1` (a second "documents", `Part of Object` the distant `Cabinets` at
Location45 — out of scope here). `show documents` (GiveObject2 `[show] %objects%`)
matches both by name; FD renders only the visible one in the R5 `ReferencedObjects
MustNot BeInsideObject AnyObject` message.

**Root cause — `resolve_plural`'s none-passed branch reset to the *full* key set,
skipping FD's tiered Visible/Seen refinement.** FD's
`RefineMatchingPossibilitesUsingRestrictions` (clsUserSession.vb:5734) spreads each
matching object into its own single-possibility Item, runs the **Applicable** tier
(keep items whose key passes all restrictions). Here both fail Applicable — Documents
fails R5 (it's inside something), Documents1 fails R3 `Must HaveBeenSeen` (never
seen) — so the whole reference empties and FD **resets to the full set and refines by
Visible** (vb:5848-5912): each emptied single-possibility item is dropped, so the
out-of-scope `Documents1` falls away and only the visible `Documents` survives. Only
if Visible empties *every* item does it reset again and refine by **Seen**; only if
Seen empties everything too does the full set survive. Scarier's `resolve_plural`
collapsed all three tiers into a blunt `chosen = all_keys`, so both documents stayed
in `ReferencedObjects` and R5 listed both.

**Fixed** in `a5run.cpp resolve_plural`: the none-passed reset now mirrors the tiered
fallback — `chosen` = the **Visible** subset of `all_keys` (`obj_visible`), else the
**Seen** subset (`obj_seen_p`), else the full set. The Applicable-survivors path
(some items pass) is unchanged, as is the `had_all`/FailOverride path (its
keys are already seen-filtered by `expand_all_objects`).

**Result:** Revenge 1→0 (full MATCH, golden captured). No other corpus game moved in
either mode; all a5 unit tests pass; budget re-blessed 1→0.

## ⭐ FD-faithful container-open visibility + seen-tracking (closed opaque containers hide contents, broad)  ✅ DONE (2026-06-30)

**SpectreOfCastleCoris 4→3, RunBronwynnRun 4→3** (both vanilla and xoshiro) — no
other corpus game moved, all a5 unit tests pass (arith/parse/dis/walk/obj/save +
Six Silver Bullets golden), budgets re-blessed.

**Symptom.** An object lying *inside a closed container* was treated as visible /
in scope and got marked **seen**, where FD hides it. Spectre: examining the money
(inside a closed container) → Scarier rendered its description **"A sum of money
for your everyday expenses."**, FD **"You see no such thing."** (never seen ⇒
`HaveBeenSeenByCharacter` fails). RunBronwynn: the `riding leathers` (in a closed
container) wrongly resolved in scope, flipping the `wear/take` outcome.

**Root cause — Scarier had only the raw `ExistsAtLocation`, never FD's
`BoundVisible`.** FD models object location two distinct ways: `ExistsAtLocation`
(clsObject.vb:270, raw containment — for `InObject` it just recurses into the
container) and `BoundVisible`/`IsVisibleAtLocation` (clsObject.vb:776/844), which
for the `InObject` case (vb:804) returns the *container's own key* (⇒ not at any
room) unless `Not Openable OrElse IsOpen OrElse IsTransparent`. So an object is
hidden iff its container is **openable AND closed AND opaque** (`Openable` =
`HasProperty("Openable")`; `IsOpen` = the `OpenStatus` property is absent or
`"Open"`; `IsTransparent` is hard-coded `False`, vb:308). Scarier's
`a5state_object_at_location` (`exists_at`) implemented only the raw recursion and
was used *everywhere* — including the scope/visibility and end-of-turn seen passes
that FD drives off `CanSeeObject`/`IsVisibleTo`/`IsVisibleAtLocation` (all
`BoundVisible`-based; the seen mark is `PrepareForNextTurn` `ch.CanSeeObject(ob)`,
clsUserSession.vb:3778).

**Fixed (faithful, zero-regression):**
1. **`a5state.cpp` — `obj_hides_contents(parent)`**: openable (HasProperty, runtime
   override winning) AND `OpenStatus` present-and-not-`"Open"`. Mirrors
   clsObject.vb:804's hide condition.
2. **`a5state.cpp` — `exists_at` gained a `visible` flag**; the `IN_OBJECT` case,
   when `visible`, returns the container key (⇒ hidden at any room) instead of
   recursing if `obj_hides_contents`. `ON_OBJECT`/supporter and held/worn always
   recurse (FD's BoundVisible never hides those). New public
   `a5state_object_visible_at_location` (visible=1); `a5state_object_at_location`
   stays raw (visible=0).
3. **Routed only the BoundVisible-based callers through the visible variant**:
   `obj_in_scope` + `resolve_object_candidates`' visible bucket (`a5run.cpp`),
   `update_seen` (the seen mark, `a5run.cpp`), object `BeVisibleToCharacter`
   (FD `CanSeeObject`) and character `BeInSameLocationAsObject` (FD
   `CanSeeObject`, incl. its group expansion) in `a5restr.cpp`. The raw
   `ExistsAtLocation` callers FD keeps raw are **unchanged**: object
   `BeAtLocation` (vb:4151), the OO `%object%.Location`/location-roots reads
   (`a5expr.cpp`), and the room-render `directly=1` lists (`a5text.cpp`,
   FD `ObjectsInLocation(..., bDirectly:=True)`).

**Result:** Spectre 4→3 (the `examine <object-in-closed-container>` →
"You see no such thing." hunk), RunBronwynn 4→3 (the closed-container
`riding leathers` scope hunk). No other corpus game moved in either mode; all a5
unit tests pass; budgets re-blessed 4→3 (both columns).

## ⭐ `AmbWord` must match names case-sensitively and fall back to empty (FD quirk, broad)  ✅ DONE (2026-06-30)

**SpectreOfCastleCoris 8→4** — no other corpus game moved, all a5 unit tests pass
(arith/parse/dis/walk/obj/save + Six Silver Bullets golden), budget re-blessed.

**Symptom (4 hunks).** `buy ale` / `buy arcani` (out of scope) → Scarier `You can't see
any ale!` / `...any arcani!`, FD `You can't see any !` (the noun rendered **empty**).

**Root cause — `clsUserSession.AmbWord` (vb:2656) compares the input word to each
candidate's `arlNames`/ProperName/descriptors CASE-SENSITIVELY (`sWord = sName`) and
returns `Nothing` (→ "") when no input word names *every* candidate.** `buy ale`
matches two objects, one named **"Ale"** and one **"ale"**; the lowercase input word
"ale" is not a case-exact name of the "Ale" object, so AmbWord finds no common word
and the `DisplayAmbiguityQuestion` cantsee (Count>1, none visible) renders `You can't
see any !`. Scarier's `amb_word` lowercased both sides (so "ale" matched both) and fell
back to the **raw text** ("ale") when nothing was common — both un-faithful.

**Fixed** in `a5run.cpp amb_word`: compare each input word to the candidates' whole
`o->names` (objects) / `c->name` + `c->descriptors` (characters) **case-sensitively**,
and return **""** (not the raw text) when no common name is found — mirroring FD's
`sWord = sName` and `AmbWord = Nothing`. (Same family as the known-words case quirk
above: the input is lowercased upstream, model names keep their original case.)

**Result:** Spectre 8→4 (the four `buy <out-of-scope>` cantsee lines now render the
empty noun). The `Which <word>?` prompt path uses the same helper and stays faithful
(an empty common word → "Which ?", as FD). No regression in any MATCH game (their
ambiguity/cantsee goldens — StarshipQuest/anno1700 — are unchanged); all a5 unit tests
pass; budget re-blessed.

## ⭐ Known-words validity check must keep model words' ORIGINAL case (FD quirk, broad)  ✅ DONE (2026-06-30)

**RevengeOfTheSpacePirates 2→1** — no other corpus game moved, all a5 unit tests pass
(arith/parse/dis/walk/obj/save + Six Silver Bullets golden), budget re-blessed.

**Symptom.** `insert crystal in bracket` (no matching task) → Scarier `I did not
understand the word "bracket".`, FD `I did not understand the word "crystal".` Both
words are "unknown"; the engines disagree on *which* is the first unknown one.

**Root cause — FD's `listKnownWords` keeps each model word's ORIGINAL case**
(clsUserSession.vb:3489-3531 adds task-command verbs, object/character articles,
prefixes, names and descriptors verbatim; only the character ProperName is
`.ToLower`-ed), and the validity loop compares the **lowercased** input word against
it with a case-sensitive `List.Contains` (vb:3534-3540, sInput already `.ToLower`-ed
at vb:3356). The object is named **"Crystal"** (prefix "Zykon"), so the lowercase
input word "crystal" is NOT a known word and FD rejects it first. Scarier's
`build_known_words` lowercased every word, so "crystal" matched and Scarier instead
rejected the next unknown word, "bracket". (`take crystal` earlier matched the take
task and never reached this check, so the quirk only shows on an unmatched command.)

**Fixed** in `a5run.cpp build_known_words`: insert command verbs and object/character
articles/prefixes/names/descriptors with original case (drop the `lower()`), keep
`lower()` only on the character proper name (FD's sole lowercased field) and on
directions/"and" (already lowercase). `not_understood`'s lookup still lowercases the
input word, reproducing FD's lowercase-sInput vs mixed-case-known comparison.

**Result:** Revenge 2→1 (residual 1 = the `show documents` plural binding "travel
documents and the documents", a `%objects%` dedup issue). No regression in any MATCH
game (their unmatched-command word-rejection lines are unaffected, or already
diverged-and-absent); all a5 unit tests pass; budget re-blessed.

## ⭐ `BeInSameLocationAsCharacter AnyCharacter` (and `AnyCharacter`-subject) unhandled (broad)  ✅ DONE (2026-06-30)

**RevengeOfTheSpacePirates 3→2** — no other corpus game moved, all a5 unit tests
pass (arith/parse/dis/walk/obj/save + Six Silver Bullets golden), budget re-blessed.

**Symptom.** `show id` / `show documents` at customs → Scarier **"There's nobody here
to show anything to!"** where FD takes the same `[show] %objects%` task (GiveObject2)
to its *fifth* restriction, **"You will have to take the ID pass out of whatever it is
in first."** The customs official is standing right there.

**Root cause — the `BeInSameLocationAsCharacter` restriction only handled a *specific*
target character (k2).** GiveObject2's first restriction is `Player Must
BeInSameLocationAsCharacter AnyCharacter`; `a5restr.cpp` did `c2 =
a5state_character_index(st, "AnyCharacter")` → −1 → returned false, so R1 failed and
the "nobody here" message fired before the object's container check could be reached.
FD's enum (clsUserSession.vb:4681-4698) has three branches: **k1=ANYCHARACTER** → `Not
htblCharacters(k2).IsAlone` (some other char shares k2's room); **k1 specific,
k2=ANYCHARACTER** → any *other* character co-located with k1; **both specific** →
identical `Location.LocationKey`. Scarier implemented only the third.

**Fixed** in `a5restr.cpp`: a `same_room(a,b)` lambda (resolves either operand through
its carrier via `a5state_character_at_location`, scanning locations for the
on/in-object case — the same seated-NPC resolution as the LostChildren fix), then the
three FD branches dispatched on `ANYCHARACTER` for k1/k2. The both-specific result is
identical to before. Now R1 passes (the official is present), R5 (`ReferencedObjects
MustNot BeInsideObject AnyObject`) becomes the deciding fail → the "take it out first"
message, matching FD.

**Result:** Revenge 3→2 (one `show` hunk fixed; residual 2 = the *other* `show
documents` plural binding "travel documents **and the documents**" — a `%objects%`
dedup issue — and a `bracket`/`crystal` known-word ordering line). No regression in any
MATCH game; all a5 unit tests pass; budget re-blessed.

## ⭐ NotUnderstood default-noun branch must match the entity regex *unanchored* (substring) (broad)  ✅ DONE (2026-06-30)

**MagneticMoon 4→2, RevengeOfTheSpacePirates 4→3, RtC 22→21** — no other corpus
game moved, all a5 unit tests pass (arith/parse/dis/walk/obj/save + Six Silver
Bullets golden), budgets re-blessed.

**Symptom (two inverted hunks, same root cause).** With no task matched, FD's
`NotUnderstood` ladder (clsUserSession.vb:3584-3609) falls back to "I don't
understand what you want to do with %X%." when a *seen+visible* object/character is
named, else `Adventure.NotUnderstood` ("Sorry, I didn't understand that command.").
Scarier got the X-vs-NotUnderstood decision backwards in two opposite ways:
- `say erlin in microphone` → Scarier **"…do with Mike Erlin."**, FD **"…didn't
  understand…"**. Scarier wrongly matched the player on the bare name-word "erlin".
- `put helmet and axe n crate` → Scarier **"…didn't understand…"**, FD **"…do with
  Mike Erlin."**. FD matched the player on descriptor **"me"** found *inside*
  "hel<me>t".

**Root cause — FD matches the entity's `sRegularExpressionString` UNANCHORED**
(`New Regex(ob/ch.sRegularExpressionString)` then `re.IsMatch(sInput)`, **no**
`^...$`, vb:3587/3600), so a noun alternative need only appear as a **substring** of
the whole input. Scarier instead used the looser word-set (`object_words`/
`character_words` + word-equality), which (a) split the proper name "Mike Erlin" into
standalone tokens "mike"/"erlin" (so "erlin" wrongly matched), and (b) required a
*whole-word* hit (so "me" inside "helmet" was missed). Since the regex's article and
prefix groups are all optional (`(art |the )?(prefix )?(noun1|noun2|…)`), `IsMatch`
reduces exactly to: **does any noun alternative occur as a substring of the lowercased
input?** The noun set is the object's `arlNames`, or the character's `arlDescriptors`
plus its **ProperName when usable** (`char_name_usable`) — the proper name kept as a
*whole* string ("mike erlin" must appear contiguously), which is why `say erlin …`
correctly falls through to NotUnderstood.

**Fixed** in `a5run.cpp not_understood`: the seen-noun object and character branches
now build the FD noun set (object names; char descriptors + name-when-usable, **not**
the word-split `*_words` set) and test each noun as a `lin.find(noun)` substring of
the lowercased input (`noun_substr` lambda). Faithful to FD's `If arl.Count = 0 Then
sRE &= "|"` fudge: a nameless seen+visible object matches always. Loop order
(objects before characters) and the seen+visible gating are unchanged.

**Result:** MagneticMoon 4→2 (both Mike-Erlin hunks gone; residual 2 = a `tie X to Y`
task-selection message + the score-0-vs-10 line), Revenge 4→3, RtC 22→21 (one
NotUnderstood line of the same class). No regression in any MATCH game; all a5 unit
tests pass; budgets re-blessed.

## ⭐ The Lost Children → full MATCH: seated-NPC same-location + plural OO `.Sum` (broad)  ✅ DONE (2026-06-30)

**LostChildren 2→0** (full MATCH, golden `test/LostChildren_expected.txt`); no
other corpus game moved, all a5 unit tests pass
(arith/run/save/obj/walk/dis/parse), budgets re-blessed. Two independent root
causes, both broad:

1. **`BeInSameLocationAsCharacter` compared the raw `char_loc`, so a seated/carried
   target was never co-located.** `give shells` at Anne's cottage matched the
   reference-free task `GiveShells3` (`[give/hand/offer] [{sea}shells] {to}
   {anne/lady/woman}`), whose first restriction is `Player Must
   BeInSameLocationAsCharacter Ann`. Anne's `CharacterLocation` is **`On Object`**
   (seated on `Chair1`, which is at `InCottage`), so her `char_loc` is NULL — the
   on-object slot lives in `char_onobj` (per the P7 fix). `pass_character`'s
   `BeInSameLocationAsCharacter` (`a5restr.cpp`) did `streq(cloc,
   st->char_loc[c2])`, i.e. `streq("InCottage", NULL)` → false. The whole task
   short-circuited at R1 (no message), so `GiveShells3` recorded nothing and the
   turn fell through to the general give task's **singular `%object%` cantsee**
   ("You can't see any shells!"). FD compares `ch.Location.LocationKey ==
   other.Location.LocationKey` (clsUserSession.vb:4681), where
   `clsCharacter.Location.LocationKey` resolves a seated character through its
   carrier to the room. **Fixed** by resolving the target via
   `a5state_character_at_location(st, c2, cloc)` (the same carrier-walk the
   renderer and `BeVisibleToCharacter` already use) instead of the raw `char_loc`
   read. Now R1 passes, the OR-group's `Seashells2 Must BeInsideObject Bag4`
   becomes the deciding fail → "You have no seashells to give to Anne!".

2. **A plural `%objects%` OO chain bound to a *single* object lost its
   collection semantics, so `.Sum` rendered "" not "0".** `take all` (rations on
   the ground) failed the stock `TakeObjects` restriction `%objects%.Parent.
   Takefix.Sum=0` (the "already-carrying" guard: a held object's parent is the
   Player, Takefix=1). `%objects%` resolved to the single key `Rations`, which
   `resolve_first` (`a5expr.cpp`) made a **single item** (`is_list=0`); the
   single-item `.Parent` produced a single-item `InCottage`, and `InCottage.
   Takefix.Sum` walked the single-item/location path where the absent `Takefix`
   rendered **empty** instead of being summed to **0** — so the expression became
   `""=0` (string-compare → false) and **every** ground object failed `take all`
   → "There is nothing worth taking here." In FD a plural reference is always a
   collection (clsObjectList), so `.Sum` aggregates with a missing per-item
   property contributing 0. **Fixed** with a `force_list` flag on `a5expr_eval`
   (`a5expr.cpp`/`.h`): when the OO head is the plural `%objects%`/`%characters%`
   (detected in `a5text.cpp`'s `expr_substitute`), the head is a list even with one
   key, so `.Sum`/`.Count`/list-`.Parent` apply. Now `Rations.Parent` → list
   `{InCottage}`, `.Takefix.Sum` → "0", `0=0` → true → the rations are taken.

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

**~~Still OPEN~~ → since DONE (the other half — the dedup-of-identical-messages
mechanism #2): closed 2026-06-30 by the two entries above, "Per-turn response
aggregation on the movement path" and "Event-fired task iterates the leftover
plural reference"; Amazon is a 0|0 MATCH.** Original note as written:
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

2. **Message merge + dedup** (`AddResponse`, vb:1295-1320). ✅ **plural path done;
   movement path DONE (2026-06-30) — see the top entry "Per-turn response
   aggregation on the movement path".** When a message text is
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

## ⏸️ PARKED — Pathway to Destruction: `<cls>` is a whole-turn wipe, not per-fragment (root-caused, not yet fixed)

> ✅ **DONE (2026-07-02) — PathwayToDestruction → full MATCH (both modes), now
> wired into the MAP (`0|0`) with golden `test/PathwayToDestruction_expected.txt`.**
> No corpus game moved (all 13 MATCH goldens hold 0/0; StoneOfWisdom 2/0,
> JacarandaJim 99/0, SixSilverBullets 18/0 unchanged), all a5 unit tests pass
> (`make -f Makefile.headless test`), and the Pathway + AchtungPanzer +
> Amazon + SixSilverBullets pipelines are ASan/UBSan-clean.
>
> The parked plan below was **refined during implementation** after reading FD's
> `FrankenDrift.Headless/Program.cs` `EmitHtml` (the real ground-truth renderer):
> `<cls>` clears only the **current commit's** buffer (one `OutputHTML`/`EmitHtml`
> call), NOT the whole turn.  A commit is one `Display(..., bCommit:=True)`
> (`clsUserSession.vb:298`, `sOutputText` reset on commit).  A **command turn is a
> single commit** (its output accumulates and flushes once, vb:3766), so a `<cls>`
> there DOES wipe the whole turn's prior output — Pathway.  But the **intro is
> several commits** (vb:226 RunImmediately+title, vb:227 Introduction, vb:229 room
> view), so an Introduction that opens with `<cls>` must NOT wipe the already-
> committed title — Achtung Panzer.  A naïve whole-turn truncation regressed every
> intro; the fix is **commit-boundary-aware**.
>
> **Implemented (three pieces):**
> 1. **`<cls>` relays a marker** (`A5_CLS_MARK`, `a5text.h`): `a5text_render_plain`
>    still clears its own fragment on `<cls>` but also leaves an `\x01` sentinel so
>    the flush can wipe earlier fragments.  `sb_pspace` treats a trailing marker
>    like a trailing newline (no stranded join-spaces).
> 2. **`sb_resolve_cls(out, floor)`** (`a5run.cpp`) drops everything from `floor`
>    to the last marker in the range.  `finish_turn` calls it with `floor=0` (a
>    command turn = one commit); **`a5run_intro` calls it at each commit boundary**
>    (Adventure-Upgrade prompt, RunImmediately+title, Introduction, room view,
>    start-of-game events), so each unit's `<cls>` clears only that unit.
> 3. **Event-chain completion dedup** (`run->ev_seen`, installed per
>    `attempt_event_task` = FD's per-`AttemptToExecuteTask` `htblResponsesPass`):
>    a completion message emitted twice within one event via `SetTasks Execute`
>    shows once, keeping the first position.  Task5's "The End" banner is executed
>    by **both** Task33 (its own `Execute Task5`) and its parent Task30 (a 4th
>    `Execute Task5` action); the dedup collapses them to one banner at the FIRST
>    position — which is **before** Task33's `<cls>`, so the whole-turn clear wipes
>    it.  Without the dedup the second banner (after the `<cls>`) would survive.
>
> Result on the final `north`: FD and Scarier both emit only the post-`<cls>` win
> narrative ("You open your eyes … you blink out of existence.") then
> `*** You have won ***` — the move message, "Testing Chamber" room view and both
> "The End" banners are wiped.  Original parked analysis retained below.

---

**Symptom**: on the walkthrough's final `north` command, Scarier emits
`[move msg][room desc][Task5 banner #1][Task33 win narrative][Task5 banner #2]`;
FD emits **only** the tail of the win narrative that comes after an embedded
`<waitkey><cls>` inside Task33's `CompletionMessage` — zero move message, zero
room description, zero banners.

**Root cause, confirmed by reading FD source (not guessed):**

1. FD's `Display(sText, bCommit, ...)` (`clsUserSession.vb:298`) does **not**
   render per call. It appends to a member field: `sOutputText =
   pSpace(sOutputText) & sText`. Rendering (`Source2HTML` → the HTML tag pass
   that actually clears on `<cls>`, mirrored by our headless `Program.cs`'s
   `EmitHtml`) only happens when a caller passes `bCommit:=True` — which in
   normal play is **once per whole turn** (see the `bCommit:=True` call sites,
   `clsUserSession.vb:3766` etc., not one per message/task).
2. So every `Display()` call for an entire turn — the movement command's own
   "You move north."/room description, Task30→31/32/33/Task5's messages, all
   of it — lands in the *same* `sOutputText` string, concatenated with
   `pSpace`, and is rendered **as one HTML blob** at end of turn.
3. `<cls>` embedded anywhere in that blob (here, deep inside Task33's
   CompletionMessage) therefore wipes **everything accumulated so far in the
   entire turn**, not just its own task's output — exactly matching the
   observed FD transcript (only the post-`<cls>` tail survives).
4. Separately, `AttemptToExecuteTask`'s `htblResponsesPass/Fail`
   (`bChildTask=False` stash/clear/accumulate/`Display()`-loop/restore, see
   `clsUserSession.vb:719-952`) dedups messages **by rendered text** within a
   flush scope. Event5's `SubEvent What=ExecuteTask` dispatch
   (`clsEvent.vb:384-391`) calls `AttemptToExecuteTask(se.sKey, True, , , , ,
   , , True)` — 4th positional arg (`bChildTask`) is omitted, i.e. **False**
   — so Task30 gets its own flush scope; Task33's nested `SetTasks Execute
   Task5` and Task30's own direct `Execute Task5` both add the identical
   banner text to the *same* `htblResponsesPass`, so it's stored once and
   displayed once (not twice, as Scarier currently does).

**What Scarier needs (two coordinated fixes, not yet implemented):**
(a) `<cls>` must wipe the master per-turn `sb_t out` buffer (`a5run.cpp`),
not just the local `sb` inside the single `a5text_render_plain` call
currently rendering one fragment — e.g. relay a sentinel marker byte out of
`a5text_render_plain` and have the ~14 `sb_puts(out, ...)` call sites that
carry rendered game text (a5run.cpp: 2315, 2409, 2518-2519, 2539, 2701, 3595,
3606, 3688, 3834, 4358, 4374, 4509, 4921, 5497) detect it and truncate `out`
before appending the remainder. Must keep `msg_has_output`/`fd_has_output`
treating a marker-only string as "no output".
(b) Widen the existing `resp_map` dedup (currently only active for plural
`%objects%` and movement commands, a5run.cpp:~2560-2615) to also cover
event-triggered task chains (`ev_tick_all`'s TurnBased-event dispatch), so a
task reached twice in one turn (once nested, once direct, as Task5 is here)
only contributes its message once.

Both are needed together — (a) alone still leaves a duplicate second banner
(Task30's own direct `Execute Task5` produces a fragment with no `<cls>` of
its own); (b) alone still leaves the earlier move-message/room-description/
first-banner text un-wiped.

**Status: parked at the user's request** before implementing — this is
purely a documentation checkpoint so the RE work (which required reading
`clsUserSession.vb`'s `Display`/`AttemptToExecuteTask` and `clsEvent.vb`'s
`SubEvent` dispatch in FrankenDrift) isn't lost. Pathway to Destruction is
**not yet wired into `run_a5_walkthroughs.sh`'s `MAP`** — do that once this
fix lands (or with an honest non-zero budget documenting this exact gap if
parked further).
