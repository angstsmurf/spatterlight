# ADRIFT 5 walkthrough regression corpus — findings

Differential regression tests built from published walkthroughs for the
real-world ADRIFT 5 games in `test/adrift5-games/`. Each game's walkthrough was
cleaned into a one-command-per-line script (`test/<Game>_walkthrough.txt`,
anno1700 style: `#` headers + bare commands) and replayed through **both** the
Scarier harness (`test/a5run_dump`) and the **FrankenDrift** reference engine
via `test/a5_groundtruth.sh`. FrankenDrift is the ground truth.

## Running

```sh
make -f Makefile.headless a5run          # build test/a5run_dump
# build FrankenDrift.Headless once (see a5_groundtruth.sh header)
test/run_a5_walkthroughs.sh              # whole corpus, summary table
test/run_a5_walkthroughs.sh -v Spectre   # one game, dump diff to /tmp/a5wt/
test/a5_groundtruth.sh test/adrift5-games/<Game>.blorb test/<Game>_walkthrough.txt
```

`run_a5_walkthroughs.sh` reports **MATCH** (Scarier == FrankenDrift),
**DIVERGE n** (n diff hunks; compared to the per-game baseline budget recorded
in the runner's MAP), or **FAIL** (hunks exceeded budget = a regression to
investigate). Games with a committed golden (`<Game>_expected.txt`) are
strict-diffed against Scarier with no dotnet dependency.

## Corpus status (baseline = current known divergence)

| Game | cmds | status | hunks | diagnosis |
|---|---:|---|---:|---|
| Achtung Panzer! | 6 | **MATCH** ✅ | 0 | golden committed; conformant |
| Anno 1700 | 246 | diverge | 335 | pre-existing script; timed storage-room event is RNG-gated (335 after P1: its 1 LocationTrigger task now fires, shifting RNG alignment) |
| The Axe of Kolt | 1010 | diverge | **4** | ~~72~~ → 42 (doubled-quote numeric vars) → 12 (event/Score chain) → **4**: plural `%objects%` reference model (singular-`%object%` token leak + spurious `resolve_plural` cantsee, see TODO top). Residual 4 = `say to`/`tell` HighestPriorityPassingTask message selection (3) + `throw seed` second-chance noref (1) |
| The Spectre of Castle Coris | 400 | diverge | 96 | ~~BUG 1~~ **FIXED** (no more teleport/flood); remaining 96 are **BUG 9** parser wording ("…not sure which object you are trying to examine." vs "You can't see any X!") |
| Run, Bronwynn, Run! | 299 | diverge | 49 | ~~BUG 1~~ **FIXED** (no more turn-4 `[GAME OVER]`); plays whole script; 47→49 after end-game text added (Scarier ends prematurely, FD plays on ⇒ win/lose block diverges) |
| Die Feuerfaust | 528 | diverge | 40 | ~~BUG 1~~ **FIXED** (no more Part-3 death); plays whole script, downstream P2/P9 visible |
| Starship Quest | 328 | diverge | 16 | ~~BUG 1~~ **FIXED** (no premature death); also **BUG 8** missing Y/N prompt text; 15→16 after end-game text (Scarier plays 1 extra turn ⇒ "in 53 turns" vs FD "52") |
| The Lost Children | 417 | diverge | 383 | **BUG 1b** *misses* a first-turn event (lost broadsword/money); stray "Follow Cancelled."; **BUG 9** parser wording; "SouthEast" vs "Southeast" |
| Magnetic Moon | 472 | diverge | 32 | **BUG 5** NPC "Captain Rosenloev" omitted from room characters; **BUG 8** missing Y/N prompt; 34→32 after end-game text (win/lose banner now matches) |
| Revenge of the Space Pirates | 414 | diverge | 53 | **BUG 4** noun alias `id`→"ID pass" not resolved (`examine id` → "You see no such thing.") → customs puzzle desyncs (139→53 after P1 LocationTrigger fix) |
| Return to Camelot (RtC) | 118 | diverge | 141 | ~~**BUG 6** 5.0.26 OR-after-AND~~ **DISPROVEN** — FD headless answers the auto-correct prompt *no* (default), so it does *not* apply CorrectBracketSequence; `x desk` works in Scarier in isolation. Real gaps: the auto-correct question *prose* + a downstream `x desk` resolution cascade (after `open/get folders/get gun/x gun/close drawer`). See TODO P5. |
| Treasure Hunt in the Amazon | 133 | **MATCH** ✅ | 0 | golden committed; conformant — plays through to `*** You have won ***`. Long road: BUG 3 group-property `buy` (44→41), the `ts_*` time subsystem (`%PropertyValue%` runtime override, `Execute Look`→`Beforeplay1` Date lines, per-item event-tick clock drift) 41→1, and finally the title's centring space (System `<RunImmediately>` PlayTune + markup-aware `bHasOutput`) 1→0. See TODO top entries. |
| Grandpa's Ranch | 93 | diverge | 125 | **BUG 2** missing room desc/exit+object listings; **BUG 7** `dig` → "nothing to dig with" (shovel not taken from `x shelves`) |
| Jacaranda Jim | 440 | diverge | 439 | **BUG 4** pronoun `get it`/`get them` unresolved (35×); **BUG 2** missing "Exits are …"; **BUG 10** leaks template `%AlanRemarks[%AlanRemarkIndex%]%`; darkness handling |
| Six Silver Bullets | 33 | MATCH ✅ (xoshiro) | 18 (xo 0) | real winning walkthrough (*** You have won ***). **BUG 14 RESOLVED**: the time-trap subsystem desynced the `Roller` RNG stream (the `'RAND(1,16)'` restriction RHS was never drawn), mistiming the sniper/bomb events and killing the player. Fixed (RAND-on-RHS draw + runtime location-group membership + `LocationOf %Player%` resolution + room-view `pSpace`); xoshiro now 0 (full FD-logic MATCH), vanilla 18 is irreducible System.Random RNG noise. See TODO top entry. |
| Stone of Wisdom | 137 | diverge | 6 | **REWRITTEN** as a real winning walkthrough (50/50, *** You have won *** in FrankenDrift, 137 turns). Surfaces **BUG 13** troll-knockout death — after `hit troll with ring` knocks the troll unconscious, Scarier still fires the troll's "you-didn't-act → die" event on the same turn, killing the player at turn 10 (FD's knockout cancels it). 3 of the 6 hunks are inherent RAND troll-taunt lines (default-RNG); the rest are that death + the loss-vs-win cascade. See TODO_a5_walkthrough_bugs.md. |
| Things That Go Bump in the Night | 310 | diverge | 8 (xo 8) | **WIRED 2026-07-02** — full max-score win (`*** CONGRATULATIONS! ***`, 250/250) under FD, script derived from the game's **own built-in `WALKTHROUGH` command** (three moves corrected for this build's cut-scenes: dropped a spurious intro `n`; added `snuffio`+`s` so the guard patrol carries you to the Treasury door in the dark). Identical 8 hunks in both RNG modes ⇒ two RNG-independent bugs: (1) **`drop all` over-expands** the bare `all` to every seen object (worn clothing, scenery "the face"/"the cave", the location object `cl_Ravin`) → "You are not holding …" instead of dropping the 6 loose held items → hands full → `climb rope` fails → the ravine beast kills the player (`*** You have lost ***`, turn 184) = 7 hunks; (2) dark `get dirt` is a silent no-op where FD says "It is too dark to find the dirt." = 1 hunk. See TODO_a5_walkthrough_bugs.md (OPEN). |
| The Lost Labyrinth of Lazaitch | 452 | diverge | 403 (xo 403) | **WIRED 2026-07-02** — full 520-point win (`*** CONGRATULATIONS! ***`, 451 turns) under FD from the game's **own built-in walkthrough** (`WLKTHRGH`), extracted verbatim from the `cl_Walkthroug` model task; **not one native command needed correcting** (FD replays the raw author commands straight to the win, zero press-O/not-understood/can't-see lines), so all 403 hunks are Scarier bugs. Dominant: the **"Fahren Layburn" teleport spell** doesn't fire in Scarier ("you say the words … but nothing happens" vs FD's transport to Layburn church), desyncing the whole village endgame. Also `sheath sword` wording + two un-gated room-desc segments. See TODO. |
| The Call of the Shaman | 151 | **MATCH** | 0 (xo 0) | **MATCH 2026-07-02** (golden) — full 265-point win (*** CONGRATULATIONS! ***) derived by play from the bundled ROT1 hint sheet. The 3 endgame-banner hunks (BUG 15, all RNG-independent) are fixed: (1) added the `%turns%`/`%Turns%` built-in to `eval_function` (ci-insensitive, value `st->turns-1` matching FD's post-`Process()` bump) → "151"; (2)+(3) the credits URLs `Https://`/`Http://` were leading-capped by the plain-text Display-boundary re-cap after the `<del>` tag was stripped — fixed by capping the room view on its marked-up buffer in `view_location_impl` and suppressing the boundary re-cap's `^`/`\n` line-start rules (`auto_capitalise_ex allow_line_start=0`). |

Anno 1700 predates this batch; its FrankenDrift golden was never empty
because of an RNG-gated storage-room event (documented in its script header).

FrankenDrift reaches `*** You have won ***` on the **Amazon**, **Grandpa's
Ranch**, **Stone of Wisdom** and **Six Silver Bullets** scripts, so those
walkthroughs are *correct* — the divergence is all Scarier. The CASA Horsfield scripts are for the original releases; a handful of
verbs differ in the remakes (both engines reject those identically, so they are
not counted as Scarier bugs).

## Prioritized Scarier conformance bugs (surfaced by this corpus)

1. **Event/timer scheduler fires too early.** ✅ **FIXED** (2026-06-29). Two
   root causes, both in event/task arming at game start:
   - **`<WhenStart>0</WhenStart>`** (a numeric serialisation FrankenDrift loads
     via `[Enum].Parse`, e.g. Axe of Kolt's "Xixon On Path") was collapsed to
     the `Immediately` default by the Scarier model parser, so the event started
     at load and fired turn ~1. FD's game-start `Select Case` matches no case for
     0, leaving the event `NotYetStarted` (armed only by its Start control). Fix:
     `a5model.cpp` now parses the numeric form (0/non-1-2-3 ⇒ that int), and
     `ev_init`'s switch already no-ops for it.
   - **`<LocationTrigger>` System tasks** (chapter/scene transitions, e.g. Axe's
     Task3 "Wagon at Start" → move to Crossroads, then Task6 "Haywain Goes")
     were never run: Scarier had no System-task auto-execution. Ported
     `clsCharacter.Move`'s queue — a Player move into a location arms every
     System task whose `LocationTrigger` is that location onto `qTasksToRun`,
     drained after the turn's task and before `TurnBasedStuff` (`ev_tick_all`).
   Result: **Axe** survives past `b` to the Crossroads; **Starship / Die
   Feuerfaust / Run Bronwynn** no longer hit a premature `[GAME OVER]`;
   **Spectre** no longer teleports/floods. Revenge 139→53 and Amazon 230→45 also
   dropped (their LocationTrigger scene tasks now fire). The premature-death
   games' budgets *rose* because they now play their whole scripts and expose
   downstream P2/P9 divergences — those are the remaining work, not a regression.
   - 1b. **Still open** — inverse in **The Lost Children**: a legitimate
     first-turn event is *missed*, and a stray "Follow Cancelled." is emitted.
2. **Room-rendering omissions.** Scarier omits the `Exits are …` / `An exit
   leads …` exit lists appended to room descriptions, and per-turn movement
   confirmations (`You move <dir>.`) / status lines (`Date: … Time: …`).
   Seen in Amazon, Grandpa's Ranch, Jacaranda Jim. High hunk-count inflator.
3. **Shop / "objects must be seen".** ✅ **FIXED** (2026-06-29). Was *not* a
   visibility gate — it was missing **object-group property inheritance**. The
   Amazon for-sale items get `StaticOrDynamic=Dynamic` + `BuyableItem` from the
   `BuyableItems` Objects-group, which FD inherits onto members but Scarier
   ignored, so the items stayed static/Hidden (never seen) and lacked
   `BuyableItem`. `a5_apply_group_properties` (a5model.cpp) now folds each
   Objects-group's `<Property>` list into its member objects' props at load.
   `buy ammo/provisions/matches/torch` purchase; Amazon 44→41, no regressions.
4. **Noun & pronoun resolution.** (a) Pronouns `it`/`them` after an examine are
   not resolved (`get it` → "not sure which object", Jacaranda Jim 35×).
   (b) Object aliases not matched: `id` → "ID pass" (Revenge of the Space
   Pirates).
5. **NPC presence list.** Magnetic Moon's control room omits the present NPC
   "Captain Rosenloev" that FrankenDrift lists.
6. **Missing 5.0.26 restriction correction.** FrankenDrift applies the ADRIFT
   5.0.26 "OR restrictions after AND restrictions were not evaluated"
   auto-correction; Scarier does not, so OR-after-AND task restrictions
   evaluate wrong (Return to Camelot: many `x <thing>` tasks fail).
7. **Object pickup via examine.** Grandpa's Ranch: the shovel revealed by
   `x shelves` is never taken, so `dig` fails "nothing to dig with".
8. **Y/N prompt text.** Scarier enters the yes/no state but doesn't print the
   question ("Do You Wish To Explore The Ship First? (Y/N)") — Starship Quest,
   Magnetic Moon.
9. **Parser error wording.** Scarier "Sorry, I'm not sure which object you are
   trying to examine." vs FrankenDrift "You see no such thing." / "You can't
   see any <noun>!". Cosmetic but inflates diffs.
10. **Template/function expansion.** Jacaranda Jim leaks a literal
    `%AlanRemarks[%AlanRemarkIndex%]%` — nested array-index function not
    evaluated. Direction capitalization "SouthEast" vs "Southeast"
    (Lost Children).
14. **Time-passing model (Six Silver Bullets).** ✅ RESOLVED (xoshiro 10→0,
    full MATCH; see TODO top entry). The root cause was an RNG-stream desync, not
    a `Time`-decrement-on-movement difference: `TimeTrapsT`'s gate
    `Roller Must BeEqualTo 'RAND(1,16)'` draws a random each turn in FD, but
    Scarier evaluated the quoted `'RAND(1,16)'` as `strtol`=0 and drew nothing, so
    its `Roller` stream ran ~31 draws behind FD and every `Roller`-gated event
    (chime/bomb/`SniperHits`) mistimed — sniping the player. Fixed by (a) drawing
    `RAND(min,max)` on a restriction RHS (`num_value`, `a5restr.cpp`), (b) routing
    `BeWithinLocationGroup` through the runtime group-membership override so the
    bell task's `RemoveLocationFromGroup` sticks, (c) resolving `LocationOf
    %Player%` via `act_key`, and (d) using `pSpace` (not the sentence-aware
    `add_space`) for the room-view look/NPC/exit joins. xoshiro now 0; vanilla 18
    is irreducible System.Random-vs-xoshiro flavour-text noise (like JacarandaJim).

## Caveats

- **RNG divergence** (default mode): Scarier (xoshiro128**) and FrankenDrift
  (.NET System.Random) produce different random text even seeded — combat /
  epigraph / dream / "random catch" lines differ. **`FD_RNG=xoshiro`** patches
  FrankenDrift to draw from a byte-identical xoshiro128** stream
  (`FrankenDrift.Headless/Program.cs` `XoshiroRandom`); RAND choices then match
  Scarier exactly (verified: the Six Silver Bullets intro epigraph/dream/voice
  variants line up across consecutive draws).
  - *Measured on this corpus, FD_RNG=xoshiro does NOT lower the hunk counts*
    (14/15 unchanged; Jacaranda Jim +15). These games desync on the structural
    bugs above (esp. BUG 1) before RNG matters, so once the transcripts diverge
    the aligned stream is washed out / adds churn. The payoff is unlocked per
    game as the structural bugs are fixed — a transcript that stays in sync then
    rides the aligned RNG to an exact every-line match. Keep it opt-in until
    BUG 1 is fixed; do not re-bless the (vanilla) baselines to it yet.
- These scripts are **best-effort**: CASA walkthroughs target the original
  releases, and some commands differ in the ADRIFT 5 remakes.
- Source walkthroughs and provenance: `test/adrift5-games/walkthroughs/`.
