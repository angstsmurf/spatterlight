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
| BugHuntOnMenelaus | WALKTHROUGH | ⚠️ derived + corrected but **FD-blocked** (see below) |
| DwarfOfDirewoodForest | WLKTHRGH | ⚠️ derived + corrected but **FD-blocked** (see below) |
| TheEuripidesEnigma | WLKTHRGH | ✅ **WIRED + FIXED 2026-07-02** (11\|11 DIVERGE, RNG-independent) — full 400-pt win; the `4` desync was just a downstream artefact of ONE spurious `hit fork on face` (see below) |
| FBA v.3c | WT | not yet extracted (has a `WALKTHROUGH (WT)` command) |
| MagorInvestigates / XanixXixonResurgence / FinnsBigAdventure | none | only the email-on-request note |

Caveat: the built-in text was authored against a slightly earlier build, so some
moves get absorbed by this build's scripted cut-scenes and must be corrected
against FrankenDrift (intro auto-walks and patrol/teleport cut-scenes are the
usual culprits). LostLabyrinth is the exception — it replayed byte-clean.

### ⚠️ Native-solution derivation: two Horsfield games are blocked in FrankenDrift itself

Both games' native walkthroughs were successfully extracted and the mis-transcribed
moves corrected, but they cannot be wired because **FrankenDrift (our ground truth)
cannot execute a key movement** — the real ADRIFT runner can (per the author's own
walkthrough), so this is an FD reference-engine limitation, not a wrong command:

- **BugHuntOnMenelaus.** Native WALKTHROUGH extracted (6 marines, `BECOME`
  switching). Only transcription fix needed for this build: Captain Erlin enters
  the roadside cottage via **`In`** (the built-in's `E`/`S` are a no-op loop + a
  dead end here). With that, Erlin's basement Meneltra kill and the other four
  marines (Boone dumpster, Zen toilet, Jones zampf-2, Foley tunnel) all succeed.
  **Blocker:** Lance-Corporal Davey must reach the 3rd-floor office Meneltra
  through a pass-gated corridor whose `<Movement>` carries an OR restriction
  (`cl_Pass BeHeldByCharacter … O … BeWornByCharacter …`, BracketSequence `(#O#)`).
  In FD the restriction message ("You need a visitor's pass …") shows **only when
  the pass is ABSENT**; once Davey *holds* the pass (restriction passes) `n`/`s`
  return "Sorry, I didn't understand that command." on every floor — the movement
  is dropped. So Davey can't kill his Meneltra; FD caps at 4/6 kills, 65/100, no
  win. (Scarier is worse here: `BECOME` doesn't relocate the player and `push 3`
  → "can't see any 3s".)
- **DwarfOfDirewoodForest.** Native WLKTHRGH extracted (single protagonist). One
  transcription fix: at "By Guard Room" the built-in's `s` must be **`creep south`**
  (`cl_CreepSouth`, `[creep/tiptoe] [s/south]`) — a plain `s` alerts the guard.
  With that the entire guard-kill/loot/drag/lock sequence plays perfectly.
  **Blocker:** "By Guard Room" (`cl_Location11`) carries a `{*}` catch-all General
  task (`cl_NullAtStar`, priority 44074, PreventOverriding=1) that replies
  "Please press O then press Enter." to every command. After the guard is killed
  the room's ordinary North `<Movement>` should let you leave, but in FD the `{*}`
  task preempts the movement, so **no command leaves the room** (`n`/`north`/`out`
  all → "press O") and the player is trapped for the rest of the script. The real
  runner evaluates the direction movement ahead of the `{*}` task; FD does not.
  (Scarier is worse: it doesn't even parse the `creep south` specific task.)

Both are catalogued as engine-precedence findings — fixing Scarier's
`{*}`-vs-movement and OR-restricted-movement handling would let it *diverge from
FD in the correct direction*, but neither game can be a MATCH golden until FD (or a
different ground truth) can complete them.

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
- **BugHuntOnMenelaus** → `Bug Hunt On Menelaus.blorb` — **has a built-in
  `WALKTHROUGH`** (start menu → win in `YOU HAVE WON!!`). Harder than TBN: 6
  playable marines (`BECOME <name>` switching), an 80-turn timer, and combat.
  Intro handshake is `O`/`N`(not vision-impaired)/`B`/`Y`(timed-event on). Partly
  driven under FD but blocked at Davey's office-building 3rd-floor corridor: bare
  `n`/`north` return "I didn't understand" even though `exits` lists north/south
  (a pass-gated custom-movement task — the intfiction.org/t/63289 v35-vs-v36 spot);
  needs the right command found before it can be wired. Scarier also diverges hard
  here (`push 3` → "can't see any 3s", building nav desyncs) so expect a large
  DIVERGE budget until those are fixed.
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
Halloween, MuseumHeist, October31st, TheFortressOfFear, Tingalan, Tribute.
(TheFortressOfFear is Horsfield but has **no** built-in `WLKTHRGH`/`WALKTHROUGH`
task; the others are by Finn Rosenløv / Kenneth Pedersen.)

`DwarfOfDirewoodForest`, `TheEuripidesEnigma` and `TheLostLabyrinthOfLazaitch`
were moved OUT of this list on 2026-07-02 — they embed a built-in `WLKTHRGH`
native solution (see the ⭐ sections above; LostLabyrinth and TheEuripidesEnigma
are now wired — only DwarfOfDirewoodForest remains, FD-blocked).

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
