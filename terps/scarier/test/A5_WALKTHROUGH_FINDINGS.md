# ADRIFT 5 walkthrough regression corpus — findings

Differential regression tests built from published walkthroughs for the
real-world ADRIFT 5 games in `test/adrift5-games/`. Each game's walkthrough was
cleaned into a one-command-per-line script (`test/<Game>_walkthrough.txt`,
anno1700 style: `#` headers + bare commands) and replayed through **both** the
Scarier harness (`test/a5run_dump`) and the **FrankenDrift** reference engine
via `test/a5_groundtruth.sh`. FrankenDrift is the ground truth.

> References here and in code comments to `TODO_a5_walkthrough_bugs.md` point
> to the conformance-bug ledger that lived next to the engine sources until
> every entry was closed; it was pruned 2026-07-14 and its full write-ups
> remain in git history (`git log --all -- terps/scarier/TODO_a5_walkthrough_bugs.md`).

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
| Achtung Panzer! | 16 | **FULL MAX-SCORE WIN, MATCH** ✅ | 0 (xo 0) | **REWIRED 2026-07-05 as a MAX-SCORE WIN** (`*** CONGRATULATIONS! ***`, "scoring the maximum 100 points!", 14 turns) with **MATCH 0\|0 in both RNG modes**, zero engine changes. The old 6-command script (Denk's review sequence) never answered the intro's Y/NO prompt, so every command bounced off "Please type Y or NO…" — the previous MATCH was vacuous (the game never started). Route derived from an A5_DUMP_XML score-task audit: 15 `IncVariable Score` tasks total 105, but the two "Tank Blows Up Safe" variants are mutually exclusive on `cl_Choice` (timed game `no` → +10 `cl_TankBlowsU4` slit-trench LocationTrigger; untimed `y` → +5 `cl_TankBlowsU3`), so exactly 100 is reachable — only in the TIMED game. Full ledger in the walkthrough header; the 6 in-tank/escape commands fit inside the event's 10-turn fuse (deaths at turn 10 in tank/on ground/behind rise). Golden committed at the win. |
| Anno 1700 | 250 | **FULL WIN, MATCH** ✅ | 0 (xo 0) | **WIRED 2026-07-05 as a FULL WIN** (`*** You have won ***`) with **MATCH 0\|0 in both RNG modes** — the founding ground-truth game finally plays to the end. The old "RNG-gated storage-room event" diagnosis was wrong: event `EffectOfTh` ("Moving player to storage room", TurnBased, Length 15) is fully deterministic — visions at turns 3/8/11, then at turn 15 `ExecuteTask MovingPlay` teleports the player to the past. Denk's wait loop stopped at the turn-11 vision, **4 turns short**, so the player was never moved and the remaining ~80 commands ran against the wrong map (the entire 335-hunk tail was this one derailment — both engines failed identically). Fix: 4 extra `z` in the wait block; **zero engine changes needed**. Golden committed at the win. |
| The Axe of Kolt | 1178 | **FULL WIN, MATCH** ✅ | 0 (xo 0) | **REWIRED 2026-07-05 from the game's own built-in WLKTHRGH to a FULL WIN** (axe lowered to Kelson), then driven to **MATCH 0\|0 (2026-07-05)**. History: ~~72~~ → 42 (doubled-quote numeric vars) → 12 (event/Score chain) → 4 (plural `%objects%` reference model) → win rebuild (HighestPriorityPassingTask pass-with-no-output must not claim the turn; MoveCharacter Inside/OntoObject char_loc sync; multi-room location-group furniture sync) → magpie RNG (FD's render1→actions→render2 Before-message draw order on a FROZEN template, `a5text_process_frozen`) → **five conformance fixes to 0** (see the run_a5_walkthroughs.sh MAP comment, 2026-07-05): (1) `SetVariable Variable330[Hidden]` — split the LHS at `[` like FD's FileIO, the silent no-op left the tomb stone-slab room descs un-gated; (2) FD's NotUnderstood-on-empty-turn (vb:3421-31) with the qTasksToRun drain BEFORE the check and the raw MARKED-UP `sOutputText` emptiness test (`st->turn_out_nonempty`); (3) the turn-global `sReferencedText` capture-leak (vb:2567/3400/4474) — `say hello` passes s_SayHelloTo's `ReferencedText Must BeContain "hello"` via earlier candidates' %text% captures; (4) direct-path override fails buffer in the exec scope with FD's POSITIONAL pass-cancels-fail (talisman/block-pile/wave-axe show only the passing override's text; AoS's 2-ref `get ashes` fail still survives its 1-ref pass); (5) `view_location_impl` runs ALR round 1 on the UNCAPPED room view (FD Display order), so Override12 blanks the lowercase "the dwark is here." listing. |
| The Axe of Kolt — V5 Intro Comp Entry | 57 | **FULL MAX-SCORE run, MATCH** ✅ | 0 (xo 0) | **WIRED 2026-07-21** — the last unwired game file in `test/adrift5-games` (`AoK Comp.blorb`). Larry Horsfield's 2012 four-location competition taster: 15 locations (4 of them menu/prologue pages), 4 characters, no built-in WLKTHRGH, and — per its own Playing Instructions — never playtested. It compresses the full game's Chapter 1, so the route's spine is lifted **verbatim** from `AxeOfKolt_walkthrough.txt` (read signpost / help innkeeper / w / x tapestry / read writing / e, then the outlaws→pass→dwark→trapper→tapestry→magor→forest→ferry→kolt→xixon→kelson→father topic run); only the barmaid's name (Bella → Lizzie Roffey) and the phrasing (`ask about X` → `ask lizzie about X`, the comp has no bare-`ask about` task) change. The other 18 ask topics and `kiss lizzie` are comp-only and were derived from the module XML. **MAXIMUM score: "you scored 72 out of a possible 1000"** — the 1000 is the full game's leftover MaxScore (Task1/Task98 set 2000, Task3 resets to 1000); 72 is the sum of every `Score` action in the module (23×2 + 7×3 + one ×5). Two timing traps: exactly **one** turn in the Inn Yard before Event1's Task75 "No Offer of Help" drags you inside without the offer, and a hard **50-move** Event2 window from `help innkeeper` to the traveller's arrival (Task198 → Task65, `EndGame Neutral`, gated on being within Group2 = the two inn rooms). **One engine fix fell out of it:** the game-start "does the Introduction end in a `<cls>`?" test compared the rendered intro's *last byte* against `A5_CLS_MARK`, but this intro ends `<waitkey><cls></b>` and the stripped `</b>` leaves an `A5_ALR_MARK` sentinel **after** the wipe marker — read as "text after the wipe", so a paragraph break was appended that survived `sb_resolve_cls` as a stray blank line above the first location (the single hunk in both RNG modes). `a5run.cpp`'s new `msg_ends_with_cls` scans back over the non-output presentation sentinels, the same set `msg_has_output` already ignores. Inert elsewhere; whole corpus re-run 107 MATCH / 8 DIVERGE-at-baseline, zero regressions. |
| The Spectre of Castle Coris | 924 | **FULL MAX-SCORE WIN** ✅ | 0 (xo 1) | **REWIRED 2026-07-04 to the MAXIMUM 700/700** (`*** CONGRATULATIONS! ***`, "…in 923 turns, scoring the maximum of 700 points!"). Old script was the CASA original-release solution (desynced in the town, never reached the castle; its 96 hunks were **BUG 9** parser wording on a derailed transcript). New base = the game's own built-in `WLKTHRGH` solution repaired against this build's map (castle-grounds `"w, ,sw"` garble; fishpond re-ordered — rod in hand BEFORE baiting, the catch needs `Held.Count <= 1` and the crumbs only hold the shoal ~2 turns, plus `z` padding so the Spectre timer fires before the bait window; `dig manure with fork` — the bare verb stalls on "DIG … WITH what?" and the stone ring is lost; `ask about talisman` at the pedlar = the last +2 of 700). The **Spectre-banish cadence was machine-converged** by `test/spectre_prayer_solver.py`: the Spectre roots your feet (movement is *silently eaten*, cascading nav desyncs) and must be banished by `read prayer` with the book **held in hand**; the solver inserts a banish at each materialisation and restores the book's prior held/bagged/dropped state (two-handed actions force-drop it). Key entry gate: the Top-of-Steps **bellpull only admits you with the full checklist carried** (peg, dead goldfish, flour sack, lamp, dead rat, chain+hook). Golden committed (vanilla 0). The 1 xoshiro hunk is a real display-time divergence: `say cook food` runs `Execute Look` before `Execute Claude Cooks` (which moves the wooden tray into the kitchen), and FD's Look lists the tray anyway because FD expands the room's dynamic object list at DISPLAY time, after the whole turn's actions; Scarier lists at Look-execution time. Vanilla-mode FD diff would add 18 pure-RNG OneOf hunks (spectre appearance/banish + dog-chase text variants), hence golden-backed. |
| Run, Bronwynn, Run! | 512 | **MATCH** ✅ | 0 (xo 0) | **REWIRED + engine-FIXED 2026-07-04, now a FULL WIN** (Duke Alaric / `YouHaveWon` endgame, 600/600) in BOTH engines. Route re-derived from the CASA solution (Phoebe M. Fuentes) with port-accurate verbs from the game's own `WALKTHROUGH` command (`[wlkthrgh]`, task `Walkthroug`, Larry Horsfield); the raw CASA original-release script had 97 unparsed cmds. **Fixed the "Scarier ends prematurely" bug** (was a turn-148 loss vs FD's full playthrough): `pass_character` had no `HaveSeenLocation` case, so `Player Must HaveSeenLocation BridgetSLi` fell through to the best-effort `return 1` — task `Caught121` (LocationTrigger `InTheWealt14`, "…walk straight into a search party…from the cathedral square") fired the instant the player reached the cathedral-square street, *before ever entering Bridget's house*. Added the case mirroring the location-subject `HaveBeenSeenByCharacter` handler (player consults `loc_seen`; non-player observer never seen, FD clsUserSession.vb:222). Whole corpus still baseline in vanilla (no golden churn). **Finale timing solved (no engine change):** the in-game walkthrough's tail stopped mid-ambush at a single `fire arrow at outlaws` fired at `Riders1==3` (a silent no-op). `FireArrowA` Executes 4 sub-tasks — `RightSideT` (InTreeS4 + `Riders1>3` ⇒ **lose**, too early), `WrongSide*` (InTreeN3 ⇒ lose, wrong tree), `RightSideG` (InTreeS4 + `Riders1<3` ⇒ **win**: +15, move `YouHaveWon`); `Riders1` counts 60→0 (`RidersCoun`) and hitting 0 loses (`RidersKill`). Rewrote the tail to exactly **5 waits + one `fire arrow at outlaws`** — the sole winning value (K=4 loses early, K=6 lets the countdown reach 0). golden committed (vanilla 0 = strict self-diff). **The 3 residual xoshiro hunks are FIXED (2026-07-04) as 3 general engine fixes** (see TODO): (a) specific-override refs reordered to FIRST-command order (FD RefsMatchSpecifics/GetAlternateRef) — `say to woman return horse` (matched command 2 of SayToCharacter, refs [char,text]) now reaches `TellLadyTo1` (Specifics [text,char]) and surfaces its `HorseAsked1` restriction message; (b) clsObject.Description getter default — an empty-composition object description reads as "There is nothing special about <the object>." (`x spinning shed` → descriptionless `Shed1`); (c) `<ReturnToDefault>` on a DisplayOnce segment resets the Displayed flags up to and including itself, so Task12's 4-line atmosphere cycle restarts (2nd woodcutter line). MATCH 0\|0, golden re-blessed (exactly those 3 lines), xoshiro budget 3→0, whole corpus unmoved in both modes. Vanilla adds 7 pure-RNG bog "…move about N metres…" pebble-throw hunks (hence golden-backed, not FD-diffed). See TODO_a5_walkthrough_bugs.md. |
| Die Feuerfaust | 648 | **MATCH** ✅ | 0 (xo 0) | **REWIRED 2026-07-04 as a MAX-SCORE WIN** (`*** CONGRATULATIONS! ***`, "scoring the maximum 800 points!", 650 turns; internal 804 — the author awards >800 total and Endgame11 fires on `Score>=800`; an exact-800 sum is unreachable, FBA-style overshoot) from the game's **own built-in `WALKTHROUGH` command** (`wlkthrgh` task, Larry Horsfield). The old script was the CASA *Spectrum* solution — wrong for this ADRIFT-5 remake (138 unparsed cmds, plays nothing). The in-game walkthrough is itself a **stale 2020 snapshot** (game tasks revised to 2022-06-06), so it desyncs in Parts 3–4 and does **not** win even in FrankenDrift; **adapted** to route around the drift: (1) `get all` before the burning-alley collapse; (2) the whole zampf-trap puzzle rebuilt (`cover pit with poles/leaves`, `lay trail`, wait ×3 at the tree, `pierce zampf's ears`/`make reins`/`put all in knapsack`/`mount zampf`/`cast stark`); (3) the women rescue (`wake woman`, `say wake the women to gundred`, `tell women join hands and hold on`, `turn opal`); (4) `put sword in scabbard`; dropped the junction `get all except papers`. The built-in text stops at 791/800; the missing points were found by an A5_DEBUG_SCORE audit (Magnetic Moon method): `listen to traders` +2, `search beams` at the Lubeck cathedral porch +3 (independent of `look under beams` — different flag), `look around corner of tent` +3, and the **split barricade casts** (heile/stark at militiamen and captain separately = +20 vs `at all` = +10; costs 2 turns inside the 15-turn `Barbarians4`→`KilledByNa` death window, reclaimed by dropping the optional `climb onto barricade`/`x wagon` examines). Not obtainable (author bugs, FD agrees byte-for-byte): `x cottages`, `ask willy about the men's plot` (shadowed by AskLandlor1), `x street` (unresolvable ambiguity), `show amulet to gundred`, `look out of slit` (no executor); the chieftain's-tent slit +5 is a death trap. The old 3 residual hunks are FIXED as 3 general engine fixes (see TODO): (a) `%CharacterName[key]%` pronoun-replacement via the PronounKeys ledger ("He ignores you."); (b) character `BeAtLocation` ANYCHARACTER + resolved LocationKey (the chieftain's-tent presence line); (c) FD's AddSpace is an unanchored contains-test whose identifier runs admit no spaces (the "closed⎵⎵⎵and" triple space). Whole 39-game corpus byte-identical in both RNG modes after all three. |
| Illumina | 12 | **MATCH** ✅ | 0 (xo 0) | **WIRED 2026-07-04 as a FULL WIN** (`*** You have won ***`) from the author's CASA solution, **VERBATIM** (Finn Rosenløv; 12 commands). Initially mis-wired 0\|5 with `open guard room door` "corrected" to `open southern door` and the FD failure filed as an FD parser bug — **that diagnosis was WRONG**: `read sign` runs `SetProperty cl_Door _ObjectNoun Guard room door`, the ADRIFT 5 **mandatory-property rename** (`_ObjectArticle`/`_ObjectPrefix`/`_ObjectNoun`, clsUserSession.vb:1972 — `_ObjectNoun` replaces `arlNames(0)` ONLY), so the door legitimately answers to "guard room door" and no longer to "door"/"southern door". FD implements it; Scarier ignored it (its `open southern door` success was accidental — the stale pre-rename name still matched). Proven symmetrically: with the author's phrasing FD wins/Scarier "Open what?"; with the substitute Scarier wins/FD "Open what?". **Fixed by implementing the `_Object*` overrides in Scarier** (match + display, see TODO) → author's verbatim solution wins in BOTH engines, MATCH 0\|0, golden re-blessed, budget 5→0. |
| Starship Quest | 873 | **FULL WIN** ✅ | 0 |  **DEATHLESS `*** CONGRATULATIONS! ***` at 795/800, 2026-07-04** — rewired from the game's OWN built-in `WALKTHROUGH` (`[wlkthrgh]`, task `Walkthroui`, Larry Horsfield, 854 cmds, LastUpdated 2022-06-07) and **adapted around three stale spots to play all the way through to the win** (previously the whole run was stuck: the walkthrough was missing the `o`/`b` Start-Options preamble the harness does NOT auto-feed, so every command bounced off "Please press O" and the "MATCH 0" was two identically-stuck transcripts). Fixes, all verified vs FrankenDrift: (a) **restored `o`/`b`** at the top; (b) both mid-game shuttle "test" launches — `remove backpack`+`drop backpack` before `sit in chair`, `get`+`wear backpack` after `stand up` (the "can't sit … backpack" restriction post-dates the built-in); (c) **the bearion chase** — SEE the bearion first (`SeeBearion`), tame the **Rhinopine** at End Of Path with nuts + `lay trail of nuts` to lure it to Bend By Tree so climbing the tree fires `ReachTree` not the death `ReachTreeY`, then `dodge bearion`; (d) **stone-hut sacrifice** — `climb on beam`, one `z`, then `d`; (e) **tigerilla skin** — `get skin` (the built-in's `get skin and tail` errors: "tail" is an unknown word in this build, so the skin was never picked up and `wrap vegetation in skin` failed "…will not help you in this game"); (f) **endgame shuttle launch** — `get gold disc` out of the backpack BEFORE `remove backpack`/`drop backpack` (the built-in dropped the backpack with the discs inside, so `put gold disc in slot` failed and the launch buzzed "INSERT COURSE TAPE!"). With the gold "Galaxis" course-disc in hand the whole redesigned endgame plays: dock at the golden Starship *Galaxis* → `put matrix in column` → elevator `say power`/`say control` → Guardian gives the blue disc → `lower dome`/`put disc in slot`/`press engage drive` = WIN. **UPGRADED to the PERFECT MAXIMUM 800/800, MATCH 0|0 (2026-07-04):** the earlier "feed vs lure mutual exclusivity" theory was WRONG — `lay trail of nuts` does not consume the brown nuts, it moves them onto the tree branch at the bend (`MoveObject Nuts1 ToLocation SittingOnB`), so after the lure you climb the tree, re-pick them (`GetNuts2` allows it because the lure reset `BrownNutsP=0`; the repeatable `PickNutsSc` +5 does NOT re-award — confirmed identical in FD), pocket them (hands free to climb down), take them back out (`GetNutsFro`, the RP sniffs) and `feed rhinopine` = `DropNutsRp1` **+5 → 800** ("scoring the maximum 800 points!", 872 turns). The RP stays under the tree for the collision rescue. The old residual **"Native's" hunk is FIXED as a real engine bug**: an OO `.Description` read (`item_description`) returned the composition RENDERED PLAIN, so the `<>` segment-join marker was stripped before the re-embedding response's `auto_capitalise` pass, which then capitalised the author-lowercase "native's …" append that FD's still-marked-up buffer blocks; `.Description` now returns the marked-up text (`a5text_describe_marked`) like FD's `Description.ToString`, and the boundary re-cap in `a5text_display_alr` only runs when ALR round 1 actually rewrote something (FD's display cap runs on marked-up text where any tag blocks new caps; per-fragment capping already adjudicated those positions). Suite otherwise byte-identical. golden re-blessed; MATCH 0|0. |
| The Lost Children | 417 | diverge | 383 | **BUG 1b** *misses* a first-turn event (lost broadsword/money); stray "Follow Cancelled."; **BUG 9** parser wording; "SouthEast" vs "Southeast" |
| Magnetic Moon | 472 | diverge | 32 | **BUG 5** NPC "Captain Rosenloev" omitted from room characters; **BUG 8** missing Y/N prompt; 34→32 after end-game text (win/lose banner now matches) |
| Revenge of the Space Pirates | 479 | **FULL MAX-SCORE WIN** ✅ | 0 (xo 0) | **REWIRED 2026-07-05 to the MAXIMUM 550/550** (`*** CONGRATULATIONS! ***`, "…in 479 turns, scoring the maximum 550 points!"), MATCH 0\|0 both modes, golden-backed. The committed script had been the stale CASA solution (15/550, desynced at the hotel; the old BUG 4 `id` alias note below described that derailed transcript). Rebuilt by hand over two sessions (410-point WIP, then finished): the **hotel bill is 15 points** and only payable after the Wednesday gym pay-off (`PaidOff1` gates the foyer stop on `d`/`n` auto-skips); duct rules (ONE held item, worn holdall follows, `goto <code>` fast-travel after `read index`); Grande's nurse-lure (21-turn `NurseGone` window — raid the medical centre, close the cupboards, `replace grille` from the duct or die); dead-cockroach-in-sandwich clears the comms op twice (10-turn windows) for the sub-space radio (freq 180452 / send / WOTAN / 4361 9292); **the radio call must precede `set timer`** (`CoOrdsSent` restriction); `goto s1` rides the fired rope straight to the radar; entering Jaelaine's room after `BombPlante=1` detonates the bomb and starts the fight — each corridor guard dies the SAME turn he is seen (`SeeGuard`/`SeeGuard1` 1-turn events; guard A needs Symon armed with the blaster); grenade the FCC within 3 turns of reaching its doors (`OutsideDoo2`); collect Grande with the guard's key (`NIntoA11`); launch: drop rifle+holdall, sit in chair, black button (power), `input 4361 9622`, green button → Location70. |
| Return to Camelot (RtC) | 118 | **FULL WIN** ✅ | 0 (xo 13) | **REWIRED 2026-07-04 to `*** You have won ***`** from the author's own .doc walkthrough (Finn Rosenløv, IF Comp 2011). Two classes of fix. (1) *Walkthrough*: courtyard↔kitchen stairs — the doc's `d`/`up` are custom-task synonyms the Runner's built-in movement pre-empts (a real, FD-shared, no-route) → use the compass exits `s`/`n`; and `wear armour` (ambiguous: battered suit vs. racked suits) → `wear battered suit of armour`. (2) *Engine*: the true blocker was `TaskExecution`. RtC is **Version 5.000020**, which predates ADRIFT 5.0.22's `<TaskExecution>` element and ran under the v4-compatible **HighestPriorityPassingTask** mode. Its central `unlock chain` puzzle (Task324, sets Variable6 to free the armour) only fires as a *lower-priority passing* task after the stock library `unlock %object%` "cannot be unlocked" fallback — impossible under HighestPriorityTask, where that failing-with-output library task claims the turn. **Scarier now version-gates the default** (`a5model.cpp`: element-less files with file-version < 5.000022 ⇒ HighestPriorityPassingTask). **FrankenDrift hardcodes HighestPriorityTask regardless of version and CANNOT win this game** — confirmed by direct dotnet run (stuck at "cannot be unlocked"). This is the one deliberate Scarier/FD divergence: golden = Scarier's winning transcript (vanilla 0), xoshiro carries the 13-hunk FD gap (RNG-independent). Same shape as Illumina/BugHunt "win in the correct direction". |
| Treasure Hunt in the Amazon | 133 | **MATCH** ✅ | 0 | golden committed; conformant — plays through to `*** You have won ***`. Long road: BUG 3 group-property `buy` (44→41), the `ts_*` time subsystem (`%PropertyValue%` runtime override, `Execute Look`→`Beforeplay1` Date lines, per-item event-tick clock drift) 41→1, and finally the title's centring space (System `<RunImmediately>` PlayTune + markup-aware `bHasOutput`) 1→0. See TODO top entries. |
| Grandpa's Ranch | 93 | diverge | 125 | **BUG 2** missing room desc/exit+object listings; **BUG 7** `dig` → "nothing to dig with" (shovel not taken from `x shelves`) |
| Jacaranda Jim | 440 | diverge | 439 | **BUG 4** pronoun `get it`/`get them` unresolved (35×); **BUG 2** missing "Exits are …"; **BUG 10** leaks template `%AlanRemarks[%AlanRemarkIndex%]%`; darkness handling |
| Six Silver Bullets | 33 | MATCH ✅ (xoshiro) | 18 (xo 0) | real winning walkthrough (*** You have won ***). **BUG 14 RESOLVED**: the time-trap subsystem desynced the `Roller` RNG stream (the `'RAND(1,16)'` restriction RHS was never drawn), mistiming the sniper/bomb events and killing the player. Fixed (RAND-on-RHS draw + runtime location-group membership + `LocationOf %Player%` resolution + room-view `pSpace`); xoshiro now 0 (full FD-logic MATCH), vanilla 18 is irreducible System.Random RNG noise. See TODO top entry. |
| Stone of Wisdom | 137 | diverge | 6 | **REWRITTEN** as a real winning walkthrough (50/50, *** You have won *** in FrankenDrift, 137 turns). Surfaces **BUG 13** troll-knockout death — after `hit troll with ring` knocks the troll unconscious, Scarier still fires the troll's "you-didn't-act → die" event on the same turn, killing the player at turn 10 (FD's knockout cancels it). 3 of the 6 hunks are inherent RAND troll-taunt lines (default-RNG); the rest are that death + the loss-vs-win cascade. See TODO_a5_walkthrough_bugs.md. |
| Things That Go Bump in the Night | 310 | **MATCH** ✅ | 0 (xo 0) | **WIRED + FIXED 2026-07-02** (golden) — full max-score win (`*** CONGRATULATIONS! ***`, 250/250), script derived from the game's **own built-in `WALKTHROUGH` command** (three moves corrected for this build's cut-scenes: dropped a spurious intro `n`; added `snuffio`+`s` so the guard patrol carries you to the Treasury door in the dark). Was 8\|8 (two RNG-independent bug families), fixed same day to 0\|0: (1) the fatal `drop all` over-expansion was FD's two-phase `BeExactText` dance — the per-item refine binds an EMPTY command reference (a `MustNot BeExactText All` gate passes during refine, fails silently after), so the `RemoveBeforeDrop` helper yields to the real `DropObjects`; plus the final `resolve_plural` pass now iterates the refined item set; (2) `SetTasks Execute <task> (%objects%)` passes a same-name reference straight through (key + typed text, FD vb:2188) so the take chain's BeExactText gates behave under `get all`; (3) an Execute'd task failing WITH a restriction message now shows it (dark `get dirt` → "It is too dark to find the dirt.") via a per-command response scope with htblResponsesFail text dedup and the pass-cancels-fail flush rule (which keeps Grandpa's flashlight and TBN's taken-grapnel fails suppressed). See TODO_a5_walkthrough_bugs.md (DONE entry). |
| The Lost Labyrinth of Lazaitch | 452 | MATCH ✅ (xoshiro) | 8 (xo **0**) | **WIRED + FIXED 2026-07-02** — full 520-point win (`*** CONGRATULATIONS! ***`, 451 turns) from the game's **own built-in walkthrough** (`WLKTHRGH`, extracted verbatim from the `cl_Walkthroug` model task, zero corrections). Initially 403\|403, all Scarier bugs; fixed same day to **xoshiro 0 = full byte-exact FD-logic conformance**: (1) location seen-tracking `st->loc_seen` (the FahrenLayb teleport child's `Location94 MustNot HaveBeenSeenByCharacter Player` always failed under the old always-"seen" stub — the "Fahren Layburn" spell + whole village endgame; the same stub also un-gated the two room-desc segments); (2) restriction key `ReferencedObjects` now falls back to the singular `ReferencedObject` binding at LOOKUP (`sheath sword`; binding at capture instead leaks stale aliases — regressed AxeOfKolt `drop chainmail`); (3) CharHereDesc `##CHARNAME##` name-casing round-trip applies to single characters too ("White Stallion"/"Unicorn"); (4) FD's Look Before+AggregateOutput message dance — two bTestingOutput renders around the actions, each drawing for room-view `<# OneOf #>`, response pinned to the FIRST render when the picks differ, slot reserved pre-actions — aligning the riding-variant draw stream. Residual vanilla 8 = System.Random-vs-xoshiro riding picks (RNG-bound, like JJ/SSB); no vanilla golden. See TODO_a5_walkthrough_bugs.md (DONE entry). |
| The Euripides Enigma | 427 | **MATCH** ✅ | 0 (xo 0) | **WIRED + FIXED 2026-07-02** (golden) — full 400-point win (`*** CONGRATULATIONS! ***`, 426 turns) from the game's **own built-in walkthrough** (`WLKTHRGH`, `cl_Walkthroug1`) with two script corrections: dropped a spurious `hit fork on face` (it *closes* the already-open canyon opening in this build) and `put pistol in pistol holster`→`holster pistol` (ambiguous phrasing). **Fixed an exponential ALR-recursion hang**: ~25 identity ALRs (a 25-asterisk banner, `OldText`==`NewText`) branched `process_inner` ~25-fold/level to the depth-8 cap (>60 s CPU); ported FD's `If sText = sALR Then Exit For` guard. Then the 11\|11 residual → 0\|0: (1) the room-view `ExplicitlyExclude`/`ExplicitlyList` listing filters now read the runtime SetProperty override layer (the boom box / drone set `ExplicitlyExclude <Selected>` when prose takes over — 7 hunks); (2) per-command pass-response text dedup on the direct path (FD's htblResponsesPass keying: `press on` Executes two crawler-journey tasks with identical text, each `Execute Look` — journey and view shown once); (3) a non-aggregate Before completion on the response-map path retires its `<DisplayOnce>` segments (FD's pre-action ToString, vb:1167) — the crevice squeeze re-showed its first-time paragraph. See TODO_a5_walkthrough_bugs.md (top entry). |
| The Dwarf of Direwood Forest | 236 | **MATCH** ✅ | 0 (xo 0) | **WIRED + FIXED 2026-07-02** (golden) — the game's **own built-in walkthrough** (`WLKTHRGH`) with one correction (`creep south` at "By Guard Room"). **Fixed the hide-in-beard bug**: the plural `%objects%` per-item binds clobbered the singular `ReferencedObject` alias, so `hide droppings, knife and key in beard` tested the general task's `cl_CanBeHidde` restriction against the *items* instead of the beard container and the beard-specific override never fired; `bind_reference` now skips the singular aliasing when the command carries both a plural `%objects%` and a singular `%object%` (FD's GetReference resolves `ReferencedObject` only to the `object1` reference, clsUserSession.vb:3990). **Conformance MATCH, not a win**: FD itself is trapped at "By Guard Room" on the return visit (the `{*}` catch-all `cl_NullAtStar` preempts the room's North movement, which the real Runner evaluates first), and Scarier now reproduces that trap byte-for-byte (~166 trailing "Please press O then press Enter." replies). Re-derive the golden if that precedence is ever changed. See TODO_a5_walkthrough_bugs.md (top entry). |
| The Dwarf of Direwood Forest (DDF, 2022-06-07 build) | 238 | **MATCH** ✅ | 0 (xo 0) | **WIRED + WON 2026-07-03** (golden, `DDF.blorb` SHA1 `2fb6f724…`) — the ORIGINAL release, without V9's mis-bound `{*}` disclaimer task, **wins the maximum 250/250 in 237 turns** in both engines (xoshiro byte-identical; the 2 vanilla hunks are the wagon's `<# OneOf #>` exit phrase, pure RNG). The built-in `WLKTHRGH` needed exactly THREE corrections (it predates the fuse→rope rename): `get rope`, `put rope in oil`, and critically **`light rope`** — the West-Side-of-Field capture `cl_Fdd017Syst1` is disarmed only by `cl_LightFuse` (the LightObjec specific on `cl_FuseO`) being complete; `light fuse`/`light oil` (General `cl_StartFire`) and `press lever` (`cl_LightFuse1`) start the same 12-turn explosion event but leave the death armed (author bug). Surfaced+fixed 2 Scarier bugs: (a) plural-slot specific-override matching — `ref_info_for_name` resolved the `%objects%` slot via the singular `ReferencedObject` alias, which with a `hide %objects% in %object%` command holds the CONTAINER (post hide-in-beard fix), so `hide axe in vegetation`'s override `cl_HideObjInV1` ([Sword, cl_Vegetation]) matched [veg, veg] and missed — now reads `ReferencedObjects` (the per-item bind); (b) NPC walk enter/exit messages (`wk_show_enter_exit`) were emitted raw — now routed through `a5text_process`/`render_plain` so a CharExits property's `<#...#>` expressions evaluate (the wagon's OneOf). Plus: no paragraph break after an intro whose render ends with `<cls>` (the "\n\n" landed after the wipe marker and survived as two stray blanks). |
| The Call of the Shaman | 151 | **MATCH** | 0 (xo 0) | **MATCH 2026-07-02** (golden) — full 265-point win (*** CONGRATULATIONS! ***) derived by play from the bundled ROT1 hint sheet. The 3 endgame-banner hunks (BUG 15, all RNG-independent) are fixed: (1) added the `%turns%`/`%Turns%` built-in to `eval_function` (ci-insensitive, value `st->turns-1` matching FD's post-`Process()` bump) → "151"; (2)+(3) the credits URLs `Https://`/`Http://` were leading-capped by the plain-text Display-boundary re-cap after the `<del>` tag was stripped — fixed by capping the room view on its marked-up buffer in `view_location_impl` and suppressing the boundary re-cap's `^`/`\n` line-start rules (`auto_capitalise_ex allow_line_start=0`). |
| Alyas of Starhollow (AoS v.4) | 650/650 **MAX** | **WIN** | 0\|0 | **MAX-SCORE WIN 2026-07-04 (golden-backed, MATCH 0\|0 both modes; the game prints its special "scoring the maximum 650 points!" finale).** Larry Horsfield. The game's *embedded* walkthrough (type `WALKTHROUGH`/`HELP`) is for an OLDER map and never wins on this v.4 build — replayed verbatim it desyncs at the mill race and ends stuck in the starting yort. `AoS_walkthrough.txt` is a from-scratch re-derivation, verified byte-identical vs FrankenDrift in both RNG modes. **The last 30 points (620→650)** = six missable 5-point content finds, located by dumping all `IncVariable Score` tasks and diffing against an `A5_DEBUG_SCORE` trace of the run: read notice (dressing room), x table graffiti (inn), read sign at By Paddock (**before** knocking — the sign task teleports Muffin ToSameLocationAs the mule, ruinous once Muffin follows you), rub the worn onyx ring (an annoyed genie confiscates it), read labels (dispensary), and open case + x pipe (the finale hands you the Pipe inside a *closed* wooden case). Verified over-max alternates the author's 650 does NOT count (walkthrough header has details): Dusty-return `cl_NoSack2` +5, `rig dinghy` 25 vs the manual fit/tie 15, and dodging the −15 `cl_NoPlank/NoNails/NoNails2` return-to-lake tool penalties (the author's own solution eats them) — all together 680/650. **Chasing the max surfaced an engine fix (BUG 20, a5text)**: the Display-boundary ALR pass ran over PLAIN text, so it matched a TextOverride *across a stripped formatting tag* that blocks FD's Display-time `ReplaceALRs` — AoS names its `Known` guard `The guard<Halberd>`, whose embedded tag defeats the game's own "The guard comes marching from above" override in FD/the real Runner (`Known` ⇒ raw ProperName is used and the ALR sees the tag mid-span); Scarier wrongly applied it at the boundary. `a5text_render_plain` now drops an `A5_ALR_MARK` ('\003') sentinel wherever a tag is stripped, boundary matching is blocked exactly where FD's is, and `finish_turn` strips the marks (`msg_has_output`/`pre_alr_ink` treat the mark as non-ink). Whole 38-game corpus byte-identical in both RNG modes. Key divergences fixed (all confirmed in-engine): mill `put vegetation in race` (not `push…in water`); `get`+`read book` for the North@West-Bank `cl_BookRead` mission gate; `climb up tree` (bare climb only prompts) + the talking-oak 1/2/3/4 dialogue with hands free; temple `in` (not `n`) to the dark room, `follow string` only after `tie string to column`; inn `1` (buy a round); keep the beggar-dropped tea towel to wrap the sandwiches so hands are free to mount Muffin; stow the undroppable cloak in the bag before the mermaid dive; the rebuilt castle (red-lantern brothel NE, apothecary sleeping-powder gated on having seen Room 3, drug wine, take uniform); **stash the cloak in the flour sack + leave the sack at the drawbridge** (keep the quest bag) so the keep guards don't jail you; pull Zagdor's dungeon lever *before* the central-well descent reveals the Sigil of Tarkyod brick; `qwerty` despatches the thief; give the Sigil to Zagdor and walk him into the King's Audience Chamber (auto-vanquishes Xarkzis, returns the Pipe, teleports you back to the lake); repair the dinghy, cut a mast from the yort frame, rig the bed sheet as a sail, `push dinghy`. *(Historical: the earlier "WIRED 2026-07-03" pass wired the losing embedded walkthrough at diverge-1 and made three real engine fixes en route — kept below for the record.)* **Three fixes 2026-07-03 (~~13~~ → 9 → 3 → 1):** (a) **`put all in bag` restriction** — the plural override scan treated an `AggregateOutput` **merge** (2nd+ item folds into an existing entry, `ents.size()` unchanged) as *no output*, so it failed to break and fell through to the lower-priority `cl_PutAllInBa`, whose `EverythingHeldBy Player` action dumped the food+money into the bag; Scarier lost the `PutObjectI` `MustNot BeObject cl_Food` "makes a mess" refusal, reported "You are not carrying the food", emitted a spurious "…into the leather bag." dup, and cascaded the food's location. Fixed by counting merges as output (`resp_map::nmut`, used by `sink_len`); FD's merging `AddResponse` claims the turn (clsUserSession vb:1295). (b) **phantom `up` exit** (6 hunks 417–546) — the move-failure route list ("…only **up and** out." vs FD "…only out.") came from `Location HaveBeenSeenByCharacter <NPC>` wrongly returning True: Scarier's location-seen handler fell back to "seen" for any non-player observer. FD sets `HasSeenLocation=True` in exactly one place (clsUserSession.vb:222) and **only for the Player**, so an NPC has never seen any location; changed the non-player branch to return False (a5restr.cpp). AoS's Flour Store `Up` is gated on `cl_Mission1 HaveBeenSeenByCharacter Finnjart` (the ferryman), which stays blocked. (c) **`put all in bag` FailOverride** (2 hunks, transcript 1001/1775) — **NOT** a `give food` NPC divergence (that earlier diagnosis was wrong: `give food` at transcript ~1340 fails *identically* in both engines and never moves the food; the food's location is byte-identical throughout, verified by `i`/`search bag` probes). It is a **message-selection** divergence on the 2nd/3rd `put all in bag`, when the only remaining un-baggable held item is the wrapped `cl_Food`: Scarier showed the food's own "makes a mess" refusal, FD shows "You are not carrying anything." That string is the **general `PutObjectsInOther` `<FailOverride>`** (not cl_PutAllInBa's like-worded restriction). FD's `AttemptToExecuteTask` finalize (clsUserSession.vb:789) shows the parent FailOverride whenever `htblResponsesPass.Count=0 AndAlso FailOverride<>"" AndAlso ContainsWord(input,"all")`, discarding the child overrides' per-item fail messages. Scarier missed it because its plural-map path is gated on `n_ref_items>1`, so an `all` that collapses to a **single** surviving item fell to the direct path and printed the child override's fail message. Fixed in `run_general` (a5run_action.cpp): an `all` command (`%objects%` text contains "all") whose parent has a `<FailOverride>` now routes through the response map even for one item, and when no child override passes, the buffered fail messages are replaced by the parent FailOverride. A genuine singular `put food in bag` (no "all") stays on the direct path and keeps "makes a mess". No corpus regressions (whole corpus unchanged; AxeOfKolt/TBN/LostLabyrinth/Dwarf/Amazon all still 0/baseline in both modes). Remaining 1: `put all in bag` spuriously reports "You are not carrying the gold gonks. … You are not carrying the silver ginks." (1 hunk). **ROOT CAUSE PINNED 2026-07-03 — it is an author-ALR suppression Scarier can't reproduce, NOT the "iteration-ordering / FD-refine-passing-set" story previously recorded here (that was wrong).** Verified end-to-end by instrumenting FrankenDrift (`ExecuteSubTasks`/`AttemptToExecuteSubTask`/`AddResponse`/`Display`/`ReplaceFunctions`) and dumping the AoS module (`A5_DUMP_XML`): (1) The `all` set is **identical** in both engines and **does** include the two nested coin objects `cl_Coins` (gold gonks) + `cl_Ginks1` (silver ginks) — they were seen & recursively-held at match time (money-pouch `cl_Pouch` is Openable+**Open**, so `BoundVisible` propagates to the room). (2) Both engines iterate in object-index order — `cl_Pouch` **first** — and moving the pouch into the **worn** bag (`Pouch`) un-holds the nested coins; when the per-item loop reaches the coins, the *general* `PutObjectsInOther` `Must BeHeldByCharacter %Player%` restriction fails in **both** engines with message template `%CharacterName% [am/are/is] not carrying %TheObjects[%objects%]%.`. FD does **not** fix the set at refine and **does** re-fail the coins (FD `SUBTASK`/`PASSRESTR` trace: `cl_Coins`→False, `cl_Ginks1`→False). (3) FD keys buffered *fail* responses by message *template* (`AggregateOutput`), so the two coin fails **merge** into one entry whose refs aggregate to {cl_Coins,cl_Ginks1}; at flush it renders the single string **"You are not carrying the gold gonks and the silver ginks."** — which the author's suppression **ALR** `cl_YouAreNotC1` (`TextOverride` OldText = that exact string, **empty NewText**) blanks in `Display`→`ReplaceALRs` (confirmed: `DISPLAY-IN preALR=[…not carrying %TheObjects…] postALR=[]`). Two sibling suppression ALRs exist for the other coin forms (`cl_YouAreNotC` "You are not carrying the gold coins!", `cl_TheGoldGon` "The gold gonks and the silver ginks is not on the large leather bag.", both empty). (4) Scarier emits the two coin fails as **separate singular** messages ("…the gold gonks." / "…the silver ginks."); Scarier *does* run ALRs (`replace_alrs`, a5text.cpp) but neither singular string **contains** the merged-form ALR OldText, so `strstr` misses and nothing is suppressed. **Fix shape:** Scarier's plural fail path would have to aggregate same-template per-item fail messages into the merged `%TheObjects[%objects%]%` form (FD's `htblResponsesFail` template-keying) *before* ALR/Display, so the author ALR can match. NOT chased: that touches the shared plural response path (run_general / resolve_plural / resp_map) the passing goldens (AxeOfKolt/TBN/LostLabyrinth/Dwarf/Amazon) depend on — too much regression risk for one cosmetic hunk fully explained by an author text-override. |
| Finn's Big Adventure (FBA v.3c) | 572 | **MATCH 0\|0** | 0 (xo 0) | **WIRED 2026-07-03 — MAX-SCORE WIN** (`*** CONGRATULATIONS! ***`, "scoring the maximum 500 points!", 572 turns), golden-backed (vanilla byte-exact, dotnet-free). Larry Horsfield; no public walkthrough — derived by blind play over sessions 1–7 (see `TODO_fba_walkthrough_progress.md`). **FULL MATCH as of 2026-07-03**: the one RNG-independent hunk was the loc-92 `cl_AtButcherS` LocationTrigger — its "rope tied" restriction (`cl_RopeTiedPa Must BeEqualTo 0`) carries the "you pull on his leash to stop him" `<Message>`, which FD shows on the failing System task but Scarier's `attempt_event_task_impl` swallowed (returned silently on any restriction failure). Fixed by emitting `st->restriction_text` (FD's `sMessage = sRestrictionText`) — see the TODO "event/System/walk restriction-fail `<Message>`" DONE entry. Internal Score overshoots to 510 (the second-restaurant "Chop Shop" meal banks +20 when +10 sufficed) but both engines print the `Score>=MaxScore` "maximum 500" win message. |
| The Book of Jax | 650 | **MATCH** ✅ | 0 (xo 0) | **WIRED 2026-07-03 as a MAX-SCORE WIN** (`*** CONGRATULATIONS! ***`, "scoring the maximum 500 points!", 652 turns) — Larry Horsfield; derived by play (decoded hint sheet), not the built-in solution. Both engines reach the win, so all hunks are Scarier text divergences. Wiring surfaced+FIXED **BUG 16** (503→55): the stock `Look` task's `AggregateOutput` `CompletionMessage` segments were rendered *without* honouring `<DisplayOnce>`, so the first-turn "(Don't forget … use the LUMINO spell.)" hint (a DisplayOnce segment gated on "prow seen") reprinted on **every** room view where FD shows it once. `render_look_string` (a5run_action.cpp) now mirrors `eval_desc_into` — a shown DisplayOnce segment is suppressed thereafter and retired on real output (`marking_display=1` at the three real render sites; the two pre/post-action test renders stay `=0`). Whole 34-game corpus stays byte-identical in both RNG modes (no other golden's Look aggregate carries a DisplayOnce). Wiring also surfaced+FIXED **BUG 17** (55→2): a bare object-key OO-property inside a room-description `<# #>` expression (potting-shed `<#LCASE(cl_Door1.OpenStatus)#>`; farm gates/half-doors `cl_gate1/2`, `cl_door3/4/6`, `cl_hatch`) was not resolved — Scarier left the literal key ("…which is cl_door1." vs FD "…which is locked."). `replace_expressions` (a5text.cpp) only ran `expr_substitute` (`%ref%.Prop` + `%func%`) on each `<#…#>` body; the bare key never got the `a5expr_replace` pass because `protect_exprs` had hidden the body from `process_inner`'s outer ReplaceOO. It now runs `a5expr_replace` on the substituted body before `a5_eval_sexpr`, mirroring `a5text_eval_expression`. 53 of 55 hunks were this. Wiring also surfaced+FIXED **BUG 18** (2→1): the game's custom `%Turns_Taken%` counter (a per-turn `TurnBased` event `cl_TurnsTaken1`→`cl_TurnsTaken2` `IncVariable`) over-counted by 6 (601 vs FD 595) — Scarier's event-fired task iterated the command's **whole** leftover `%objects%` set, but FD's post-Display `NewReferences` is only the LAST *displayed* message's refs (its `AddResponse` `bHasOutput` gate never enters an empty-output response). On `put all in bag` re-putting items already inside the bag (their `cl_PutObjInBa` completions render empty), Scarier left `n_ref_items`=7 (the silent moves) so the per-turn event ticked +7; FD left 1 (the one visible "…the length of rope…" message) → +1. `resp_flush` (a5run_action.cpp) now re-applies the last **output-producing** entry's refs as the leftover, collapsing to singular when that message was single-ref. Whole corpus unchanged (Amazon's `get ammo and rifle` +2 double-tick preserved — those two takes DO share one non-empty 2-ref message). Wiring also surfaced+FIXED **BUG 19** (1→0, **FULL MATCH**): the plough-message leading pSpace, a **markup-strip-timing difference**. The `cl_EndConvo` "…plough his field…" completion is preceded by a conversation message whose *raw* text ends `…\n<font color = #01ffff>` (a trailing colour-reset tag after the newline). FD keeps markup in `sOutputText` and applies `pSpace` to the RAW buffer, so it sees the trailing `>` (non-`vbLf`) and prepends `"  "`; Scarier strips `<font>` during `a5text_process` (a5text.cpp:2053) *before* `sb_pspace`, which then saw the `\n` and added nothing. The message renderers (`a5text_describe`/`a5text_describe_ex`) now append a sentinel `A5_PS_MARK` when the *pre-strip* text ends non-`\n` but the *stripped* text ends in `\n` (i.e. FD's buffer would end in a tag/`<br>`/entity, not a real newline); `sb_pspace` treats the marker as a non-newline tail (adds the join spaces) and `finish_turn` strips it. Faithfully replicates FD's raw-buffer pSpace without deferring the whole tag-strip. **Whole 34-game corpus byte-identical in both RNG modes — zero ripple** (the marker only fires on the exact `\n`+trailing-tag shape); unit tests + `make sanitize` clean. See TODO_a5_walkthrough_bugs.md. |
| I Summon Thee! | 15 | diverge | 6 (xo **1**) | **WIRED 2026-07-07 as a FULL WIN** (`*** You have won ***`, `ISummonThee.taf`, Theodidactus 2020). No public walkthrough — solved by RE. You are a demon bound in a warding circle in the Upper Tower; the circle wards cosmodemons/gods/djinns/undead but **not nymphs**, so a first-turn `conjure nymphs` erases it (task `CheckNymph`), skipping Clark's entire forced intro and leaving power at exactly 25 (`InitialValue` 28 − conjure 3), the "Beyond all mortal comprehension" tier the win needs. Then race to the vault (`annihilate vault door` — the iron door is just a General flavor-only "open" task; annihilate moves it `ToLocation60`), `take talisman` (+`RAND(5,10)` and defuses the `CheckDead` loss where the 4 cultists burn it), and `break the seal` at the Drive (`Location1`, needs `Power1>=25`). Both engines win. **Objects-Destroyed gap FIXED 2026-07-07** (vanilla 8→6, xoshiro 4→2): the win task runs `EndGame Win` as its FIRST action and only THEN `SetTasks Execute CheckEscap (Everything.StaticOrDynamic)` (the per-object tally, +1 per member at `Location60`). The group-member loop in `a5run_action.cpp`'s SetTasks-Execute handler broke on `st->game_over`, which the earlier `EndGame` had already set, so it ran only member 0 (Talisman, not at Location60) → `Objects Destroyed` 0 vs FD's 2 (Circle+Door) → Final Score 39 vs 41. FD checks game-over only in the main loop, so a sub-task iterates every member regardless; fixed by capturing `was_over` before the loop and breaking only on a game-over NEWLY triggered by a member's own run (preserves the defensive break for a member that itself ends the game; LostCoastlines world-gen still MATCHes 0\|0). Now `Objects Destroyed: 2`, `Final Score: 41`. **Hunk (1) FIXED 2026-07-07 (xoshiro 2→1):** the **`%object%`→key** fix (a5text.cpp `eval_function` now returns `MatchingPossibilities(0)` = the entity KEY, Global.vb:1792) closed the nymph + "The Door" names — the author keys `Nymphs`/`Door` equal their nouns. It regressed Halloween as feared, but the cause was NOT the `.Children` On/In filter alone: the garbage line came from `dk_ListFirstL1`'s `.Children(Characters, In)` returning the *object* hook instead of *characters*. Fixed by making `.Children`/`.Contents` honour the entity-TYPE arg too (see engine-fix #15). Zero corpus blast radius; Halloween holds MATCH 0\|0. **Hunk (2) FIXED 2026-07-07 (xoshiro 1→0, vanilla 6→5):** **completion-message `Rand` draw ORDER.** `Annihilate2`'s message `The %object% %Annihilateflavor[Rand(1,10)]%` drew its flavor at the wrong stream position: with `FD_RNG_TRACE`/`A5_TRACE_RAND` aligned, FD draws the `RAND(1,10)` for the flavor right at the command task's `AttemptToExecuteTask` finalize — BEFORE the turn's NPC `RAND(1,6)` event draws (flavor=5, "swallowed up by roaring flames") — while Scarier flushed the deferred AggregateOutput completion at `finish_turn`, i.e. AFTER `ev_tick_all` (flavor=4, "quite simply explodes"). Root cause: `a5run_flush_display_defers` ran only at `finish_turn`, past the event tick; the Skybreak deferral genuinely only needed to be past the **LocationTrigger drain** (SidequestE), not past the events. Fix: also flush inside `ev_tick_all`, right after `drain_tasks_to_run` and before `wk_tick_all` — the command's held draws now resolve after the drain but before TurnBasedStuff, matching FD for both games (Skybreak/LostCoastlines hold MATCH 0\|0; the `finish_turn` flush stays as the fallback for turns that never tick events). The stream realigns immediately after (talisman `RAND(5,10)`=6 in both). Remaining vanilla 5 = the documented System.Random-vs-xoshiro flavor caveat, not a bug; xoshiro-aligned mode is a clean **0**. |
| Race Against Time | 130 | **FULL WIN, MATCH** ✅ | 0 (xo 0) | **WIRED 2026-07-10 as a FULL WIN** (`*** You have won ***`) with **MATCH 0\|0 in both RNG modes + save/restore OK on the first replay — zero engine changes**. Escape-the-exploding-space-station game (ADRIFT 5.0.366, 26 rooms / 3 elevator levels): a virus has wiped out the ISL crew; you repair the sabotaged self-destruct trigger (map pin from the Chief Engineer's projected photo + silver pin from the commander's chain-opened box unlock the storage handle; plastic tube + metal ball from the deformed tube rebuild the mechanism in the last resort chamber), fire it, and outrun the 49-turn evacuation timer (`cl_WarningThr`) back to the shuttle. Airlock codes: 11275622 in / 27115694 out ("today's date" 11-27-56 + digit-sum 22, then pair-sum 94; both engines also accept the D-M-Y/Y-M-D orderings). The elevator dies in the post-activation power surge, so the escape goes through the hidden plate under the commander's desk (S, D, push panel). Script = the author's `Walk-Through RaT.doc` converted 1:1 (129 steps + `b` at the options page). The pliers step 15 marked "not absolutely necessary" IS required on this route: `cl_TakingMaos` needs them held to fish the blue fob from the door gap, and that fob is the only exit from Mao's quarters once the body is moved (the hint system says as much). Removing your helmet is a 15-turn scripted death (`cl_PlayerRemo`); the vial under Mao's desk is optional flavor that only re-skins the win text. Golden committed at the win. |
| Algernon's Conundrum | 19 | **FULL WIN, MATCH** ✅ | 0 (xo 0) | **WIRED 2026-07-09 as a FULL WIN** (`*** You have won ***`), MATCH 0\|0 both modes, golden-backed. Tiny 2-room puzzle: help Algernon get royal jelly from an angry beehive. **Surfaced + FIXED an engine bug (empty player start location):** the module leaves the Player's `CharacterAtLocation` **blank** (`<Value />`), so Scarier started the player at location "" — every command hit "You see no such thing." / unresolved `Player.Location.Exits.List`. FrankenDrift's loader (FileIO.vb:851-862) re-homes a Player whose location is Hidden or has an empty key to the **first** location in the adventure; Scarier now mirrors this in `a5state_new` (a5state.cpp) — player-only, only when `char_onobj==NULL` and the location key is null/empty. Whole 48-game corpus unmoved in both RNG modes. **Puzzle:** the pool's stones spell the combination (`four red, one blue, two yellow, three green`) — press each colour square that many times (each press just increments its counter; MoveMonume fires at red=4/blue=1/yellow=2/green=3) to slide the monument aside, then take the beekeeper's suit from the Secret Garden and `give suit to algernon` (he suits up and fetches the jelly himself). No scoring (Score/MaxScore unused → 0/0). Alt win: wear suit, open box, take jelly, `give jelly to algernon`. |
| Tribute: Return to the City of Secrets | 143 | diverge | 2 (xo 2) | **WIRED 2026-07-02 as a FULL WIN** (`*** You have won ***`, 100/100, 143 turns) from the game's **own built-in `WALKTHROUGH` command** (Kenneth Pedersen's `Walkthroug` task, extracted verbatim, **zero corrections** — the intro "Press a key" is auto-advanced by the harness). Kenneth Pedersen / Emily Short, previously mis-filed under "no walkthrough material." The 2 RNG-independent hunks: FD renders a Specific **Override** of the `TakeObjectsFromOthers` general task (`TakeGemFro` mirror gem, `TakeGemFro1` boxes gem — both grabbed via a bare `get gem` whose parent container is inferred) with a leading blank line — the vestigial paragraph slot where the general task's `(from the <parent>)` auto-note would print — which Scarier collapses. Other gems either print the real `(from the …)` note or take from the floor (no note); Scarier matches those. Cosmetic; not chased (regression risk to the 22 passing goldens for 2 blank lines). See TODO_a5_walkthrough_bugs.md. |

Anno 1700 predates this batch; it was long mis-diagnosed as having an
"RNG-gated storage-room event" — in fact the script just waited 4 turns too
few (see its table row). Since 2026-07-05 it is a golden-backed FULL WIN,
MATCH 0|0.

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
15. **`.Children`/`.Contents` OO filter ignored the entity-TYPE arg + `%object%`
    rendered the name not the key.** ✅ **FIXED & LANDED** (2026-07-07,
    `a5expr.cpp` + `a5text.cpp`; Halloween MATCH 0\|0, ISummonThee xoshiro 2→1,
    corpus all-MATCH). Two coupled fixes:

    (a) **`%objectN%`/single `%objects%` now renders the entity KEY** (a5text.cpp
    `eval_function`), mirroring FD's `ReplaceFunctions` (`itm.MatchingPossibilities
    (0)` = the object key, Global.vb:1792; `PossibleKeys` adds `sKey`,
    clsUserSession.vb:3931) — `ReplaceOO` only rewrites `key.Property` tokens, so a
    standalone key survives verbatim. This is what lets a SetTasks reference-list
    arg `%object%.Children(...)` chain from the real key, and it fixes ISummonThee's
    standalone `%object%` (author keys `Nymphs`/`Door` == their nouns → "The
    Nymphs"/"The Door", not the old A5_ART_NONE prefix+noun "The Comely Nymphs"/
    "The Vault Door"). Zero corpus blast radius; two synthetic unit tests
    (`a5_disambig_test`, `a5_objects_test`) that used a bare `%object%` probe with
    arbitrary keys (`Key1`/`Coin1`/`Lamp1` ≠ their nouns) were updated to expect
    the faithful key rendering.

    (b) **`.Children`/`.Contents` now honours BOTH the entity type (Objects /
    Characters / Both) AND the where (In / On / OnAndIn)** — new `oo_children_set()`
    + `chars_children()` faithfully reproduce FD's `ReplaceOOProperty` dispatch
    table (Global.vb:826-911), including its **empty-set fall-through** for any arg
    combination the `Select Case` doesn't list (a bare `On`, `Characters,On` on a
    container with no characters, …). The old handler ignored the type half and
    always returned objects.

    **What actually made Halloween match (the ref-snapshot theory in the old TODO
    was a partial misread).** Examining the hole (`dk_Hul2`, holding the hook
    `dk_Krog` *inside*): `dk_ListOnlyFi` (`.Contents`, Inside) fires legitimately,
    renders `Inde i hullet er krogen.`, and Scarier's `replace_alrs` chain-blanks it
    to `""` via the author's `dk_IndeIHulle`→`dk_IndeIHulle1` TextOverrides —
    **identical to FD, object2 stays bound, no snapshot needed.** The garbage `Inde
    i hullet er 0.` came from a *sibling*, **`dk_ListFirstL1`**, gated on
    `%object%.Children(Characters, In).Count>0` (the *characters* inside the hole =
    none). Before fix (b) `.Children(Characters, In)` wrongly returned the *object*
    `dk_Krog` → `.Count=1` → the gate passed → the task ran with `%character%`
    unbound → `0`. With fix (b) the character child-set is correctly empty, the gate
    fails, and the line never renders. See
    **`TODO_a5_aggregateoutput_suppression.md`** for the full instrumented trace
    (the author-ALR mechanism is real and correctly documented there; only the
    "which sub-task + object2-snapshot" attribution was off).

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

## Alien Diver (Daza, ADRIFT 5) — WON (repair ship + take off)

`test/AlienDiver_walkthrough.txt` (85 commands) reaches `*** You have won ***`
in **both** Scarier and FrankenDrift (`FD_RNG=xoshiro`) — the ship repairs and
takes off. Deterministic under Scarier's seed-1234 mode.

The game was **100% unwinnable** in Scarier before four engine fixes (commit
"make Alien Diver's core mechanics work"): the cube dice → blank-card →
craft-card → extract-fragment loop never fired, and the crashed ship (which is
placed in one *random* ocean room) landed nowhere. Root causes were all the
"apply a SelectionOnly marker property as a reference" pattern
(`%X.Objects.ObjectIsAT%` → the cube, `BlankCards.ObjectIsAC` → the blank cards)
plus `MoveObject ToLocationGroup` on a *dynamic* object needing one random room.

**Route (seed 1234):** you awake at a random room — **Ocean M005 (Loc33)** — and
the ship is at **Ocean M053 (Loc17)**. Get one fragment of each colour by the
core loop (roll to unlock a cube → `ec` blank card → `cc` craft on a colour cube
→ `pc` play on a *same colour+power* cube), refilling oxygen at the ship
(`enter ship`/`exit ship`) and **avoiding the four Rip rooms** (99-102) whose
currents sweep you off course. Then `enter ship`, `fix ship`, go to the cockpit,
`sit on seat`, `take off`.

**Wired into the strict suite at MATCH 0|0** (`AlienDiver|AlienDiver.blorb|0|0`).
Reaching a byte-exact `FD_RNG=xoshiro` diff took a chain of engine fixes (BUG 1
fragment over-count, BUG 2 save/restore round-trip, BUG 3 crafting-bind, the
`<#…#>` status expression, the EndGameText `<cls>` floor, and finally BUG 4
seen-timing + the v5 empty-room listing grammar; the derivation doc
`TODO_aliendiver_divergences.md` was deleted 2026-07-11 once done — see git
history for the full analysis):

* **BUG 4 — move-time seen-marking.** FD's `clsCharacter.Move` marks the arrival
  location, its objects and its visible characters seen the *moment* the player
  moves (before the room render and before the movement's `AfterTextAndActions`
  overrides), so a same-turn `Must HaveBeenSeenByCharacter %Player%` gate already
  sees the arrival — e.g. walking onto the crashed ship immediately satisfies
  `ResetRollC → CheckIfPla1` and sets `Shipfound`. Scarier only refreshed at turn
  boundaries, lagging the ship-location status by three commands and blanking the
  opening cube-status block. Fix: `mark_player_arrival_seen` in the MoveCharacter
  action handler (a5run_action.cpp).
* **v5 empty-room listing grammar.** A v5 room whose body (long description +
  special-listed objects) is empty lists its general objects as `There is X here.`,
  not the trailing `Also here is X.` (clsLocation.vb:132-139). Fix in
  view_location_impl (a5text.cpp).

Whole a5 golden suite stays green in both RNG modes + save/restore. The vanilla
column is a Scarier self-golden (`AlienDiver_expected.txt`) because vanilla FD
(System.Random) lands the ship in a different room; the game is only FD-diffable
under `FD_RNG=xoshiro`. Verify by direct replay:
`./test/a5run_dump test/adrift5-games/AlienDiver.blorb test/AlienDiver_walkthrough.txt | grep "Well Done"`.


## All Through the Night (Daniel Saults, 2013) — Ending C: Salvation, MATCH 0|0

A short Bogleech-jam horror piece: a burrowing-thing "intruder" lays siege to
your house over a fixed ~57-turn timeline and you pick one of four endings
(A: Intrusion = swarmed/death, B: Abdication = flee out front/back yard,
C: Salvation = call 911 and survive behind a sealed house, D: Retribution =
topple the wobbly entertainment centre onto it in the living room). No scoring;
the "win" is a good ending. The walkthrough plays **C: Salvation**, the designed
hero route, because it exercises the most engine surface (garage inventory,
lock/barricade/board verbs, the whole event scheduler, and the police countdown).

**Route & mechanics (no engine change — clean MATCH 0|0 both RNG modes on first
try).** The intrusion runs on an absolute turn clock (TV-news event → `NewsOver`
→ `IntruderPr`/`InitiateIn` sets `Intruderal`≥1 at ~turn 30 → the 57-turn
`Intrusion` event). Nothing may be barricaded/boarded/called before the siege
(`Intruderal`≥1), but **locking works immediately**. The intruder's entry
sub-events each check one seal:

* `IntruderOp`/`IntruderOp1` (front/back door *open & walk in*, ~turn 33/43) are
  gated only by `Frontdoorl`/`Backdoorlo` — i.e. **the door being LOCKED**, not
  barricaded. Locking one door but only barricading the other lets the intruder
  in the unlocked side (early bug in derivation: back door barricaded-but-unlocked
  → intruder enters the basement, wanders up, swarms you = Ending A).
* `emIntruderBr..Br3` (windows → basement/bedroom/spare/living) are stopped by
  **boarding** `Window2`/`Window1`/`Window`/`Window3`. The kitchen (`Window4`) and
  bathroom (`emWindow`) windows are never used — only those four matter, and the
  `Boards` object covers exactly four windows.
* `emIntruderBr4`/`Br5` (late door *breach*) are stopped by **barricading** Door1
  (couch/recliner) and Door5 (futon).

So the full seal is **lock + barricade both doors AND board all four windows**.
Call 911 any time after the siege starts (`Call` SetTasks-executes `emCavalry`,
which only fires once `emIntrusionF` completes near the end of the `Intrusion`
event → `emCavalryCou` 3-turn countdown → Ending C). Then simply outlast the
battering: 45 waits land the police on the exact turn the ending fires. Golden
`AllThroughTheNight_expected.txt`, wired `AllThroughTheNight|...|0|0`.

## An Adventurer's Backyard (Nick Gauthier, 2015) — WON 25/25 MAX, MATCH 0|0

A minimal treasure-hunt (22 rooms, no NPCs/deaths/timers). All 25 points come
from ten scored deeds: chop the treehouse floor with the axe (+2) and take the
locket in the hollow beneath (+3); fish the coin out of the fountain (+3); bait
the kitchen fly with the sugar+flypaper (+2, flypaper is hidden under the porch
welcome mat, sugar in the lower kitchen cabinet's bowl); feed that fly to the
hallway spider so it leaves the rose (+1) and only THEN lift the diamond ring
from the vase (+3 — taking it while the spider guards the rose just backs you
away); open the envelope hidden in the bathroom toilet tank (+3); take the
collar off the cat on the master-bed (+3); set the stepladder against the roof
from the balcony (+2) and grab the meteorite on the roof (+3); then `score`.

**Surfaced + fixed a real Scarier conformance bug — carry-capacity was never
enforced.** The ADRIFT library Take/Put tasks carry the bulk/weight limit as a
Property restriction with an *arithmetic-expression* RHS, e.g.
`MaxBulk %Player% Must GreaterThanOrEqualTo %Player%.Held.Size.Sum+%objects%.Size.Sum`.
`a5restr.cpp pass_property` evaluated integer-RHS inequalities but fell through
to a lenient always-pass for any non-integer RHS, so Scarier let the player
over-carry where FrankenDrift refuses ("The stepladder is too bulky to carry at
the moment."). The stepladder is bulk 81 of the player's 90 limit, so the real
Runner will not pick it up unless you first drop the axe and the early
treasures — the walkthrough does exactly that, and without the fix Scarier
"won" a route the authentic Runner stalls at 20/25.

Fix: `pass_property`'s numeric-inequality branch now evaluates a `%reference%`
RHS through `a5text_eval_expression` (which already handles `.Held`/`.Size.Sum`
aggregation) and compares numerically. It is deliberately gated on
`is_clean_int(value) || strchr(value,'%')` so a bare non-integer state word
(PathwayToDestruction has an oddly-authored `OpenStatus Obj Must LessThan Open`
that must keep the lenient pass) is left untouched — narrowing that guard was
needed to avoid regressing PathwayToDestruction's metal-door move. Full suite
stays green (43 MATCH incl. this game + 8 pre-existing DIVERGE, 0 regressions).
Golden `AnAdventurersBackyard_expected.txt`, wired `AnAdventurersBackyard|...|0|0`.

## Return to Castle Coris (Larry Horsfield, 2020) — WON 400/400 MAX, MATCH 0|0

Alaric Blackmoon episode (Version 5.0000366; 1874 objects, 1512 tasks).
The game ships its own solution as the `Walkthroug` task (`wlkthrgh` /
`WALKTHROUGH`), a single ~430-command line ending "...Adventure Complete!".
Extracted verbatim, it wins in neither FrankenDrift nor Scarier as-written —
the two engines derail *identically* at each point, confirming Scarier tracks
FD faithfully; the repairs are all against the built version's map/mechanics:

* **Start menu.** The printed route begins at the tunnel entrance; prepend `o`
  (type O → Start Options page) then `b` (Begin the game), the same intro
  pattern as the Spectre/Axe Horsfield games.
* **Slime-eater sack (+5+5).** "u - u - search tunnel - d - d - get eaters"
  over-shoots by one level: the oilskin sack (task `SearchEart`) is found by
  searching the **Rock Tunnel** (Location43, one `u` above the waterfall cave),
  not the Earth Tunnel two `u` up. Corrected to "u - search tunnel - d - get
  eaters"; with the sack held, `get eaters` (`GetSlimeEa`) auto-catches them.
* **Gold ring vs. the alehouse.** The doc wears the gold ring from the cave
  nest all the way to Christiana, but entering the main alehouse (Location93)
  with any gold *worn or directly held* fires the System `LocationTrigger`
  death `CarryingGo2` (`Bracelet|Necklace|Ring worn`, unless shrunk) — the men
  "see the gold ... and they attack you", losing at 300/400. The other gold
  (bracelet/necklace/teeth) is already bagged; only the ring is worn, so
  `remove ring` / `put ring in bag` before the first entry survives it, and at
  Christiana the doc's `remove ring` becomes `get ring` (from the bag) before
  `give ring`.
* **Flambeau's green door.** The doc's single `s` from the East Gate stops at
  the north-end RED-door house; Flambeau lives behind the GREEN door halfway
  along South Lane (Location91), one more `s`, where `pick lock` (needs the
  wire) works (+5).

Result: "...in 437 turns, scoring the maximum 400 points!" in both engines.

**Ex-residual 0|1, root-caused 2026-07-21 → 0|0.** The one xoshiro hunk was the
first `look in gap` under the outcrop: task `LookInGap`'s completion has a
second alternate ("You can also see `Location66.Objects.DynamicLocation.List`
under there.", `DisplayWhen=StartAfterDefaultDescription`) gated on `AnyObject
Must BeAtLocation Location66`. Instrumenting FD showed the *restriction* is NOT
the divergence — both engines pass it (Location66's 7 statics count for
AnyObject/`AllObjects`). The divergence was the OO property `.Objects`: the
runner's ReplaceOOProperty "Objects" (Global.vb:913/1504) calls
`loc.ObjectsInLocation` with DEFAULT arguments — `AllListedObjects`, directly —
i.e. dynamic objects unless ExplicitlyExclude plus statics only when
ExplicitlyList, NOT the restriction evaluator's `AllObjects`. Under Outcrop's
statics are un-listed, so in FD the list is EMPTY → `.List` renders "nothing" →
"You can also see nothing under there." → which the game's own `YouCanAlso` ALR
(OldText exactly that sentence, empty NewText) erases whole, leaving just the
base sentence. Scarier's `objs_in_location` (a5expr.cpp) returned ALL objects,
so the sentence survived with the scenery list. Fixed by applying the
listed-objects filter there (sole consumer is OO `.Objects`); full suite stays
green and the game is now byte-identical to FD. Vanilla previously also showed
5 pure-RNG hunks (salt-flats vulture/eagle/lizard atmospheric draws under .NET
System.Random), gone under the golden compare. Wired
`ReturnToCastleCoris|ReturnToCastleCoris.blorb|0|0`, golden re-blessed.

## Edith's Cats (Bunkphor, 2016) — ★ WON, MATCH 0|0

EctoComp 2016 "La Petite Mort" (3-hour jam) horror vignette; player "Robi", a
schizophrenic blogger in a thinly-veiled Orbán-era Hungary. 3 rooms, one Neutral
ending, no score. Winning route (6 turns, byte-identical to FrankenDrift):
`kiss Edith` (unlocks Rehab→east) → `east` → `wait` (bus arrives, unlocks
Bus stop→south) → `south` → `fuck Edith` (cats mutate, doorbell) → `wait`
(Wait1 → `EndGame Neutral`, "The End"). All three room exits are task-gated, so
the route is forced. Only custom tasks: KissEdith (Specific override of
`kiss %character%` for Edith), Wait, Fuck, KissEdith1 (Specific override of
Fuck), Wait1.

**Engine fix — On/InCharacter carrier location + seen.** Edith starts with
CharacterLocation **"On Character"** / CharOnWho=Player (she rides the player
piggyback). Scarier left On/InCharacter's `char_loc` NULL and treated it as
not-at-any-location, so Edith was neither present nor "seen": `kiss Edith` failed
the `HaveSeenCharacter` restriction ("Sorry, I'm not sure which character you are
referring to.") and `x edith` said "You see no such thing.", leaving the game
unwinnable. FrankenDrift derives clsCharacterLocation.LocationKey from the
carrier, so Edith is present and seen from turn 1. Added `char_onchar[]`
(carrier CHARACTER key, parsed from CharOnWho / CharInsideWho) and resolved it
through the carrier in `a5state_character_location_key` (recursive, depth-32
guard), `a5state_character_at_location`, and
`a5state_character_visible_at_location`. Purely additive — every new branch is
gated on `char_onchar[ci]!=NULL`, set only for On/InCharacter characters, so all
other games are unaffected (full suite stays green). Golden
`EdithsCats_expected.txt`, wired `EdithsCats|edithscats.taf|0|0`.
