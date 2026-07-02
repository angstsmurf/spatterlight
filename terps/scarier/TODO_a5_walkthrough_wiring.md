# TODO: wire remaining walkthroughs into the a5 regression harness

`test/run_a5_walkthroughs.sh` only exercises a game once it has (1) a game file
in `test/adrift5-games/`, (2) a turn-by-turn command script
`test/<Name>_walkthrough.txt`, and (3) a `name|game|vbudget|xbudget` line in the
script's `MAP`. `test/adrift5-games/walkthroughs/` already has raw
walkthrough/hint material for several games whose game files are staged but
that never got a command script or a MAP line. This is the backlog.

## Ready to wire (script + game file already staged — just needs a MAP line)

- ~~**PathwayToDestruction**~~ ✅ **WIRED (2026-07-02).** Full MATCH in both RNG
  modes (`0|0` MAP line, golden `test/PathwayToDestruction_expected.txt`). Surfaced
  and fixed the `<cls>` commit-boundary bug — see the top entry in
  `TODO_a5_walkthrough_bugs.md` ("Pathway to Destruction: `<cls>` … DONE").

## ⭐ Most Larry Horsfield games ship a built-in WALKTHROUGH (the "native" solution)

Discovered 2026-07-02: nearly every Horsfield game prints its **entire solution**
in-game. Two flavours: a start-menu `WALKTHROUGH` (Bug Hunt lists it; TBN has it
unadvertised) OR a `WLKTHRGH` command whose text is a `cl_Walkthroug` task in the
model. Either way the raw commands can be pulled straight out of `test/a5dump`
output (grep for the `<Command>[wlkthrgh]/WALKTHROUGH` task's `<Text>`), stripped
of parenthetical annotations, and replayed with only the start-menu handshake
prepended (`o` then `b`; Bug Hunt also needs a vision-impairment `n` and a
timed-event `y`). No external walkthrough needed. **Native-solution audit
(2026-07-02):**

| game | built-in? | native solution status |
|---|---|---|
| ThingsThatGoBumpInTheNight | WALKTHROUGH | ✅ **WIRED** (8\|8) — 3 cut-scene corrections |
| **LostLabyrinthOfLazaitch** | WLKTHRGH | ✅ **WIRED + FIXED 2026-07-02** (403\|403 → **8\|0, xoshiro FULL MATCH**) — full 520-pt win, ZERO corrections (see below) |
| BugHuntOnMenelaus | WALKTHROUGH | ✅ **WIRED + FULL WIN 2026-07-02** (`0\|23`) — Scarier wins 100/100 where FD can't; see below |
| DwarfOfDirewoodForest | WLKTHRGH | ✅ **WIRED + FIXED 2026-07-02** (0\|0 conformance MATCH — but FD-trapped, NOT a win; see below) |
| TheEuripidesEnigma | WLKTHRGH | ✅ **WIRED + FIXED 2026-07-02** (11\|11 DIVERGE, RNG-independent) — full 400-pt win; the `4` desync was just a downstream artefact of ONE spurious `hit fork on face` (see below) |
| FinnsBigAdventure (FBA v.3c) | ❌ **vestigial WT** | **NO built-in walkthrough (verified 2026-07-02).** The HELP/VOCAB text advertises "WALKTHROUGH (WT)" but **no task backs that command** — `a5dump` has no `cl_Walkthroug*` printer task (only an unrelated `cl_Walkthroug5` location-trigger System event) and no `[wt]`/`[walkthrough]` Command anywhere; typing `wt` in-game just loops the intro handshake. The same intro says "a walkthrough is available on request (type HELP)" → **email-only**. Blind-play like Magor/Xanix (see hints-only group below). |
| MagorInvestigates / XanixXixonResurgence | none | only the email-on-request note |

Caveat: the built-in text was authored against a slightly earlier build, so some
moves get absorbed by this build's scripted cut-scenes and must be corrected
against FrankenDrift (intro auto-walks and patrol/teleport cut-scenes are the
usual culprits). LostLabyrinth is the exception — it replayed byte-clean.

### ⭐ BugHuntOnMenelaus — WIRED as a FULL WIN; Scarier SURPASSES FrankenDrift

**WIRED 2026-07-02 (`0|23`, DIVERGE) — Scarier plays the entire game to
`*** CONGRATULATIONS! *** …the maximum 100 points!` (all 6 Meneltra, 69 turns),
which FrankenDrift (our ground truth) CANNOT.** Native WALKTHROUGH extracted (6
marines, `BECOME` switching), two transcription fixes for this build: Captain
Erlin enters the roadside cottage via **`In`** (the built-in's `E`/`S` are a no-op
loop + a dead end here), and the final move is **`n`** to the shuttle (the
built-in's `w` was for an earlier build — the auto rendezvous already returns the
player to the town centre as Erlin). Golden = Scarier's own winning transcript
(`test/BugHuntOnMenelaus_expected.txt`); the xoshiro column carries the FD
differential (23, RNG-independent) as the *documented FD gap*.

**Golden cleanup (later 2026-07-02): a double-render "By Shuttle" bug fixed.**
The disembark cut-scene runs through `cl_ZMovePlaye`, which `Execute Look` twice
in one command — once after the player moves and again after the marine squad
follows — so Scarier emitted the "By Shuttle" room view twice per shuttle move
(FD renders it once, keying stock Look's AggregateOutput on its response
template). Mirrored the non-resp path's `pass_seen` dedup in `resp_flush`
(`look_seen` set: collapse identical deferred `render_look_string` views within
one command). Golden re-trimmed to the single-view output; all 22 other goldens
stay byte-identical, FD gap unchanged at 23.

The TODO had this catalogued as "FD-blocked … Scarier is worse here (BECOME
doesn't relocate the player)". Both halves turned out to be one root cause:

- **The real Scarier bug was BECOME, not movement.** `BECOME <character>` is a
  `MoveCharacter Character %Player% ToSwitchWith <char>` action. Scarier's
  MoveCharacter had no `ToSwitchWith` case, so every BECOME **silently no-op'd** —
  the viewpoint stayed in Captain Erlin's cottage basement and every later marine
  command failed against the wrong room. Once BECOME actually switches the player
  (see the fix below), Scarier's *ordinary* pass-gated movement handled Davey's
  corridor fine — it was never an OR-restriction bug in Scarier; the marine just
  never got there. (The old `push 3`→"can't see any 3s" note was the same
  downstream fallout: Davey was stuck in the basement, not in the elevator.)
- **FrankenDrift is the one that can't finish.** Lance-Corporal Davey reaches the
  3rd-floor office Meneltra through a pass-gated corridor whose `<Movement>`
  carries an OR restriction (`cl_Pass BeHeldByCharacter … O … BeWornByCharacter …`,
  BracketSequence `(#O#)`). In FD, once Davey *holds* the pass (restriction
  passes), `n`/`s` return "Sorry, I didn't understand that command." — the
  movement is dropped, so FD caps at 4/6 kills, 65/100, no win. The real ADRIFT
  Runner (per the author's own walkthrough) walks it, and so does Scarier. Hence
  no FD MATCH is possible; this is a divergence *in the correct direction*.

The fix (full write-up in `TODO_a5_walkthrough_bugs.md`): a dynamic current-player
key (`a5_state_t.player_key`, mirroring `clsAdventure.Player`). `ToSwitchWith`
retargets it when the player is involved (don't move anyone — change *which*
character is the player, FD `clsUserSession.vb`); the old player stays put as an
NPC. Every `%Player%` / player-scope resolution now reads `a5state_player_key`
instead of the hard-coded literal `"Player"`, and `char_perspective` keys 2nd
person on "is the current player" so a switched-in marine narrates as "you …", not
by name. All 20 golden games stay byte-identical (player_key is `"Player"` until a
BECOME, and no other corpus game uses BECOME) — zero regressions.
- ~~**DwarfOfDirewoodForest**~~ ✅ **WIRED 2026-07-02 as a 0|0 conformance MATCH
  (both RNG modes, golden `test/DwarfOfDirewoodForest_expected.txt`) — but NOT a
  win.** Native WLKTHRGH extracted (single protagonist). One transcription fix:
  at "By Guard Room" the built-in's `s` must be **`creep south`**
  (`cl_CreepSouth`, `[creep/tiptoe] [s/south]`) — a plain `s` alerts the guard.
  The **hide-in-beard root cause is FIXED** (see the top entry in
  `TODO_a5_walkthrough_bugs.md`): the plural `%objects%` per-item binds were
  clobbering the singular `ReferencedObject` alias (the beard container), so
  `hide droppings, knife and key in beard` failed the general task's
  `cl_CanBeHidde` restriction against the *items* and the beard-specific
  override never fired. With that fixed (and `creep south` parsing fine —
  the earlier "Scarier doesn't parse it" note was just downstream fallout),
  Scarier now matches FD **byte-for-byte over the full 236-command native
  script**, including the trap: FD still cannot leave "By Guard Room" on the
  return visit — the `{*}` catch-all (`cl_NullAtStar`, priority 44074,
  PreventOverriding=1) preempts the room's ordinary North `<Movement>`, which
  the real ADRIFT runner evaluates first — so both engines answer the last ~166
  commands with "Please press O then press Enter." identically. The pre-trap
  first act (cell escape, guard kill/loot/drag/lock) is real gameplay coverage.
  If the movement-before-`{*}` precedence is ever adopted (in FD or Scarier),
  the golden must be re-derived and the same script should then reach the win.

BugHunt (see the ⭐ section above) is now the resolved case of this class: it
diverges from FD *in the correct direction* (Scarier wins, FD stops), so it is
wired against Scarier's own winning golden with the FD gap (23) carried in the
xoshiro column rather than as a MATCH. (Dwarf's `{*}`-vs-movement trap is the
same family but there FD and Scarier agree byte-for-byte, so it IS a MATCH.)

### ⭐ LostLabyrinthOfLazaitch — native solution wired with zero corrections; FIXED to 8|0 (xoshiro FULL MATCH)

Extracted straight from the `cl_Walkthroug` task, `o`/`b` prepended, annotations
stripped — nothing else. FrankenDrift replays the raw author commands to
`*** CONGRATULATIONS! *** …in 451 turns, scoring a total of 520 points!` with no
press-O / not-understood / can't-see lines, so it is a faithful native solution and
all 403 original hunks were Scarier bugs. **All fixed same day (403|403 → 8|0):**
location seen-tracking (`loc_seen`, the "Fahren Layburn" teleport), the
`ReferencedObjects`-from-singular restriction alias (`sheath sword`), CharHereDesc
name-casing ("White Stallion"), and FD's Look-message double-test-render dance
(the riding `<# OneOf #>` draw stream). The residual vanilla 8 are pure
System.Random-vs-xoshiro riding-variant picks (RNG-bound, like JacarandaJim /
SixSilverBullets), so no vanilla golden. See `TODO_a5_walkthrough_bugs.md` (top
entry, DONE) for the full write-up.

### ⭐ TheEuripidesEnigma — native solution wired (11|11 DIVERGE); the `4` was a red herring + an exponential ALR-recursion hang fixed

Extracted straight from the `cl_Walkthroug1` task, `o`/`b` prepended, annotations
stripped. **TWO corrections** (both verified against FrankenDrift, which now
replays the raw author commands to `*** CONGRATULATIONS! *** …in 426 turns,
scoring the maximum 400 points!` with ZERO not-understood / no-route / ambiguity
lines):

1. **Dropped `hit fork on face` at "By Cliff In Canyon".** In this build the
   round opening leading North is already resonated open, so striking the fork
   there *closes* it ("the round hole disappears, leaving a smooth cliff face")
   and every subsequent move fails ("no route to the north, only south and
   southwest"). This ONE spurious line desynced the entire back half — the `4`
   numeric-conversation-choice failure the TODO flagged was merely a downstream
   symptom of being stuck in the canyon, not the root cause.
2. **`put pistol in pistol holster` → `holster pistol`.** The raw phrasing is
   ambiguous in this build ("pistol" matches both the pistol and the pistol
   holster) → FD "Sorry, I'm not sure which object you're referring to."; the
   game's own `holster pistol` verb (used later in the same script) holsters it
   cleanly.

**Surfaced + fixed a Scarier hang** (see `TODO_a5_walkthrough_bugs.md` top entry):
the corrected script drove Scarier far enough to hit the north-end storeroom,
where it wedged on ~25 **identity ALRs** (a 25-asterisk banner line whose
`OldText` == `NewText`). Scarier's `replace_alrs` recursed `process_inner` on the
replacement *before* its self-reference guard, branching ~25-fold per level to
the depth-8 cap (~25⁸ calls). Ported FD's `If sText = sALR Then Exit For`
(Global.vb:532) guard — bail before recursing when the whole text equals the
(unprocessed) replacement. Scarier now finishes the full 400-pt win in ~1.8 s.

The residual **11 hunks** (identical in both RNG modes ⇒ RNG-independent real
bugs, no vanilla golden while it diverges) are three families of object-listing /
render bugs: (a) a dropped object that the room's long description already names
by prose ("boom box"/"small drone") is *also* appended to the "Also here is …"
auto-list; (b) a one-off doubled cliff-face description; (c) one movement-message
wording variant. Catalogued in `TODO_a5_walkthrough_bugs.md`.

## Have a full walkthrough, need conversion to a command script

- ~~**CallOfTheShaman**~~ ✅ **WIRED (2026-07-02).** Full 265-point win
  (`*** CONGRATULATIONS! ***`) in both RNG modes; `3|3` MAP line, DIVERGE.
  The source file is a ROT1-encoded *hint sheet* (not a turn-by-turn
  walkthrough), so — like StoneOfWisdom — the win script
  (`test/CallOfTheShaman_walkthrough.txt`) was derived by actually playing the
  game to a win using the decoded hints as a route guide. The 3 residual hunks
  are RNG-independent endgame-banner bugs (BUG 15: `%Turns%` not substituted +
  leading-cap URL mangling) — see `TODO_a5_walkthrough_bugs.md` /
  `A5_WALKTHROUGH_FINDINGS.md`. No golden committed while it diverges.
- ❌ **LostCoastlines** → `Lost_Coastlines.taf` — **NOT a walkthrough.** The
  `walkthroughs/LostCoastlines_walkthrough.pdf` (8.6 MB) is an image-only PDF
  containing a hand-drawn **nautical map feelie** (green coastlines + blue
  sailing-route dashes, plus decorative bordered blank pages) — no text layer, no
  commands. And Lost Coastlines is a procedural/random sailing-exploration game
  that could not byte-align to FrankenDrift anyway. Would need a real walkthrough
  (none found: not on CASA, not public) *and* xoshiro-only comparison. Deprioritise.
- ❌ **Skybreak** → `Skybreak.taf` — **NOT a walkthrough.** The
  `walkthroughs/Skybreak_walkthrough.pdf` (16 pp) is the in-game **manual/lore**
  (cosmography, skill list, combat formulae), not a command sequence. Skybreak is
  a class-based RPG with randomised combat and multiple win conditions — hard to
  wire deterministically even with a real walkthrough (none found).

## Have hints only (not a full walkthrough) — treat with the StoneOfWisdom caution

The `*_hints.txt` files here are just **intfiction.org forum-thread fragments**
(a single stuck puzzle each), NOT walkthroughs — Larry Horsfield only sends full
walkthroughs by email, and none are on CASA/public (verified 2026-07-02).
StoneOfWisdom's first script was a hint-sheet reconstruction with no real
navigation that tested nothing; assume the same risk here. The unlock for two of
these is the **built-in `WALKTHROUGH` command** (see the ⭐ section above) — that
gives a real solution to correct against FD, no blind play needed.

- ~~**ThingsThatGoBumpInTheNight**~~ ✅ **WIRED (2026-07-02).** DIVERGE 8|8 (see
  the harness MAP + `A5_WALKTHROUGH_FINDINGS.md`). Full max-score win
  (`*** CONGRATULATIONS! ***`, 250/250, 310 turns) under FD, derived from the
  game's built-in `WALKTHROUGH` with three cut-scene corrections (spurious intro
  `n` dropped; `snuffio`+`s` added so the guard patrol carries you to the Treasury
  door in the dark — the exact intfiction.org/t/77639 sticking point). The 8 hunks
  (identical both RNG modes) are a real Scarier bug: `drop all` over-expands the
  bare `all` to every seen object (incl. worn/scenery/the location object) and
  kills the player at the ravine, plus one dark-room `get dirt` line. Catalogued
  in `TODO_a5_walkthrough_bugs.md` (OPEN) — fixing it likely takes TBN to a near
  MATCH.
- ~~**BugHuntOnMenelaus**~~ ✅ **WIRED 2026-07-02 as a FULL WIN (`0|23`).** Scarier
  now plays it to `*** CONGRATULATIONS! ***` (100/100) via the built-in
  `WALKTHROUGH` once BECOME player-switching was implemented — and wins where FD
  is blocked at Davey's pass-gated corridor. See the ⭐ section above.
- **FinnsBigAdventure** → `FBA v.3c.blorb` — no built-in `WALKTHROUGH`; hints
  fragment only (`walkthroughs/FinnsBigAdventure_hints.txt`). Needs a full
  play-to-win. The forum thread does sketch a usable opening (move stool → mantel
  items; copy the spell from the notepad onto the paper; find the rucksack for
  carrying; keep the telescope, tinderbox, both toy-soldier sets and the warbelt
  or you lock yourself out of the win).
- **MagorInvestigates** → `MI_v.1.blorb` — no built-in `WALKTHROUGH` (start-menu
  commands are HANDFIRE/INSTRUCTIONS/HELP/NAVIGATION/TASKS/VOCAB); hints fragment
  is one puzzle (make herbal tea — the kettle is in the mug's room). ~200-move
  wizard puzzle game; needs a full blind play-to-win.
- **XanixXixonResurgence** → `XXR v.4.blorb` — no built-in `WALKTHROUGH` (the
  author removed the in-game one per intfiction.org/t/63142); hints fragment only.
  **An earlier build with the built-in walkthrough is NOT publicly obtainable
  (checked 2026-07-02):** itch.io, adrift.co and the ParserComp 2023 entry all
  serve the identical v4 (SHA1 `d11f0b5a140a6d982c518bb77e5458986c76b0e7`, 996300
  bytes — adrift.co download is byte-for-byte our local copy); it is not on the IF
  Archive; CASA (game id 10450) has no solution uploaded; and the earliest Wayback
  snapshot (2023-09-10) already lists `XXR v.4.blorb`. The author updated in place,
  so v1–v3 aren't hosted anywhere. Only route to a walkthrough is emailing the
  author (in-game `HELP` gives the address). Even then, weak candidate: the endgame
  giant-hybrid fight is **randomised** ("keep hitting it with the axe") → won't
  vanilla-align; xoshiro-only at best.

## Not in scope here: no walkthrough material at all yet

These game files are staged in `test/adrift5-games/` but there's no
walkthrough or hints for them anywhere in the corpus — finding/writing one is
a separate task from "wiring up," so they're not listed above:
Halloween, MuseumHeist, October31st, TheFortressOfFear, Tingalan.
(TheFortressOfFear is Horsfield but has **no** built-in `WLKTHRGH`/`WALKTHROUGH`
task; the others are by Finn Rosenløv.)

**⚠️ Recheck each of these for a built-in `WALKTHROUGH`/`WLKTHRGH` task before
assuming "no material"** — that is exactly how Tribute (below) was missed. Grep
the `a5dump` output for `walkthrou`/`wlkthrgh` and confirm it's backed by a real
`<Command>[walkthrough…]` printer task (a `<Text>` full of `> cmd` lines), not a
vestigial HELP mention like FinnsBigAdventure's dead `cl_Walkthroug5` event.

### ⭐ Tribute — a built-in `WALKTHROUGH` was hiding here; wired as a FULL WIN (2\|2)

**WIRED 2026-07-02** (`2|2`, DIVERGE). Tribute ("Return to the City of Secrets,"
Kenneth Pedersen, based on Emily Short's *City of Secrets*) was mis-filed under
"no walkthrough material" but ships a real built-in `WALKTHROUGH` command (a
`Walkthroug` task, `[walkthrough/walkthru/walk through/walk thru]`, `<Text>` a
143-line `> cmd` solution). Extracted verbatim with **zero corrections** — no
start-menu handshake (the intro "Press a key to continue" is auto-advanced by
the harness), no cut-scene fixes. Scarier plays it straight to
`*** You have won ***`, 100/100, 143 turns. The 2 RNG-independent hunks are a
cosmetic override-of-`TakeObjectsFromOthers` blank-line difference on two gem
grabs; catalogued in `TODO_a5_walkthrough_bugs.md` (OPEN, not chased — see the
regression-risk note there). No golden while it diverges.

`DwarfOfDirewoodForest`, `TheEuripidesEnigma`, `TheLostLabyrinthOfLazaitch` and
`Tribute` were moved OUT of this list on 2026-07-02 — they embed a built-in
`WLKTHRGH`/`WALKTHROUGH` native solution (see the ⭐ sections; LostLabyrinth,
TheEuripidesEnigma and Tribute are wired as full wins, DwarfOfDirewoodForest as
a 0|0 conformance MATCH that ends FD-trapped at "By Guard Room").

## Wiring checklist per game

1. Derive/convert a full winning (or intentionally-losing, if that's the
   only path) command script into `test/<Name>_walkthrough.txt`.
2. `test/a5_groundtruth.sh test/adrift5-games/<file> test/<Name>_walkthrough.txt`
   in both vanilla and `FD_RNG=xoshiro` modes; count the diff hunks.
3. Add a `name|game|vbudget|xbudget` line to the `MAP` heredoc in
   `run_a5_walkthroughs.sh`, using the observed hunk counts as the initial
   budget (0|0 if it's a clean MATCH).
4. If the vanilla transcript is stable, save it as
   `test/<Name>_expected.txt` so the harness can use the fast golden-diff
   path instead of shelling out to dotnet/FrankenDrift every run.
5. If it diverges at baseline, log the root cause in
   `A5_WALKTHROUGH_FINDINGS.md` / `TODO_a5_walkthrough_bugs.md` rather than
   just carrying a budget number with no explanation.
