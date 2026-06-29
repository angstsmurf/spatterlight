# TODO: ADRIFT 5 conformance bugs surfaced by the walkthrough corpus

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
if it reaches 0, capture a golden `test/<Game>_expected.txt`). Only Achtung
Panzer is fully conformant (MATCH) today.

Ordered by impact. Each item: symptom → evidence → suspected locus → fix → verify.

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

**Still open (1b):** **Lost Children** *misses* a legitimate first-turn event
(lost broadsword/money) and emits a stray `Follow Cancelled.` — the inverse
ordering problem; cross-check walk-tick vs event-tick vs follow-resolution order
in `ev_tick_all`. Not addressed by the above; track as its own item.

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

### 2b — movement confirmations + status line  (still open; re-diagnosed 2026-06-29)
- **The "standard-library merge" hypothesis is wrong.** FD loads **no** library:
  `Global.GetLibraries` (which would look for `StandardLibrary.amf`) has **zero
  callers** and the file is absent from the FD tree. So `You move <dir>.` and the
  `Date: … Time: …` status line both come from the **game's own data**, not a
  merged library — Scarier must already have the data and is mis-running /
  mis-ordering it. (Note: `You move <dir>.` also appears on Scarier's side in
  anno1700, positionally shifted — i.e. it *is* emitted, just desynced by an
  upstream cascade, not absent.)
- status line `Date: … Time: …` (Amazon) — a **game turn-event** (Amazon.xml
  ~line 11824) that prints the date/time each turn; Scarier isn't firing it.
  Entangled with the Amazon P4 buy cascade; diagnose together.

**Locus:** the v5 location/look renderer is `a5text_view_location` (`a5text.cpp`,
not `a5run.cpp`); turn-event firing in the turn driver (`ev_tick_all`).

**Verify:** Amazon simple-movement stretch matches line-for-line once the
standard-library move task + date/time event fire.

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

### 3b — object aliases not matched  (still open)
`examine id` / `show id` → "You see no such thing." while FD resolves `id` → "ID
pass". **Revenge of the Space Pirates** (sibling nouns `lighter`/`documents`/
`card` do resolve, so it's alias-specific).  `name_match` (`a5run.cpp`) already
iterates all `names[]` entries; investigate whether the "ID pass" alias is being
dropped at model-parse or whether it is a multi-word alias the single-name
`name_match` path mishandles.

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

## P6 — Object pickup via examine not registered

**Symptom:** the shovel revealed by `x shelves` is never taken, so the final
`dig` → "You have nothing to dig with"; FD wins. **Grandpa's Ranch.**

**Locus:** task side-effects of examine (move-object-to-player on look), object
state after `x <container/surface>` in `a5run.cpp` task execution.

**Fix:** ensure the examine task's object-movement/grant action executes (the
shovel becomes held/available) as FD does.

**Verify:** Grandpa's Ranch reaches "YOU HAVE WON!!! 15/15".

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

## P10 — Template / function expansion gaps

**Symptom:** Jacaranda Jim leaks a literal `%AlanRemarks[%AlanRemarkIndex%]%` —
a nested array-index function/variable reference not evaluated. **Jacaranda Jim.**

**Locus:** text/function expander `a5text.cpp` (`Global.ReplaceFunctions` analog,
`a5text.cpp:1235`); nested `%var[%index%]%` array-subscript handling.

**Fix:** evaluate inner `%...%` first, then the array subscript on the outer
function/variable.

---

## Harness follow-ups

- After each fix, re-run `test/run_a5_walkthroughs.sh`, lower the game's baseline
  in the runner MAP, and capture a golden `_expected.txt` when it hits 0.
- **P1 is now fixed**, so revisit `FD_RNG=xoshiro` for the combat/RAND-heavy
  Horsfield games (`FD_RNG=xoshiro test/run_a5_walkthroughs.sh`): FrankenDrift
  then draws the same RNG stream as Scarier (`FrankenDrift.Headless/Program.cs`
  `XoshiroRandom`), so in-sync transcripts converge to exact every-line matches
  instead of stopping at the RAND-divergence wall. The games now play their whole
  scripts, so the aligned stream should finally pay off on the in-sync stretches —
  measure per game and, where it helps, re-bless that game's baseline under
  xoshiro (note it in the MAP). The vanilla baselines above are the
  `FD_RNG`-unset numbers.
- **Stone of Wisdom** is a weak test (reconstructed from a ROT13 hint sheet with
  no navigation; neither engine progresses) — fix the script or drop it.
