# TODO: wire remaining walkthroughs into the a5 regression harness

`test/run_a5_walkthroughs.sh` only exercises a game once it has (1) a game file
in `test/adrift5-games/`, (2) a turn-by-turn command script
`test/<Name>_walkthrough.txt`, and (3) a `name|game|vbudget|xbudget` line in the
script's `MAP`. `test/adrift5-games/walkthroughs/` already has raw
walkthrough/hint material for several games whose game files are staged but
that never got a command script or a MAP line. This is the backlog.

**STATUS (2026-07-06): the wiring backlog is CLEARED — every staged game is
wired.** The MAP now carries **41 games**: **33 golden-backed 0|0 MATCH in both
RNG modes** (TheFortressOfFear joined this tier 2026-07-06 — its 0|38 baseline
was driven all the way to a clean 0|0 FULL MATCH, see below), plus **RtC (0|13)**
— golden-backed vanilla MATCH whose xoshiro-13 gap is an explained FD-side bug
with Scarier on the correct side (`TODO_a5_walkthrough_bugs.md`) — and **7
vanilla-only System.Random-vs-xoshiro DIVERGE rows** (StoneOfWisdom 2|0,
JacarandaJim 99|0, SixSilverBullets 18|0, LostLabyrinthOfLazaitch 8|0,
October31st 106|0, LostCoastlines 1|0, Skybreak 2|0) whose xoshiro columns — the
real conformance metric — are all clean (0). Changes since 2026-07-02:

- **StarshipQuest UPGRADED to the PERFECT MAXIMUM 800/800, MATCH 0|0
  (2026-07-04)** — the native built-in walkthrough re-derived route now grafts
  the rhinopine feed (+5) back in after the `lay trail of nuts` bearion lure
  (the "mutually exclusive" theory was wrong: the lure moves the nuts onto the
  bend tree's branch, where they can be re-picked).  The old residual
  "Native's" hunk was a REAL engine bug, now fixed (OO `.Description` reads
  return marked-up text like FD's `Description.ToString`; display-boundary
  re-cap gated on an actual ALR rewrite) — see the ⭐ Starship Quest entry in
  `TODO_a5_walkthrough_bugs.md`.

- **BugHuntOnMenelaus 0|23 → 0|0 full MATCH.** The "documented FD gap" is gone:
  the corridor blocker was a **FrankenDrift crash** (cl_PlayerMove1's bracket
  sequence indexes past its restriction list → PassSingleRestriction got
  Nothing → NRE swallowed by GetGeneralTask; the known ADRIFT Runner v5.0.35
  bug, fixed upstream in v5.0.36) — **fixed in FD** (null-guard, commit
  25c4e3ea), so FD now wins 100/100 too. The residual `read sign` divergence
  (2) fell to the per-character `HasSeenObject` + `msg_has_output`/bHasOutput
  fixes (see `TODO_a5_walkthrough_bugs.md`).
- **FinnsBigAdventure WIRED at MAX SCORE 500/500** (572 turns, deterministic,
  golden, 0|0) — derivation log in `TODO_fba_walkthrough_progress.md`.
- **DwarfOfDirewoodForestDDF (the older DDF build) WIRED as a real WIN
  250/250, 0|0** (`light rope`, not `light fuse`, disarms the field capture);
  Version 9 is confirmed UNWINNABLE (game-data bug) and stays wired as the
  FD-trapped conformance MATCH (see `TODO_a5_walkthrough_bugs.md` 📄 DDF entry).
- **AoS (Alyas of Starhollow) WIRED 0|0** — new corpus game (`AoS v.4.blorb`),
  full MATCH after the put-all fail-aggregation / `FailOverride` fixes.
- **TheBookOfJax WIRED as a full 500-point WIN, 0|0** (652 turns, `BoJ
  v.2.blorb`) after the markup-aware pSpace fix. Golden
  `test/BookOfJax_expected.txt` blessed 2026-07-03 (verified 0|0 live vs FD in
  both modes immediately before blessing) — every MATCH game is now
  golden-backed.
- **Tingalan WIRED as a real 16-command WIN, 0|0** (lore book → Village search
  → wagon → Smiling Spirit → decline the pearl → wait until dawn). The
  accept-the-pearl deep-woods survival run remains an optional stretch goal
  (see `TODO_a5_walkthrough_bugs.md` ⭐ Tingalan).
- **Halloween, MagorInvestigates, MuseumHeist, October31st, TheFortressOfFear,
  Xanix WIRED 0|0 as 4-command opening-turn smoke probes**
  (look / examine me / inventory / wait) — conformance guards for the intro +
  first-room render, NOT wins.
- **MuseumHeist UPGRADED to a PERFECT-SCORE run 0|0 (2026-07-03)**: blind-derived
  36-command route, TOTAL SALES 940M = the game's own top Feedback tier ("truly
  perfect!").  Surfaced + fixed 3 general engine bugs (BeInsideObject/BeOnObject
  in/on-chain recursion, Unicode keys in OO expressions, bare-DirectionsEnum
  `.Name`) — see the ⭐ Museum Heist entry in `TODO_a5_walkthrough_bugs.md`.

- **GrandmasFlyingSaucer WIRED as a FULL WIN 10/10, 0|0 (2026-07-03)** — new
  corpus game (Kenneth Pedersen, TALP 2022, `GFS_Frankendrift.blorb` +
  Garry Francis's solution, both supplied by the user).  Conversion notes:
  `G`/AGAIN is not understood by FrankenDrift (nor Scarier) — every `G` is
  spelled out as the repeated command; the solution under-counts the grandma
  intro conversation by one (needs 8 talks).  Surfaced **5 general engine
  fixes** (runtime location-group statics, furniture-seat location sync,
  ALR-free action values, two-slash perspective brackets, on-furniture
  effective character location) — see commit 051fdab8.
- **TheGardenParty WIRED as a FULL WIN 100/100, 0|0 (2026-07-03)** — new
  corpus game (Larry Horsfield mini-comp entry, user-supplied blorb +
  walkthrough).  Converted verbatim (no start-menu handshake in this one,
  unlike the big Horsfield games); replayed 0|0 clean on the first try, no
  engine changes needed.

- **Halloween UPGRADED to a blind-derived FULL WIN 0|0 (2026-07-03)** — Finn
  Rosenløv's Danish-language haunted-house game, no walkthrough material
  exists anywhere; the 42-command win (`dræb dracula` → `*** Du har
  vundet ***`) was derived purely from the `a5dump` model XML
  (Tingalan/MuseumHeist template).  Surfaced **2 general engine fixes**
  (character-in-closed-container "is here" visibility; rich-Text property
  value_node rendering on characters/locations) — see the ⭐ Halloween entry
  in `TODO_a5_walkthrough_bugs.md`.

- **October31st UPGRADED to a FULL 100/100 WIN (2026-07-03)** — converted
  from the author's PDF walkthrough (user-supplied).  **xoshiro 0 = full
  every-line conformance MATCH**; vanilla 106 = inherent System.Random
  walk divergence (werewolf/mummy random walks; FD-vanilla even dies), so
  it is a DIVERGE row with no golden, JacarandaJim-class.  Surfaced **2
  general engine fixes** (walk/event sub-display `<DisplayOnce>` retire;
  command-topic keywords CorrectCommand) — see the ⭐ October 31st entry in
  `TODO_a5_walkthrough_bugs.md`.

- **Illumina WIRED as a FULL WIN (2026-07-03)** — new corpus game (Finn
  Rosenløv, user-supplied blorb + the author's CASA solution).  13 commands,
  one correction ("open guard room door" → the first "open southern door").
  **Scarier wins; FrankenDrift cannot parse `open southern door` ("Open
  what?") — a real FD reference-resolution gap**, so it is wired
  BugHunt-style: golden = Scarier's winning transcript (vanilla 0), xoshiro
  budget 5 = the documented FD gap.  Root-causing/fixing the FD parser gap
  (and then re-blessing to 0|0) is an open follow-up.
- **LostChildren + MagneticMoon REWIRED to their NATIVE built-in WLKTHRGH
  solutions (2026-07-03)** — the old scripts were CASA solutions for the
  Spectrum ORIGINALS and derailed against the ADRIFT 5 ports (user report);
  both games were missed by the 2026-07-02 Horsfield audit exactly like
  Tribute.  LostChildren: full rescue-ending win, zero corrections, 0|0.
  MagneticMoon: 795/800 win, 2|2 (two open one-char hunks).  9 engine fixes
  between them — see `TODO_a5_walkthrough_bugs.md`.

- **MagorInvestigates UPGRADED to a blind-derived FULL WIN 0|0 (2026-07-03)** —
  `*** CONGRATULATIONS! ***`, 64 turns, 9 tasks, golden
  `test/MagorInvestigates_expected.txt`.  Derived entirely from the `a5dump`
  model XML (win = set `cl_LineageTra` then `Up` to King Kelson; the whole game
  is the herbal-tea-cure-the-archivist → trace-the-lineage chain the hints
  fragment pointed at).  Surfaced **1 general engine fix** (the
  `render_look_string` StartAfterDefaultDescription rebuild-from-default, so the
  fire-lit chamber's opening auto-look keeps the room view instead of Scarier's
  spurious "It is too dark…" override) — see the ⭐ Magor entry in
  `TODO_a5_walkthrough_bugs.md`.

**Backlog CLEARED (2026-07-06).** Xanix WON 2026-07-05, and the last holdout
**TheFortressOfFear** is now a blind-derived FULL MAX-SCORE WIN 1500/1500
(see the ⭐ TheFortressOfFear entry below; golden-backed **0\|0 FULL MATCH** —
the 0\|38 baseline was fully closed 2026-07-06 through the xoshiro-driven fixes
in commits d987cdf2 (38→2), 5fa31363 (2→1) and afc50a88 (1→0), all catalogued in
`TODO_a5_walkthrough_bugs.md`). Every game with any usable
walkthrough source is now wired as a real win or conformance MATCH.
LostCoastlines and Skybreak are smoke-wired into the MAP (2026-07-06,
DIVERGE-at-baseline, see below) but stay smoke probes permanently — no usable
walkthrough source exists for either and both are RNG-heavy enough that a real
win wouldn't byte-align to FrankenDrift anyway.

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
| ThingsThatGoBumpInTheNight | WALKTHROUGH | ✅ **WIRED + FIXED 2026-07-02** (8\|8 → **0\|0 MATCH**, golden) — 3 cut-scene corrections |
| **LostLabyrinthOfLazaitch** | WLKTHRGH | ✅ **WIRED + FIXED 2026-07-02** (403\|403 → **8\|0, xoshiro FULL MATCH**) — full 520-pt win, ZERO corrections (see below) |
| BugHuntOnMenelaus | WALKTHROUGH | ✅ **WIRED + FULL WIN 2026-07-02**, then **0\|23 → 0\|0 full MATCH 2026-07-03** — the gap was an FD crash, fixed in FD; see below |
| DwarfOfDirewoodForest | WLKTHRGH | ✅ **WIRED + FIXED 2026-07-02** (0\|0 conformance MATCH — v9 confirmed UNWINNABLE, FD-trapped; see below). **The older DDF build WINS 250/250** — wired separately as `DwarfOfDirewoodForestDDF` (0\|0) on 2026-07-03 |
| TheEuripidesEnigma | WLKTHRGH | ✅ **WIRED + FIXED 2026-07-02** (11\|11 → **0\|0 MATCH**, golden) — full 400-pt win; the `4` desync was just a downstream artefact of ONE spurious `hit fork on face` (see below) |
| FinnsBigAdventure (FBA v.3c) | ❌ **vestigial WT** | **NO built-in walkthrough (verified 2026-07-02)** — email-only. ✅ **Blind play paid off: WIRED 2026-07-03 at MAX SCORE 500/500 (0\|0, golden)**; see the hints-only group below + `TODO_fba_walkthrough_progress.md` |
| MagorInvestigates | none (email-only) | ✅ **BLIND-DERIVED FULL WIN 0\|0 (2026-07-03)** from the `a5dump` model — golden, +1 engine fix (see ⭐ Magor in the bugs TODO) |
| XanixXixonResurgence | none | ✅ **BLIND-DERIVED FULL MAX-SCORE WIN 600+/600 (2026-07-05), MATCH 0\|0 both modes** — 370-command run; the "randomised endgame" is one `RAND(1,100)>=30` survival roll per turn (attack the hybrid on the 2nd turn after the KO); +1 engine fix (room-view ALR sealing, see ⭐ Xanix in the bugs TODO) |

Caveat: the built-in text was authored against a slightly earlier build, so some
moves get absorbed by this build's scripted cut-scenes and must be corrected
against FrankenDrift (intro auto-walks and patrol/teleport cut-scenes are the
usual culprits). LostLabyrinth is the exception — it replayed byte-clean.

### ⭐ BugHuntOnMenelaus — WIRED as a FULL WIN; now 0|0 MATCH (the "FD gap" was an FD crash, fixed in FD)

> **UPDATE 2026-07-03: 0|23 → 0|0 full MATCH both modes.** The corridor
> blocker described below was root-caused as a **FrankenDrift NRE**, not an
> engine-model divergence: `cl_PlayerMove1`'s bracket sequence indexes past its
> restriction list, `PassSingleRestriction` receives Nothing, `restx.Copy`
> throws, and `GetGeneralTask` swallows it → no task selected → "Sorry, I
> didn't understand that command." (This is the known ADRIFT Runner v5.0.35
> bug, fixed upstream in v5.0.36; game hint thread intfiction.org/t/63289.)
> Fixed in FrankenDrift with a `PassSingleRestriction` null-guard (commit
> 25c4e3ea) — FD now walks the corridor and wins 100/100, matching Scarier.
> The 2 residual `read sign` hunks then fell to the per-character
> `HasSeenObject`-across-BECOME fix and the `msg_has_output`/bHasOutput fix
> (both in `TODO_a5_walkthrough_bugs.md`). Golden re-blessed; MAP row now
> `0|0`. The historical analysis below is retained — its Scarier-side BECOME
> findings all stand; only the "FD cannot finish" conclusion is superseded.

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

BugHunt (see the ⭐ section above) was briefly the "diverges in the correct
direction" case of this class (Scarier wins, FD stops, FD gap 23 carried in
the xoshiro column) — until 2026-07-03, when the FD-side blocker turned out to
be a fixable FD crash; it is now an ordinary 0|0 MATCH. Dwarf's
`{*}`-vs-movement trap is different: there FD and Scarier agree byte-for-byte
(both engines trap identically), so it always was a MATCH — and the older DDF
build sidesteps the trap entirely and is wired as a real 250/250 win
(`DwarfOfDirewoodForestDDF`).

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

### ⭐ TheEuripidesEnigma — native solution wired (11|11 → 0|0 MATCH); the `4` was a red herring + an exponential ALR-recursion hang fixed

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
bugs) were three families of object-listing / render bugs: (a) a dropped object
that the room's long description already names by prose ("boom box"/"small
drone") *also* appended to the "Also here is …" auto-list; (b) a one-off doubled
cliff-face description; (c) one movement-message wording variant. **All fixed
same day → 0|0 MATCH, golden `test/TheEuripidesEnigma_expected.txt`** — see the
⭐ Euripides entry in `TODO_a5_walkthrough_bugs.md` (runtime ExplicitlyExclude,
pass-text dedup, map-path DisplayOnce retire).

## Have a full walkthrough, need conversion to a command script

- ~~**CallOfTheShaman**~~ ✅ **WIRED + FIXED (2026-07-02), 0|0 MATCH** (golden
  `test/CallOfTheShaman_expected.txt`). Full 265-point win
  (`*** CONGRATULATIONS! ***`) in both RNG modes.
  The source file is a ROT1-encoded *hint sheet* (not a turn-by-turn
  walkthrough), so — like StoneOfWisdom — the win script
  (`test/CallOfTheShaman_walkthrough.txt`) was derived by actually playing the
  game to a win using the decoded hints as a route guide. The 3 original hunks
  (RNG-independent endgame-banner bugs: `%Turns%` not substituted +
  leading-cap URL mangling) were fixed same day — see the ⭐ Shaman entry in
  `TODO_a5_walkthrough_bugs.md`.
- ~~**LostCoastlines**~~ ✅ **SMOKE-WIRED (2026-07-06), DIVERGE 1|0 (at
  baseline).** No real walkthrough exists (`walkthroughs/LostCoastlines_walkthrough.pdf`,
  8.6 MB, is an image-only hand-drawn nautical map feelie — no text layer, no
  commands) and Lost Coastlines is a procedural/random sailing-exploration game
  that can't byte-align to FrankenDrift anyway, so this stays a 4-command smoke
  probe (`test/LostCoastlines_walkthrough.txt`), not a real win. Getting the
  `.taf` to load at all surfaced a general engine fix — see the ⭐
  LostCoastlines/Skybreak entry in `TODO_a5_walkthrough_bugs.md`. The 1 residual
  vanilla hunk (xoshiro is clean, `0`) is the randomised starting-outfit text
  (`%defaultshirt[Rand(1,10)]%`): a System.Random-vs-xoshiro draw-order pick, not
  a bug — it vanishes under the aligned stream, so no golden blessed (same class
  as JacarandaJim/SixSilverBullets/StoneOfWisdom/LostLabyrinthOfLazaitch/
  October31st/Skybreak).
- ~~**Skybreak**~~ ✅ **REAL MODEST PLAYTHROUGH WIRED (2026-07-06), golden-backed
  DIVERGE 0|1 (at baseline; was a 4-command smoke probe at 2|0).**
  No published walkthrough exists (`walkthroughs/Skybreak_walkthrough.pdf`, 16 pp,
  is the obfuscated in-game manual/lore, not a command sequence) and Skybreak is a
  class-based cosmic roguelike with a semi-random travel path, RNG-gated grind
  wins, and space-combat minigames — so a full deterministic WIN is a long
  derivation (left as documented follow-up). The probe was replaced with a
  genuine, correctly-handled session: create a Human Earthling+Explorer, then ten
  hyperspace jumps surviving one Lone Wolve space battle, ending docked at Lambda
  Apis. This was previously IMPOSSIBLE — the random jump landed the player nowhere
  (`MoveCharacter ToLocationGroup` read only static group members, but the
  destination group is runtime-populated each jump), rendering the world as the
  literal `Player.Location.Description`. Fixed generally with an FD-faithful live
  group-membership list (`clsGroup.arlMembers`); see the ⭐ Skybreak random-jump
  entry in `TODO_a5_walkthrough_bugs.md`. The 1 residual xoshiro hunk is the
  initial dock's flavour-line draw-order pick (two adjacent `%var[rand(1,25)]%`
  arrays resolved in FD's `htblVariables` hash order) — the documented
  `%defaultshirt[Rand]%` RNG class, not a bug. Vanilla golden-backed (0).

## Have hints only (not a full walkthrough) — treat with the StoneOfWisdom caution

The `*_hints.txt` files here are just **intfiction.org forum-thread fragments**
(a single stuck puzzle each), NOT walkthroughs — Larry Horsfield only sends full
walkthroughs by email, and none are on CASA/public (verified 2026-07-02).
StoneOfWisdom's first script was a hint-sheet reconstruction with no real
navigation that tested nothing; assume the same risk here. The unlock for two of
these is the **built-in `WALKTHROUGH` command** (see the ⭐ section above) — that
gives a real solution to correct against FD, no blind play needed.

- ~~**ThingsThatGoBumpInTheNight**~~ ✅ **WIRED + FIXED (2026-07-02), 0|0 MATCH**
  (golden `test/ThingsThatGoBumpInTheNight_expected.txt`). Full max-score win
  (`*** CONGRATULATIONS! ***`, 250/250, 310 turns), derived from the
  game's built-in `WALKTHROUGH` with three cut-scene corrections (spurious intro
  `n` dropped; `snuffio`+`s` added so the guard patrol carries you to the Treasury
  door in the dark — the exact intfiction.org/t/77639 sticking point). The 8
  original hunks (`drop all` over-expansion killing the player at the ravine +
  the dark-room `get dirt` line) were the BeExactText/SetTasks-Execute response
  model — fixed same day, see the ⭐ TBN entry in `TODO_a5_walkthrough_bugs.md`.
- ~~**BugHuntOnMenelaus**~~ ✅ **WIRED 2026-07-02 as a FULL WIN (`0|23`).** Scarier
  now plays it to `*** CONGRATULATIONS! ***` (100/100) via the built-in
  `WALKTHROUGH` once BECOME player-switching was implemented — and wins where FD
  is blocked at Davey's pass-gated corridor. See the ⭐ section above.
- ~~**FinnsBigAdventure**~~ ✅ **WIRED 2026-07-03 at MAX SCORE 500/500 (0|0
  MATCH both modes, golden `test/FinnsBigAdventure_expected.txt`).** The blind
  play-to-win succeeded: derived via the game's `WWDD` per-location hint system
  + model scoring-task extraction, section by section (100 → 160 → 230 → 245 →
  285 → 330 → 450 → 500), full deterministic win in 572 turns, byte-verified
  against FD. The last +50 was the orb chain, `stand in pouffe`, `read words`,
  Mannbroom's questions, telescope recon, the Fancy Dress question, the farmer
  offer, and the Chop Shop meal; several unscored tasks are confirmed
  dead/unreachable data bugs (`cl_XFireplace`, `cl_TieLeashTo`, `cl_RowNeFromR`,
  `cl_SankoraSee1`). Full derivation log: `TODO_fba_walkthrough_progress.md`.
  Also surfaced 4 general engine fixes along the way (MoveCharacter
  InsideObject/OntoObject/ToParentLocation, plural-path event-task completion,
  restriction-fail `<Message>` on event/System/walk tasks, custodian-niche
  stealth — see `TODO_a5_walkthrough_bugs.md`).
- ~~**MagorInvestigates**~~ ✅ **BLIND-DERIVED FULL WIN 0|0 (2026-07-03)** →
  `MI_v.1.blorb`.  No built-in `WALKTHROUGH`; the whole win was reconstructed
  from the `a5dump` model XML (the hints fragment — herbal tea, kettle in the
  mug's room — was just the one puzzle it pointed at).  Turned out to be a
  ~64-turn linear investigation, not a ~200-move maze: LUMINO → specs → meet
  Stinker → herbal → pick chamomile+peppermint → brew (kettle/tap/arm/towel) →
  cure Stinker → trace the lineage at the long table → report → up to the King.
  Golden `test/MagorInvestigates_expected.txt`; +1 engine fix (⭐ Magor in
  `TODO_a5_walkthrough_bugs.md`).
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
  vanilla-align; xoshiro-only at best. ~~**Smoke probe wired 0|0 (2026-07-03,
  MAP name `Xanix`) — opening-turn conformance covered; the win is the open
  item.**~~
  **UPDATE 2026-07-05: BLIND-DERIVED FULL MAX-SCORE WIN, MATCH 0|0 in BOTH RNG
  modes** (the "won't vanilla-align" fear was wrong: the fight is ONE
  `cl_SurvivalRa = RAND(1,100)` roll per turn, checked `>=30` by the axe attack
  — a 71% window that both vanilla System.Random and xoshiro happen to pass on
  the same turn).  370-command run, best ending (`Score >= 600` → Endgame150),
  625 internal points at turn 369.  Route highlights: caravansery outfitting
  (camels/equipment/clothes/provisions, character-swap haggling as Kelson) →
  sceptre + map from the wall bodies → red-mist shelter in the basement
  (clear mudbricks → trapdoor → CLOSE it) → madman's key → tinsmith key-cast
  (sweep/equipment/scuttle/coal + wax stopper→disc→impression→molten tin→
  trough) → outcrop cave glowing marks (the two death-trap hints) → Erwin
  caravan Q&A → jaguar path for the dead bird → feed the stairway snake →
  golden queen in the vegetation, silver knight in the magpie nest (tree via
  LEAVE KELSON, branch breaks onto level 3) → statue gauntlet (KNEEL, lever,
  cut ropes, leap-of-faith chasm) → tomb chess ghosts (magic arbalest+quiver)
  → native capture (break bed, rub rope, SHOUT, GET SCEPTRE) → city recce
  (8 spots, guard-cycle timed runs) → two-gate assault (arbalest ×2 E, longbow
  W) → hide bodies → dark-room conference → poison the fountain (crawl while
  the officer is inside) → prison keys → 4 jail questions auto-release →
  hybrid: attack with axe EXACTLY 2 turns after the KO (first survival roll
  lands then) → teeth trophy → native village heals Kelson → camels → Siwa
  sell-back → horses → ride North.  Golden `test/Xanix_expected.txt`; +1
  engine fix (⭐ Xanix room-view ALR sealing in `TODO_a5_walkthrough_bugs.md`).

## No walkthrough material at all yet — now smoke-probed; real wins still open

These game files were staged in `test/adrift5-games/` with no walkthrough or
hints anywhere in the corpus: Halloween, MuseumHeist, October31st,
TheFortressOfFear, Tingalan. (TheFortressOfFear is Horsfield but has **no**
built-in `WLKTHRGH`/`WALKTHROUGH` task; the others are by Finn Rosenløv.)

**Update 2026-07-03:** all five are now in the MAP at 0|0. **Tingalan** got a
real derived 16-command winning walkthrough (see the ⭐ Tingalan entry in
`TODO_a5_walkthrough_bugs.md` — engine RNG loop fixed, win path
reverse-engineered from the model). **MuseumHeist** was then upgraded the same
way (later 2026-07-03) to a blind-derived PERFECT-SCORE 940M run (36 commands,
3 engine fixes — see the ⭐ Museum Heist entry in `TODO_a5_walkthrough_bugs.md`;
the whole derivation ran off `a5dump`'s model XML + iterative `a5run_dump`
probing, no `A5_DUMP_VARS` needed). **Halloween followed (2026-07-03 evening):
blind-derived FULL WIN 0|0, 2 engine fixes — see the ⭐ Halloween entry in
`TODO_a5_walkthrough_bugs.md`.** October31st was later upgraded too (see
above). **TheFortressOfFear** was the last of the five, upgraded 2026-07-06
from the 4-command smoke probe to a blind-derived FULL MAX-SCORE WIN
1500/1500 (see the ⭐ TheFortressOfFear entry below) — the backlog is clear.

### ⭐ TheFortressOfFear — ✅ WON 2026-07-06: blind-derived FULL MAX-SCORE WIN 1500/1500, golden-backed 0\|0 FULL MATCH (from a 0\|38 baseline)

No built-in `WLKTHRGH`/`WALKTHROUGH` and no external walkthrough exists (Horsfield
sends these by email only, per the ⭐ section above; none turned up for this
title). Blind-derived the same way as Tingalan/MuseumHeist/Halloween/Magor/
Xanix: BFS shortest-path routing over the `a5dump` map graph
(`test/route.py`, fed from a `map.tsv` scrape) plus a location-tracking replay
annotator (`test/annotate.py`) to keep the running script honest turn-by-turn,
driven via `test/run.sh` (`a5run_dump` against `test/adrift5-games/TheFortressOfFear.blorb`).

Score is shown `/1500` in-game but the real win check (`s_Endgame120`,
task `EndGame1200`) only requires `Score >= 1200` at the bell ropes; the final
script lands the FULL 1500 anyway (`*** You have won ***`, 2115 turns,
`test/TheFortressOfFear_walkthrough.txt`, golden
`test/TheFortressOfFear_expected.txt`). Finale route from checkpoint 6
(bridge 263, 1384 pts): courtyard ambush auto-shelters you at the well →
load shield+lantern into the bucket (calfskin into the worn scrip — the
bucket refuses non-Property15 "would get wet" items), lower bucket, climb
down empty-handed → lantern-in-bucket reveals the well door → tunnel →
dusty cellar (mirror on shelves) → cellar 117/118 (see candle light +
retrieve the sword/henbane pushed through the crack) → `x aisles` at 297
*is* the Var223 gate for the 297→298 route → glue pot @298, glue mirror to
shield, clean it with the calfskin held (Var246) → larder: `search larder`
(Var236 "food eaten" — REQUIRED by the guard WinFight!) + `throw henbane
into cauldron`, wait 3 turns at 299 for Grub's Up → kitchen (touch NOTHING)
→ corridor 301: ONE-turn window, `creep up on guard` (needs chainmail WORN
+ shield HELD + short sword HELD + Var236, else scripted death) → swap to
the guard's dagger+longsword, drop the short sword (holding both swords or
neither at the Event20 6-turn check = death) → 303 mercs talk, `follow other
mercenaries` (3-turn window) → 304 `throw dagger at tall mercenary` (needs
longsword held, short sword NOT) → 305 open doors, drop all, keep ONLY the
shield → 120 Wladyslaw auto-showdown (clean mirror on held shield,
Held.Count==1) → teleported to 123 The Crossing, whose ONLY exit is
`find bell tower entrance` (Task1420; also the +5 that replaces the
tapestry variant — same Var250) → open bell tower door → `run up stairs` →
`wedge door with chair` within 1 turn (Event24) → drop shield →
`pull bell ropes` (+10, win) within 4 turns of the wedge (Event25).

The finale surfaced TWO general engine bugs, both fixed (see the ⭐
TheFortressOfFear entry in `TODO_a5_walkthrough_bugs.md`): (1) wildcard
command variants — FD's `GetRegularExpression` tries `%object% *` with the
wildcard REMOVED first, so `find bell tower entrance` binds the whole
phrase to the unseen Bell Tower door instead of lazily splitting at "bell";
without it Task1420 never fires and Loc123 is an unwinnable dead end;
(2) the 256-byte `ref_value` binding buffer truncated big drop-all/get-all
`ReferencedObjects` pipe-lists ("…the large hammer and Objec"/"and O"/
phantom "the secret door", items silently missing from aggregate lists).
The 0\|38 baseline (NPC-timing / convo-repeat / +3-score-drift classes) was then
closed OUT completely — a fourth run of xoshiro-driven fixes (commits d987cdf2
38→2, 5fa31363 2→1, afc50a88 1→0; the last being the endgame "answers above" on
the silent winning task) took it to a **0\|0 FULL MATCH**, golden re-blessed
2026-07-06. See `TODO_a5_walkthrough_bugs.md` for each fix.
All six were re-checked 2026-07-03 for hidden built-in walkthrough tasks
(`a5dump | grep -i 'walkthrou\|wlkthrgh'`): none — Magor/Xanix only carry the
email-the-author blurb.

**⚠️ Recheck each of these for a built-in `WALKTHROUGH`/`WLKTHRGH` task before
assuming "no material"** — that is exactly how Tribute (below) was missed. Grep
the `a5dump` output for `walkthrou`/`wlkthrgh` and confirm it's backed by a real
`<Command>[walkthrough…]` printer task (a `<Text>` full of `> cmd` lines), not a
vestigial HELP mention like FinnsBigAdventure's dead `cl_Walkthroug5` event.

### ⭐ Tribute — a built-in `WALKTHROUGH` was hiding here; wired as a FULL WIN (0\|0 MATCH)

**WIRED 2026-07-02, FIXED to 0\|0 MATCH same day** (golden
`test/Tribute_expected.txt`). Tribute ("Return to the City of Secrets,"
Kenneth Pedersen, based on Emily Short's *City of Secrets*) was mis-filed under
"no walkthrough material" but ships a real built-in `WALKTHROUGH` command (a
`Walkthroug` task, `[walkthrough/walkthru/walk through/walk thru]`, `<Text>` a
143-line `> cmd` solution). Extracted verbatim with **zero corrections** — no
start-menu handshake (the intro "Press a key to continue" is auto-advanced by
the harness), no cut-scene fixes. Scarier plays it straight to
`*** You have won ***`, 100/100, 143 turns. The 2 residual hunks turned out to
be an authored-ALR-blanks-a-message paragraph-slot bug (NOT the override
theory) — fixed via the pre-ALR `bHasOutput` test; see
`TODO_a5_walkthrough_bugs.md` (⭐ Tribute entry, DONE).

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
