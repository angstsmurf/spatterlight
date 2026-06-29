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

**Still open (Grandpa residual 45):** (a) the **dial numeric-input prompt** —
`turn dial` should enter a "Which number do you choose?" input state, `853` then
opens the box (same family as **P8** Y/N prompts; Scarier emits "I did not
understand the word 853"). (b) the **bull-capture event chain** (`open gate` /
`wave cloth` / `pull rope` move Buster between enclosures via events) — Scarier
doesn't track Buster's position, so the eastern enclosure is never reachable and
`dig` → "You see no reason to dig here." Both are distinct from the examine/
darkness mechanism and block the final `*** YOU HAVE WON 15/15 ***`.

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
