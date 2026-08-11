# ADRIFT 5 walkthrough regression corpus — findings

Differential regression tests built from published walkthroughs for the
real-world ADRIFT 5 games in `test/adrift5/games/`, plus the synthetic probe and
Samples rows documented at the foot of this file. Each game's walkthrough was
cleaned into a one-command-per-line script
(`test/adrift5/goldens/<Game>_walkthrough.txt`, anno1700 style: `#` headers +
bare commands) and replayed through **both** the Scarier harness
(`test/adrift5/harness/a5run_dump`) and the **FrankenDrift** reference engine via
`test/adrift5/harness/a5_groundtruth.sh`. FrankenDrift is the ground truth.

> **Pruned companion docs.** Several `TODO_*.md` design/diagnosis docs cited
> here and in code comments were deleted once their last entry closed; the
> write-ups remain in git history, recoverable with
> `git log --all --diff-filter=D -- '*<name>.md'` and then `git show <sha>^:<path>`.
> They are: `TODO_a5_walkthrough_bugs` (the conformance-bug ledger, pruned
> 2026-07-14), `TODO_a5_aggregateoutput_suppression`, `TODO_a5_undo`,
> `TODO_a5_walkthrough_wiring`, `TODO_a5_frankendrift_save_compat`,
> `TODO_aliendiver_divergences` and `TODO_fba_walkthrough_progress` (all
> 2026-07-11), and `TODO_following_you`, `TODO_questgiver_divergence` and
> `TODO_symphonica_schtick_ordering` (2026-08-09).
>
> **This file itself was pruned 2026-08-10.** Two sections went: a 32-row
> per-game status table (superseded by the MAP, and stale — it still called
> Lost Children, Magnetic Moon, Grandpa's Ranch and Jacaranda Jim "diverge"
> long after they were fixed), and the 17-entry *Prioritized Scarier
> conformance bugs* ledger, every entry of which is now closed. The mechanisms
> those entries described live at the fix sites as code comments; the dated
> derivation history lives in the comment block of `run_a5_walkthroughs.sh`;
> the removed text is in git
> (`git log -p -- test/adrift5/notes/A5_WALKTHROUGH_FINDINGS.md`).

## Running

```sh
# Default: Scarier in isolation (goldens only; no FrankenDrift / dotnet)
make -f Makefile.headless test

# Opt-in: full corpus vs FrankenDrift (needs FD.Headless + dotnet)
make -f Makefile.headless test-fd
# equivalents:
make -f Makefile.headless a5walkthroughs
make -f Makefile.headless a5run          # build test/adrift5/harness/a5run_dump
test/adrift5/harness/run_a5_walkthroughs.sh
test/adrift5/harness/run_a5_walkthroughs.sh -v Spectre   # one game, dump diff
test/adrift5/harness/a5_groundtruth.sh test/adrift5/games/<Game>.blorb \
    test/adrift5/goldens/<Game>_walkthrough.txt
```

`make test` runs golden-backed a5 rows via `--golden-only` (SKIP if no
`*_expected.txt`). `test-fd` / `run_a5_walkthroughs.sh` reports **MATCH**
(Scarier == FrankenDrift), **DIVERGE n** (n diff hunks, at the per-game
baseline budget recorded in the runner's MAP), **OKbetter** (below budget in
some mode — that is a fix, re-bless the MAP row) or **FAIL** (over budget, or
the save/restore self-check diverged — a regression). Games with a committed
golden (`<Game>_expected.txt`) are strict-diffed against Scarier with no
dotnet dependency.

## Corpus status

As of **2026-08-11: 199 rows — 182 MATCH, 17 DIVERGE at baseline, 0 FAIL**,
about 30 s wall clock at the default `-j8`.

Per-game numbers are deliberately **not** tabulated here any more; they went
stale faster than they were read. The authoritative record is:

* the **MAP** at the foot of `test/adrift5/harness/run_a5_walkthroughs.sh` — one
  `Game|file|vanilla|xoshiro` row per game, where the budget *is* the assertion;
* the ~1400 lines of dated wiring commentary above it, which is where each
  game's route, engine fixes and re-blessings were logged as they happened;
* the committed goldens and scripts in `test/adrift5/goldens/`.

Every structural conformance bug this corpus surfaced has been fixed. **Every
remaining non-zero budget is RNG-stream noise, a deliberate divergence, or the
one budgeted cosmetic gap in AlienDiverV13** — the xoshiro column is the real
conformance verdict, so a row at `N|0` is byte-identical to FrankenDrift and its
vanilla `N` is flavour text drawn from a different stream. The 17 rows, and why
they are not zero:

| Row | v\|xo | why |
|---|---|---|
| StoneOfWisdom | 2\|0 | vanilla RNG noise only |
| JacarandaJim | 99\|0 | the corpus's most RNG-soaked game (its own history: 271 → 101 → 99) |
| JacarandaJim2011 | 37\|0 | same game, 2011 release, same RNG class |
| SixSilverBullets | 18\|0 | `Roller`/time-trap draws; xoshiro 0 since the `Roller Must BeEqualTo 'RAND(1,16)'` restriction was made to *draw* (`num_value`, a5restr.cpp) instead of `strtol`-ing the quoted expression to 0 |
| SixSilverBulletsTruth | 111\|0 | second (truth-ending) script through the same RNG subsystem |
| LostLabyrinthOfLazaitch | 8\|0 | vanilla RNG noise only |
| October31st | 106\|0 | werewolf random walk |
| October31stComp | 106\|0 | same profile, comp release |
| Oktober31Dansk | 32\|0 | same werewolf class, Danish release |
| ISummonThee | 5\|0 | vanilla RNG noise only |
| AlienDiverV13 | 0\|26 | **authored-text/model drift in an alternate build**, two cosmetic classes: 14 × a "Sorry, I'm not sure which object you are trying to #." FD does not emit, and 12 × `"Playable Card"` for FD's `Playable Card` — v13 sets the card noun with `SetProperty ReferencedObject _ObjectNoun "Playable Card"` onto a property the object does not yet carry, and the runner's add-branch (which Scarier mirrors) stores such a value raw instead of evaluating the quoted string. Vanilla is a golden diff, hence 0 |
| TempusFugit | 0\|1 | one stray blank line (~line 538 of the xoshiro transcript); cosmetic, undiagnosed. Vanilla is a golden diff, hence 0 |
| OS (PlugIn.Exe) | 0\|1 | **architectural, deliberately left.** `PlayerWin` is a Specific override on `Stand1`'s AfterTextAndActions that zeroes `Aipoints`, and the runner expands an AggregateOutput message at Display — i.e. *after* those actions — so its `%aiPoints%` reads 0 and it prints "Tester wins!". Scarier renders the static skeleton at emit time (deferring only the random-bearing pieces), still sees 22, and prints "AiMReele99 wins.". Closing it means deferring whole aggregate messages, which puts pSpace/position semantics at risk corpus-wide |
| ProbePopups | 4\|4 | see the probe table below |
| ProbeTaskActions | 1\|0 | vanilla RNG noise only |
| ProbeUndoAfterEnd | 0\|4 | **not our bug** — pins a Runner bug Scarier does not reproduce; must stay exactly 4 |
| ProbeVariables | 1\|1 | `%Version%` only: every interpreter reports its own |

Two more games carry heavy vanilla RNG divergence that the committed golden
hides (their vanilla column reads 0 because it is a Scarier self-golden diff, not
an FD diff): **BeginnersCave** (~136 hunks — every turn of substance is a T&T
combat round) and **AlienDiver** (vanilla FD lands the crashed ship in a
different room, so the game is only FD-diffable under `FD_RNG=xoshiro`).

### Deliberate divergences from FrankenDrift

Three, and only three — anything else that differs is a bug:

1. **`<TaskExecution>` version gate** (`a5model.cpp`, the one place Scarier
   knowingly leaves FD). Files with no `<TaskExecution>` element predate the
   5.0.22 setting and ran with the v4-compatible `HighestPriorityPassingTask`
   behaviour in Campbell's Runner; FrankenDrift hardcodes
   `HighestPriorityTask` regardless of version, which makes genuinely
   pre-5.0.22 games unwinnable (Return to Camelot, v5.000020, whose central
   unlock chain only fires as a lower-priority *passing* task after the stock
   library's "cannot be unlocked" fallback). Scarier falls back on the file
   version: below 5.000022 ⇒ `HighestPriorityPassingTask`, at/after ⇒
   `HighestPriorityTask`.
2. **ProbeUndoAfterEnd 0|4** — pins [adrift.co bug 19196](https://www.adrift.co/bug/19196)
   ("UNDO after the game ends re-runs events"), which Scarier does not
   reproduce. The 4 xoshiro hunks must stay *exactly* 4: a drop to 0 would mean
   Scarier had acquired the bug.
3. **OS 0|1** — the aggregate-message-at-Display gap in the table above, left
   open on purpose because closing it risks pSpace/position semantics
   corpus-wide.

## Caveats

- **RNG divergence** (vanilla mode): Scarier (xoshiro128\*\*) and FrankenDrift
  (.NET System.Random) produce different random text even seeded — combat /
  epigraph / dream / "random catch" lines differ. **`FD_RNG=xoshiro`** patches
  FrankenDrift to draw from a byte-identical xoshiro128\*\* stream
  (`FrankenDrift.Headless/Program.cs` `XoshiroRandom`); RAND choices then match
  Scarier exactly. The runner reports both columns and **the xoshiro column is
  the conformance verdict**; a non-zero vanilla column beside a zero xoshiro
  one is noise, not a bug. (Early on, before the structural bugs were fixed,
  `FD_RNG=xoshiro` lowered nothing — the transcripts desynced structurally
  before RNG mattered. That is no longer the case: it is now what pins most of
  the corpus at byte-exact.)
- These scripts are **best-effort**: CASA walkthroughs target the original
  releases, and some commands differ in the ADRIFT 5 remakes. Where both
  engines reject a verb identically that is not counted as a Scarier bug.
- Some corpus games are legitimately **unfinishable in their shipped build** —
  authored-data breakage that FrankenDrift reproduces exactly (Penrhyn below,
  Sorry For Your Loss, The Awakeners, Tempus Fugit). Their scripts stop at the
  authored dead end on purpose.
- Source walkthroughs and provenance: `test/adrift5/downloaded/`,
  `test/adrift5/games.manifest.tsv`.

## Per-game write-ups

Unlike the v4 corpus, a5 games have no per-game `notes/<Game>_walkthrough.md` —
the directory holds only Symphonica64's route doc. So for the games below these
sections are the **sole record** of the route and of the engine fixes deriving
it forced, and they are kept verbatim.

### Alien Diver (Daza, ADRIFT 5) — WON (repair ship + take off)

`test/adrift5/goldens/AlienDiver_walkthrough.txt` (85 commands) reaches `*** You have won ***`
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
`./test/adrift5/harness/a5run_dump test/adrift5/games/AlienDiver.blorb test/adrift5/goldens/AlienDiver_walkthrough.txt | grep "Well Done"`.


### All Through the Night (Daniel Saults, 2013) — Ending C: Salvation, MATCH 0|0

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

### An Adventurer's Backyard (Nick Gauthier, 2015) — WON 25/25 MAX, MATCH 0|0

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

### Return to Castle Coris (Larry Horsfield, 2020) — WON 400/400 MAX, MATCH 0|0

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

### Edith's Cats (Bunkphor, 2016) — ★ WON, MATCH 0|0

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

### Penrhyn: The Burning Sky (Rob Sherwin, 2020) — UNFINISHABLE (authentic), MATCH 0|0

A tutorial-driven period drama. Ralph runs morning errands round Gwylanne (bank
a money bag, drop an envelope at Codger's, meet Violet at the gardens), his
father is attacked at the forge (chloroform, ambulance, hospital), and Ralph
must chase the lead into the walled **Hovel District** to find his brother. The
37-command walkthrough drives the *entire* authored chain — Act 1 Scenes 1–3 —
to the West Gate, where the game's own on-screen tutorial prints **"Just type:
show note"**.

**The game is unfinishable from there, and it is authentic authored-data
breakage — not a Scarier bug: FrankenDrift strands identically at the same
gate.** Root cause (verified in the model XML): the West-Gate exit
(Location11 → Location22) is gated on `Noteisshow=1`, set only by task
`GiveNoteTo` (`show/give note`), whose restrictions require the business note
`BusinessNo` **held** (+ inside `SlingBag` + bag worn). But `BusinessNo` ships
`<DynamicLocation>Hidden</DynamicLocation>`, and the *only* task that un-hides it
(`JumpToHove`: `MoveObject BusinessNo ToCarriedBy %Player%`) is a leftover debug
command `jump to hovel` gated on `Testingact BeEqualTo 1` — and **`Testingact`
is never assigned 1 anywhere in the game** (there are zero `Testingact = …`
actions). So the note can never be obtained, the gate never opens, and the Hovel
District, the Badger's Crown workshop, and the "To be continued…" ending
(Location42) are all permanently unreachable. `show note` fails with "Sorry, I
didn't understand that command" because with the note Hidden there is no `note`
noun in scope for `GiveNoteTo` to bind. The walkthrough legitimately terminates
at that gate (same class as Sorry For Your Loss / The Awakeners / Tempus Fugit).

**Engine fixes — the Arkell follower (dynamic On-Character).** Ralph's shoulder
raven "Arkell" is kept on the player each turn by System task `ArkellFoll2`,
which runs `MoveCharacter Arkell ToSameLocationAs %Player%` **then**
`MoveCharacter Arkell OntoCharacter %Player%` (the second overrides the first;
FD `clsUserSession.vb:1884` sets `ExistWhere=OnCharacter`). Two RNG-independent
gaps surfaced, both fixed in the `adrift5/` engine (the runtime twins of the
static On-Character loader path added for Edith's Cats, see above):

1. **`MoveCharacter … OntoCharacter` was a no-op** — the `to`-enum switch in
   `a5run_action.cpp` had no `OntoCharacter` case, so it fell through to
   "best-effort no-op" and Arkell was never placed on the carrier. Added a branch
   that sets `char_onchar[ci]` and clears `char_loc`/`char_onobj`/`char_in`
   (with FD's self/recursive-placement guard). Also clear `char_onchar` at the
   top of every *other* MoveCharacter branch, so a rider that is later sent
   elsewhere (Penrhyn drops Arkell to Location2/Hidden once he is "broken")
   stops riding — matching FD's fresh-`dest` `ch.Move`.
2. **`BeOnCharacter` restriction read the wrong field** — `a5restr.cpp` looked at
   `char_onobj` (which only ever names objects), so an on-character test always
   failed. Switched it to `char_onchar`.

Before the fix Scarier printed the default follow alt ("Arkell follows Ralph.")
and omitted Ark from room listings; FD showed the on-shoulder alt ("Arkell
follows along on Ralph's shoulder.") and "Ark … are here." After the fix the
deterministic transcript is byte-identical to FD across the whole run. The only
residual is 2 **random** courtyard-ambient lines (`xoshiro` mode = 0 hunks,
proving pure RNG noise), pinned by the committed golden. Golden
`Penrhyn_expected.txt`, wired `Penrhyn|Penrhyn_The Burning Sky_v2.blorb|0|0`.
Full suite after the change: **115 MATCH / 11 DIVERGE, 0 FAIL**; a5 unit tests
green.

### Symphonica64 — ★ WON 74/74, MATCH 0|0

Route: `notes/Symphonica64_walkthrough.md`. What follows is the engine-fix
history the derivation forced.

**Static-follower loader gap FIXED (2116 → 263).**
The largest a5 game in the corpus (415 rooms, 263 objects, 156 characters, 613
tasks). The godmode-free golden collects all 74 members of the "Scores" group,
gives the Gadget to Sir Stephen for the Ticket, and wins via the `49587948EFG`
GoToExtens code (`*** You have won ***`). Wired `Symphonica64|symphonica.blorb|0|263`
against a strict dotnet-free golden (`Symphonica64_expected.txt`), so **vanilla
= 0 hunks (MATCH)** and the save/restore self-check is **OK**.

The `xoshiro` column WAS a 2116-hunk DIVERGE, of which **1956 (89%) were the
line "1980s Barry Leitch is following you."** — present in FD, absent in Scarier.
This turned out NOT to be a rendering-phrasing difference (both engines share the
identical `char_here_desc` render path, and the *dynamic* followers Rory / Konkey
Dong, moved at runtime via `MoveCharacter … OntoCharacter %Player%`, already
resolved correctly). Barry is a **static** `CharacterLocation="On Character"`
follower whose `CharOnWho` is the *variable* `"%Player%"`, and the static-model
loader (`a5state.cpp`) stored that literal verbatim. Every downstream carrier
lookup (`a5state_character_index("%Player%")` → −1) then dropped Barry from every
room listing, so his authored `CharHereDesc` ("%CharacterName% is following you.")
never printed. **Fix:** normalise `%Player%` → `st->player_key` when decoding the
static On/In-Character start state (`resolve_carrier`), mirroring the object
loader's existing `%Player%` normalisation — a pure loader gap the character
branch never received. Barry now resolves to the player's room and the line
appears 1956× exactly as FD does. Whole-suite re-run confirmed **no other game
changed** (Symphonica is the corpus's only static `%Player%`-carrier follower;
Edith's Cats used the literal key `Player`, not the variable). The residual 263
hunks are non-follower RNG/prose noise: Rory's `OneOf` taunt variants ("being
generically vicious" / "glaring at you angrily" / "chewing your toe", which
System.Random can't align to xoshiro), river-bank description variants,
Kickstarter backer flavour text, and inventory-line ordering.

**Engine fix — carrier state now persists across save/restore.** Wiring this
game surfaced a genuine ScarierExt save bug: the save path (`save_scarier_body`
in `a5run.cpp`) serialised `OnObj`/`In` for each character but **not**
`char_onchar` (the carrier CHARACTER key set at runtime by `MoveCharacter …
OntoCharacter`). For a player-follower `char_loc` is NULL and the room resolves
through the carrier, so after an `A5_SAVE_AT` round-trip every follower vanished
from room descriptions (first observed at Rory/Konkey Dong, a 1060-hunk
self-check DIFF). Added a symmetric `OnChar` element to save and restore
(`restore_scarier_body`); an absent element `intern_key(NULL)`-clears it,
correctly un-following a character that stopped riding since the save. After the
fix the save/restore round-trip is byte-identical (0 hunks) and the game still
wins. The FD-format save path (`save_fd_game`) still writes such followers as
`Hidden` — out of scope; the ScarierExt path is what the self-check and
Spatterlight autosave use.

**Symphonica64 residual FIXED (263 → 0): now MATCH 0|0.** The 263-hunk xoshiro
residual (diagnosed in the pruned `TODO_symphonica_schtick_ordering.md`) came
apart into three distinct engine gaps, each fixed against the runner sources:

1. **Event-fired tasks now defer AggregateOutput completion draws to the end of
   their own attempt** (`attempt_event_task_impl`, `a5run_events.cpp`). An
   event/walk/LocationTrigger-fired task is its own `AttemptToExecuteTask` in
   the runner (`bChildTask=False`, clsEvent.vb:389 / clsCharacter.vb:1630 /
   clsUserSession.vb:3424), so its aggregate responses Display — and expand
   their `<# OneOf #>`s — at the END of that attempt (vb:782,851-856), after
   every SetTasks-Execute'd sibling has run its eager restriction rands.
   Scarier rendered them inline in `run_task`, so Symphonica's per-turn
   Schtick1 children (After + aggregate `<# OneOf #>` taunts for Rory, the
   exposed Karateka, Sir Mart et al.) drew BEFORE the last sibling Bystander1's
   `rand(1,100)` restriction, phase-shifting the whole stream once per turn —
   the bulk of the 263. The command path's existing `display_defers` sink is
   now armed per event-task attempt and partially flushed
   (`a5run_flush_display_defers_from`) at its end.

2. **Deferred `<#..#>` draws flush in TEXT order, not push order**
   (`a5run_flush_display_defers_from`, `a5run_action.cpp`). The runner's
   Display expands one response at a time: ReplaceFunctions first, then
   ReplaceExpressions over the fully OO-substituted text LEFT TO RIGHT
   (Global.vb:523-524, 510-516). A `<#OneOf#>` nested inside an OO description
   read — Rory's `CharHereDesc` taunt inside ZoneFantas's
   `Player.Location.Description` completion — therefore draws AFTER the
   textually-earlier top-level teleport `<#OneOf#>`, while Scarier's sink
   received the nested one first (the OO pass runs before the expression
   scan). The flush now re-orders the untagged sexpr entries among themselves
   by sentinel text position; `\001` (function-pass) and `\002` (room view)
   entries keep their push slots, which already mirror the runner's pass order.

3. **A SelectionOnly list filter with a `(False)`/`(0)` argument inverts**
   (`oo_prop`, `a5expr.cpp`). `%Player%.Held(False).Isscore(False).List(..)` —
   the game's inventory line for NON-score items — kept the score-carrying
   members regardless of the argument. ReplaceOOProperty drops a has-property
   member on `"false"/"0"` (Global.vb:971) and keeps a lacks-property member
   (vb:1040); Scarier now does the same.

Golden re-blessed, MAP re-wired `Symphonica64|symphonica.blorb|0|0`. Full suite
after the three fixes: **116 MATCH / 11 DIVERGE, 0 FAIL** — no other game moved
in either column, save/restore self-checks all OK, a5 unit tests and the v4
corpus green.

### Dinner Plans — ★ best ending, MATCH 0|0

Nick Fisher (as "PC-Fan"), 2014; an AIF dating sim, eight rooms, 273 tasks, no
walkthrough anywhere. Route: `goldens/DinnerPlans_walkthrough.txt` (100
commands, prose header included), wired `DinnerPlans|Dinner Plans.taf|0|0`.
Like TASP the game is explicit, so its walkthrough and golden are gitignored
and stay local-only; the row runs only where they exist.

The game is one variable, `emilyhappy`, plus a day counter. Four evenings run
the same loop — dress the living room, `order pizza`, `answer door`, talk to
Emily, tip her, `eat pizza` (which sets `gotobar`), drive to Max's Tavern and
debrief Anna. Going south out of the bar is the day boundary: it bumps
`gameday`, drops you home, and clears `toldannaaboutemily` / `askannaaboutstrategy`,
so that pair has to be redone nightly (and night 2 additionally gates on
`ask anna about music`). On day 4 the leave-the-bar task fires
Endgameche / Endgamebad / Day4intro in order: `emilyhappy <= -22` is the creep
ending, `< 23` the losing one, `>= 23` buys the last evening; the best ending
(Endgamebes — Anna walks in, the threesome, the scheme explained) needs
**`emilyhappy >= 32`**, and `fuck emily` itself needs `> 30`.

**32 is the ceiling, and it is only just reachable.** Every once-only source —
three generous tips, the hockey gear, the guitar on its stand, playing it
plugged in, handing it to Emily, the repair manual, fixing the car, and Emily's
job / sports / game / bass / band topics — totals **29**. The slack comes from
`WatchSport`, which is Repeatable with no once-per-evening guard, so each
`watch sports` on night 2 is another +1; four of them make exactly 32. The
route deliberately skips `ask emily about pizza bella` (+1): its keyword set
overlaps Topic1's and FD's Dictionary enumeration order picks the other topic
there — a measured .NET artifact, so the phrase is avoided rather than have the
golden encode the divergence.

**Engine fix — a topic reply is REAL output, so its `<DisplayOnce>` segments
retire** (`emit_topic_conv`, `a5run_conv.cpp`). Scarier rendered conversation
replies under the ambient `marking_display` (0, i.e. the runner's
`bTestingOutput = True` "am I allowed to say anything?" probe), so a topic's
DisplayOnce alternatives never got marked used and every re-ask replayed the
first description forever. `clsUserSession` runs the chosen reply through
`Display` with `bTestingOutput` **False**, so those segments do retire and the
topic advances. Fixed by scoping the emit in an `a5_mark_guard mg (st, 1)`.
Dinner Plans leans on this hard — each of Anna's four topics carries one
description per evening, keyed only on the previous one having been used up, so
before the fix the bar debrief was frozen on night 1 (20 xoshiro hunks). After
it the whole 100-command run is byte-identical to FrankenDrift in both columns.
Whole-corpus re-sweep after the change: **no other row moved**, including all
23 golden-less rows re-checked individually against FD at their MAP baselines.

### Let Me In — loader/parser smoke row, MATCH 0|0

BBBen, 2015. **Not a walkthrough**: the row loads the game and issues six
neutral commands (`look` / `x me` / `i` / `score` / `wait` / `look`). Wired
`LetMeIn|LMI_v100.blorb|0|0`, MATCH on the first pass, no engine work needed.

Worth a row because of its shape rather than its content: **490 tasks and 402
variables in a single location**, the heaviest text-matching task table in the
corpus, every task a wildcard/optional-group pattern competing on priority (346
keyed). Even neutral input exercises the loader, the matcher's priority
ordering across all of them, and the no-match fallback.

**No golden is committed** — with no `_expected.txt` the harness takes the
FrankenDrift differential branch, so no transcript of the game enters the repo.
The row needs `FD_ROOT` and is SKIPped by `--golden-only`, which is the trade.

### Four adult games deliberately left unwired

The last manifest rows without walkthroughs are four explicit adult titles.
Deriving a route means playing the sexual content to an ending and committing a
transcript, and three of the four are ruled out on content grounds rather than
technical ones. Recorded here so the gap reads as a decision:

* **Evil on Queen Street** — its opening scene, printed on load before any
  command, sexualises a character framed throughout as the player's
  high-school-aged sister. Every transcript of the game *is* that passage, so
  even a smoke row only files it as a fixture to re-bless and diff.
* **PervertActionCrisis** — explicit, and the model carries 178 `student` /
  184 `school` / 83 `teacher` hits: a school setting with students.
* **PAFv1_2** (*Pervert Action: Future*) — explicit, same author and series,
  and no age-of-consent statement anywhere in its intro. Unstated ages.

All four *load* cleanly, which is the part worth knowing: `a5dump` reads the
51 MB PAC blorb to 2.7 MB of model and the 253 MB PAF blorb to 2.9 MB in
0.12 s. The blorb path is not what is missing. Let Me In is the one that could
carry a row — its intro states all characters are over the age of consent, the
setting is a fantasy tower, no school and no family relationship — and what
made its *route* undesirable (the game is a deception engine: the win is talking
past a locked door with `IAmThePrince` / `IAmYourDaddy` / `IAmAHumanBeing` /
`TrustMe`, i.e. manufactured consent) is precisely what the smoke row does not
traverse.

## Synthetic conformance probes (test/adrift5/probes/, imported 2026-08-08)

Feature-targeted probe scripts — 23 `Probe*` MAP rows over 19 synthetic games,
17 of them from the **adrift-5-rs test suite** plus our own `undo_after_end`
and `delrt` (some games carry several rows, e.g. the four `lifecycle` ones).
The sources are
committed under
`test/adrift5/probes/src/*.xml` and compiled offline into the committed
`test/adrift5/probes/*.taf` by `make -f Makefile.headless a5probetafs` (which embeds
`test/adrift5/probes/tools/libraries/StandardLibrary.amf` via
`test/adrift5/probes/tools/xml2taf.py` — decompression/obfuscation logic matching
FrankenDrift's FileIO.vb).  Each `* section` of the original `.regtest` scripts
became one `test/Probe*_walkthrough.txt` (bare `>` waitkey keypresses and
`quit` dropped — both are no-ops/harness-level in the transcript harness).
Unlike the real-game corpus these never SKIP: the tafs are in-tree, so the
probes run everywhere, including without FrankenDrift for the golden-backed ones.

adrift-5-rs's own expected outputs were used only as a third opinion — blessing
went through the usual FrankenDrift differential.  Wiring surfaced **12
divergence groups**; all of them are now closed or accounted for, leaving
**19 of the 23 rows at MATCH 0|0** and every probe passing the save/restore
self-check.  The four that remain are ProbePopups (8 → 4 hunks),
ProbeTaskActions (1 vanilla RNG-noise hunk), ProbeVariables (`%Version%`) and
ProbeUndoAfterEnd — see the rows below.  ProbeRandomness is fully aligned under
`FD_RNG=xoshiro` (its 3 vanilla hunks are pure System.Random stream noise), so
it is golden-backed at 0|0.  ProbeUndoAfterEnd (added 2026-08-09) is the one row
whose budget is **not** ours: it pins a Runner bug (adrift.co 19196) that Scarier
does not reproduce, so its 4 xoshiro hunks must stay exactly 4 — a drop to 0
would mean Scarier had *acquired* the bug.  ProbeDel / ProbeDelrt (added
2026-08-11 with `<del>` support) briefly carried 0|3 and 0|5 for a reason that
was also not ours — FrankenDrift.Headless dropped `<del>` entirely — and are
back at 0|0 now that `EmitHtml` handles the tag; see the `<del>` section below.

| Probe | v|xo | diagnosis |
|---|---|---|
| ProbeAmbiguity | 0\|0 | **FIXED** (was 3\|3): the `Which key?` clarifier (`blue`) never resolved because run_remembered's force_name/force_key pin only applied to singular `%object%`/`%item%`/`%character%` references — the library's `get/drop %objects%` commands take the plural-branch singular path in resolve_refine, which `continue`d before the pin.  The pin now applies there too (a5run_ref.cpp). |
| ProbeDel | 0\|0 | Came with the `<del>` implementation (PR #148, `a5text.cpp` `sb_del_glyph`).  Was 0\|3 for a week: FD.Headless dropped `<del>` (`[abc]`/`[ax]`/`[a\n]` where the real Runner gives `[ab]`/`[a]`/`[a]`), **not** ours.  `EmitHtml` now handles the tag. |
| ProbeDelrt | 0\|0 | Our own probe (`src/delrt.xml`, 2026-08-11), written to be read at **load** — cases 1–7 live in the Introduction — so it could be measured directly in the genuine run500.exe under Wine.  Was 0\|5 for the same FD.Headless reason; case 8 (a `<del>` at the head of a *later* message) was additionally a real Scarier gap, closed the same day by the turn-level `A5_DEL_MARK` pass.  See the `<del>` section below. |
| ProbeEvents | 0\|0 | **FIXED** (was 1\|1; the old "LookText timing at start" diagnosis was wrong — the real divergence was at event END: Scarier kept showing the SetLook text after the event expired).  FD keeps a per-event `stackLookText` and `LookText()` returns entries only while `Status = Running` (clsEvent.vb:20/132-153); ViewLocation loops ALL events in model order appending each gate-passing LookText (clsLocation.vb:141-144).  Scarier's look stack entries now carry their owning event key, and view_location iterates events gated on a `st->ev_running` callback into the run layer; the key is persisted in the save `<Look>` block. |
| ProbeHiPriTask | 0\|0 | **FIXED** (was 1\|1): FD's BeInPosition restriction (clsUserSession.vb:4902-4915) reads the raw `CharacterPosition` *property* gated on HasProperty — NOT the Position getter that defaults to Standing.  The library-injected default Player has no such property (FileIO only adds explicit `<Property>` nodes; SetProperty on an absent character property no-ops), so `MustNot BeInPosition Standing` passes and the passing `stand` task beats the failing one.  Scarier's BeInPosition now uses the property view (a5state_entity_prop) instead of the Standing-defaulted char_position cache (a5restr.cpp). |
| ProbeLifecycleRestart | 0\|0 | **FIXED 2026-08-09** (was 3\|3: walk enter/exit announcements missing after a walk restart — Scarier stopped the walk dead instead of restarting it).  The control re-trigger guard was unfaithful: FD's `Not task.Children(True).Contains(w.sTriggeringTask)` (clsUserSession.vb:873/894) blocks a control only when the walk's triggering task is a DIRECT Specific-override child of the completing task — clsTask.Children (clsTask.vb:336) is one level down and does NOT include the task itself.  Scarier additionally blocked `triggering_task == task_key` (and recursed through grandchildren), so TaskRestartPatrol's Stop control fired, recorded itself as the trigger, and then blocked its own Start control — the Stop→Restart upgrade (clsCharacter.vb:1366-1367) never happened and the walk just stopped.  ctrl_retrigger_blocked now mirrors FD exactly (a5run_events.cpp). |
| ProbeMap | 0\|0 | **FIXED** (was 1\|1): a failed `MoveCharacter ... InDirection` now Displays the blocking restriction's message the way the runner does (clsUserSession.vb:1748 `Display(sRestrictionText)`) — *immediately*, so it lands **before** the task's buffered CompletionMessage in the output stream (`The door is closed.  Revealed...`).  Implemented as an immediate-Display splice point in run_task (a5run_action.cpp imm_display).  (The map raster itself is covered by `a5maptest`, below.) |
| ProbePopups | 4\|4 | Was 8\|8.  Fixed: `%PopUpChoice[...]%` is now left **unevaluated** like FD (its MsgBox throws off-Windows, ReplaceFunctions catches, token survives verbatim — Global.vb:2278/2483; crucially it must never consume a script line, which desynced Beagle2's whole transcript), and a *bare* `%PopUpInput%` (no `[args]`) also stays verbatim instead of eating a line.  Remaining 4 hunks are one cosmetic artifact: FD's expression tokenizer re-serializes the unevaluated function without spaces after commas (`"Pick a colour","red","blue"`) where Scarier keeps the source text's `, ` spacing. |
| ProbeRandomness | 0\|0 | Golden-backed MATCH.  Vanilla FD residue (3 hunks: walk RandomKey wing pick, `roll` RAND value) is pure RNG-stream noise — byte-aligned under xoshiro. |
| ProbeRefCapture | 0\|0 | **FIXED 2026-08-08** (was 4\|4).  (a) `%location%` command refs now resolve typed text to location **keys** via FD's word-optional short-description matcher (each Split token contributes an optional group, anchored regex, first model-order hit wins — clsLocation.vb:97 + clsUserSession.vb:5459), with the miss path falling through to the Location Must-Exist second chance; `%item%` gained the same location arm as its third fallback (vb:5654).  (b) Numbered reference *functions* `%location1..5%`/`%item1..5%` resolve to the bound key via oo_firstkey/eval_function (unbound → verbatim numbered token), and the leave-verbatim path now rewrites **all seven** singular aliases to numbered form (object/character/location/direction/item/text/number — Global.vb:1754), not just `%object%`.  (c) Replicated FD's `%characters%` crash-abort: "characters" is missing from ReferenceNames() (Global.vb:473) so NewReferences is ReDim'd short (clsUserSession.vb:275) and the group handler throws IndexOutOfRange; GetGeneralTask's Catch (vb:6118) returns Nothing, discarding all candidates → NotUnderstood ladder.  A regex-matched command carrying a "characters" ref now aborts the whole general-task scan the same way. |
| ProbeRestrictions | 0\|0 | **FIXED 2026-08-08** (was 10\|10).  Added the missing restriction quantifier arms in a5restr.cpp: object `BeInGroup`/`BeVisibleToCharacter` now take AnyObject/NoObject (clsUserSession.vb:4193/4338), character `Exist` AnyCharacter (vb:4600), `BeHoldingObject` AnyCharacter = `IsHeldByAnyone` (recursive through held containers, clsObject.vb:737), `BeWearingObject` AnyCharacter = `IsWornByAnyone` (direct only, vb:756), stance ops take `TheFloor` (= the position with ExistWhere AtLocation, vb:4834), and `HaveRouteInDirection` takes `AnyDirection` (loops DirectionsEnum, vb:4608).  New `pass_item` evaluator for the previously always-pass `Item` restriction type (three-way objects→characters→locations lookup + BeType/Exist/HaveProperty/..., vb:4972-5060) — `checkitem bob` now fails `ReferencedItem Must BeType Object` with `Not an object item.`  And ValueList property values now store the label's mapped **integer** like FD's clsProperty (`Very heavy` → 81; converted at model load in a5_fix_valuelist_props + in the SetProperties action), which makes the StandardLibrary's `MaxWeight %Player% Must GreaterThanOrEqualTo ...Weight.Sum...` carry restriction real: the huge boulder is now refused with `The huge boulder is too heavy to carry at the moment.` |
| ProbeTaskActions | 1\|0 | **FIXED 2026-08-08** (was 15\|14).  (a) `%LocationOf[object]%` now enumerates clsObject.LocationRoots (clsObject.vb:460, new a5state_object_location_root): roots resolve through container/on/part-of chains and through an At-Location carrier (held/worn/part-of-character), instead of the old directly-placed-only test that rendered `loc=` empty; `%LocationOf[character]%` resolves through carriers via a5state_character_location_key and reports `Hidden` for a hidden character (clsCharacter.vb:1790).  (b) `MoveObject EverythingAtLocation` no longer sweeps **static** objects (clsUserSession.vb:1484 `Not ob.IsStatic`) — Bob's furniture-sync moves stopped chasing mis-swept furniture.  (c) character `BeInGroup` restriction was entirely unhandled (fell through to pass): added per clsUserSession.vb:4779.  (d) `MoveCharacter ToSwitchWith` (neither = player) now replicates FD's actual net effect — the swap exchanges the Location refs but the epilogue `ch.Move(dest)` (dest pre-filled from the mover's ORIGINAL place, vb:1727/1902) moves it straight back: k2 lands at k1's spot, k1 stays put.  (e) a pure Greet no longer double-prints the intro topic: FD re-finds the same topic in the main lookup but AddResponse (vb:1296) keys responses by message text and merges the duplicate.  Remaining 1 vanilla hunk = the `ToLocationGroup` single RandomKey draw (RNG-stream noise, byte-aligned under xoshiro). |
| ProbeUDF | 0\|0 | **FIXED 2026-08-08** (was 4\|4).  UDF evaluation now mirrors FD's EvaluateUDF (Global.vb:1653): args split SplitArgs-faithfully (bracket-depth commas, empties dropped, no trim — vb:1623), each arg that sniffs as an integer expression (digit-op-digit where the "op" class `[+-/]` is really the char *range* `'+'..'/'`, i.e. `* + , - . / ^`) is folded through EvaluateExpression keeping the original on failure, then substituted into the **raw** body *before* evaluation, and the result is re-expanded via ReplaceFunctions with `<#..#>` expressions GUID-protected — so `<# %n% * 2 #>` sees `21` and yields 42 instead of unbound-0, and `3 + 4` folds to `7` in the echo.  The `%object%`-arg full-name render and the char-arg refusal came free from the ProbeRefCapture fixes (numbered-alias rewrite + `%characters%` abort). |
| ProbeUndoAfterEnd | 0\|4 | **DELIBERATE divergence -- Scarier is the correct side.**  Pins our behaviour on [adrift.co bug 19196](https://www.adrift.co/bug/19196) ("UNDO fail to handle events correctly after game end", Critical, open since 2018): after an UNDO at the post-game prompt the real Runner starts stopped events and stops running ones.  Cause (confirmed in FD): `clsEvent.NextCommand` -- the deferred Start/Stop/Pause/Resume an EventControl sets while `bEventsRunning` is False -- is private clsEvent state and is **not** in `clsGameState` (clsState.vb:136 saves only Status / TimerToEndOfEvent / iLastSubEventTime / LastSubEvent, and walks only Status + timer), while the ending turn never ticks to consume it (`TimeBasedStuff` exits once `eGameState <> Running`) -- so the undone task's event controls fire on the first tick after `States.SetLastState` resumes play.  Scarier's undo point is the full lean `a5run_save_blob` (a5run.cpp `a5run_snapshot`), which carries `next_command`/`just_started`/`when_start`/`triggering_task`/`se_ft`/`length_value` too, so `a5run_undo` rolls back cleanly: after the undo RUNNER and PAUSEE keep beating, SLEEPER stays dormant and RESUMEE stays suspended, where FD stops RUNNER, pauses PAUSEE and beats SLEEPER + RESUMEE (3 of the 4 hunks).  The 4th hunk is the same family one level up: the text replayed after "Undone." is FD's live `sTurnOutput`, which `clsGameState` does not roll back either, so FD echoes an older turn's output where Scarier echoes the undone turn's (`undo_turn_text`, parallel to the undo stack).  Same class as the known Turns quirk -- see the post-game guard notes. |
| ProbeVariables | 1\|1 | Only `%Version%` remains: `5036.6` (Scarier mimics the real Runner's 5.0.36.6) vs FD's own assembly version `080.0` — every interpreter reports its own version, and the adrift-5-rs regtest accepts any via `/Version=.+`.  FIXED 2026-08-08: `%ListCharactersOn/In%` now the bare `CharacterHashTable.List` name join (Global.vb:2159/2166 — the here-desc form belongs to OnAndIn only), `NumberAsText` is a faithful NumberToString port (Global.vb:2563, incl. the drop-the-digit-before-the-decimal-point quirk), and `%Release%` defaults to 1 when the .taf carries no Babel release metadata (Babel.vb:354). |
| ProbeWalk | 0\|0 | **FIXED 2026-08-08** (was 1\|1).  `You set off for East Wing.` — the set-off message's `%location1%` now renders: the typed destination resolves to a location key (the ProbeRefCapture location-matcher fix) and the numbered reference function substitutes it, so the destination name lands in the message instead of an empty string. |

Not yet wired: the three regtest-less sources (`rich_text.xml`,
`text_set_variable.xml`, `timed_events.xml` — no command scripts came with
them).

The eight `Samples/*.taf` regtests (Cloak, Conversation, Doors, DarkRoom,
JackAndBeanstalk, Notebook, TorchBattery, BlockingExits) are wired as
`Sample*` rows.  The game files ship with the ADRIFT 5 developer
distribution, not with the suite; they were recovered by extracting the
"ADRIFT Installer.msi" payload with msiextract (msitools) into
test/adrift5/games/ — no Wine install needed (the same MSI also yields
run500.exe/dev500.exe, the real 5.0 Runner/Developer, kept in mind for
future Runner arbitration).  Result: **8 golden MATCHes**:

| Probe | Budget | Diagnosis |
| --- | --- | --- |
| SampleConversation | 0\|0 | FIXED 2026-08-09 (was 2\|2: `She looks up` vs FD's first-meeting `An old lady looks up`, and `an old lady` vs FD's definite `the old lady` after the chat).  Four coordinated bDisplaying-model fixes: (1) the PronounKeys ledger clears unconditionally each turn — FD's init PrepareForNextTurn (vb:283) runs AFTER the title/intro/first-room displays (vb:227-229), so no game-start entry survives into command 1; (2) Display-time room views (render_look_marked) run with intro_active=1 — the aggregate Look renders inside Display; (3) update_seen mirrors PrepareForNextTurn's Introduced reset for characters the player can no longer see (clsUserSession.vb:3794-3797), so a re-encountered NPC is indefinite again; (4) the Look task's pre/post-action probe renders also run with intro_active=1 — FD's probes run inside Display (vb:1177-1203), the first probe records a PronounKeys entry, the second pronoun-replaces the name ("she"), the probes differ, and the response pins to the FIRST probe's text (vb:1200), which must therefore be a bDisplaying render ("the old lady"). |

Conversion footnote: `> @ comment` lines in the regtests are adrift-5-rs
harness-level comments, dropped like `quit` (feeding one through made
Scarier and FD both parse it as a game command).  The suite's ninth Samples
regtest (persistence.regtest, Cloak-based save/restore/undo) was NOT
converted: the harness's own A5_SAVE_AT round-trip self-check covers that
ground, and the transcript harness has no in-game save-dialog channel.

## `<del>` — measured in the real Runner (delrt probe, 2026-08-11)

`<del>` (issue #142, implemented by PR #148 in `a5text.cpp`: `sb_del_glyph` plus
the `del` arm of the tag dispatch) is the one a5 feature the FrankenDrift
differential **cannot** arbitrate: `FrankenDrift.Headless`'s `EmitHtml`
(`Program.cs`) handles only `br` and `cls` and drops `<del>` on the floor, and
the two FD front ends disagree with each other anyway — `GlkHtmlWin.cs:270`
deletes only from the pending `current` StringBuilder (which is cleared at
*every* tag) and then falls back to Gargoyle's `garglk_unput_string`, while the
Eto `AdriftOutput.cs:183` does a whole-window `Buffer.Delete`.

So it was measured directly instead, in the genuine **ADRIFT 5.0.36 Runner
(`run500.exe`) under Wine** (see the Wine harness notes), using
`test/adrift5/probes/src/delrt.xml` — every case sits in the *Introduction*, so
the whole probe renders at load and can be read off the Runner window with no
gameplay.  Result: **`<del>` deletes one character from the Runner's whole
accumulated turn buffer.**  It is not scoped to a style run, a line, or a
message:

| # | markup | run500.exe 5.0.36 | Scarier |
|---|---|---|---|
| 1 | `1[abc<del>]` | `1[ab]` | same |
| 2 | `2[a<b>x</b><del>]` | `2[a]` | same |
| 3 | `3[a<br><del>]` | `3[a]` | same |
| 4 | `4[a\n<del>]` | `4[a]` | same |
| 5 | `5[a\n\n<del>]` | `5[a\n]` | same |
| 6 | `websites:\n\n<del>https://one\n \n<del>http://two` | `websites:\nhttps://one\n http://two` (leading space kept) | same |
| 7 | room LongDescription starting with `<del>` | room name and description JOIN: `Delete Lab7ROOMDESC…` | same |
| 8 | msg A `8[aZ`, then an Execute-Task msg B `<del><del><del>]END8` | `8[a]END8` — eats the two pSpace join spaces **and** the `Z` from the previous message | same (since 2026-08-11) |
| 9 | msg A `9[x\n\n<del>` (newline survives), then msg B `]END9` | `9[x\n  ]END9` — pSpace join spaces ADDED: the raw text ends in the tag's `>`, not vbLf | same |
| 10 | msg A `10[a<br>`, then msg B `]END10` | `10[a\n  ]END10` — a trailing `<br>` also gets the join spaces | same (ps_mark_trailing) |
| 11 | msg A `11[a\n` (bare source newline), then msg B `]END11` | `11[a\n]END11` — no join spaces | same |

Case 8 was the **cross-message gap**, fixed 2026-08-11 along exactly the
`<cls>` pattern that sits ten lines above the `del` arm.  `sb_del_glyph` runs on
the per-message buffer inside `a5text_render_plain`, so when the current
fragment holds no glyph the backward walk hits `i == 0` and can delete nothing —
the shape #142 calls out ("kinda common, especially to undo a paragraph break")
and the one documented at `adrift5/a5run_resp.cpp:269` (WW2 Elevator Escape's
`"(standing up first)"` → ALR → `<del>`, a message whose entire content renders
to marks).  It now leaves an **`A5_DEL_MARK`** (`\037`; `\004` is taken by the
display-defer sentinel) that `sb_resolve_del` applies to the accumulated turn
buffer in `finish_turn`, immediately after `sb_resolve_cls` so a `<cls>` that
wiped the marker wins.  `sb_del_glyph` moved to `a5sb.cpp` and took a `from`
offset for it.  Second real customer besides the probe: TheEuripidesEnigma's
`You press button 7 <del>on the boombox…` — the completion message starts with
`<del>`, and the Runner eats one of the two pSpace join spaces.

(Related, RESOLVED 2026-08-11 — measured, not a bug: the unconditional
`sb_putc (&sb, A5_ALR_MARK)` after a delete does hide a surviving newline from
`sb_pspace`, which tests only the final byte — but that mask IS the Runner's
behaviour, and the once-proposed "walk back over zero-width sentinels the way
`sb_resolve_cls` does" fix would have introduced a divergence.  `Global.pSpace`
(Global.vb:568) tests `sText.EndsWith(vbLf)` on the RAW accumulated text, tags
included, so any message whose source ends in a tag's `>` gets the two join
spaces even when the last thing the player sees is a newline.  Probe cases
9–11 (below) measured it in run500.exe: `9[x\n\n<del>` + `]END9` renders
`9[x\n  ]END9` — join spaces added despite the surviving newline, exactly what
the ALR-masked final-byte test produces.  A trailing `<br>` (case 10) also
gets the spaces — `ps_mark_trailing`'s `A5_PS_MARK` already handles that — and
only a bare source newline (case 11) suppresses them.  `sb_pspace` now carries
a do-not-fix comment citing these cases.)

Cases 1–7 are byte-for-byte right, which is what re-blessed ten corpus goldens
on 2026-08-11: nine of them are Larry Horsfield's credits block (case 6 verbatim
— `…websites:\r\n\r\n<del>https://lazzah.itch.io\r\n \r\n<del>http://www.adrift.co/games`,
one newline eaten per `<del>`, stray leading space kept), plus QuestGiver (a
blank line removed from the quest listing, ×33) and IlluminaDansk (an
`AppendToPreviousDescription` alternative whose text starts with `<del>`, eating
one of the two join spaces).  Their **xoshiro** budgets were raised at the same
time — 0|1 IlluminaDansk, 0|2 ThingsThatGoBumpInTheNight / BugHuntOnMenelaus /
AoS / FinnsBigAdventure / MagorInvestigates / Xanix / BookOfJax, 0|5
MaroonedOnMazoomah, 0|33 QuestGiver, 0|3 ProbeDel, 0|5 ProbeDelrt — every one of
those hunks being FD.Headless's missing `<del>`, not ours.

`EmitHtml` has since **learned the tag** (`FrankenDrift.Headless/Program.cs`, our
`scarier-headless` branch): output is held in a static `_pending` StringBuilder
and flushed just before the next prompt, so `<del>` can reach back over message
boundaries the way the Runner's Display buffer does, and text runs decode their
entities inline so a following `<del>` removes a decoded character rather than
half an `&amp;`.  All twelve budgets are back to **0|0**.

## a5maptest — auto-map raster regression (map.taf)

`test/adrift5/harness/run_a5_maptest.sh [-b]` renders six views of the hand-authored map
stress game (`test/adrift5/probes/src/map.xml` → `map.taf`, 15 rooms) with
`test/adrift5/harness/a5map_dump` (build: `make -f Makefile.headless a5map`) and sha256s the
PPMs against `test/adrift5/probes/map_golden.sha256`.  These goldens are
**self-blessed** (FrankenDrift has no map), verified by eyeball:
`sips -s format png /tmp/a5maptest/<view>.ppm --out /tmp/<view>.png`.  The
views cover: initial seen-rooms state, full-map reveal, a `Hide`den room, an
attic level change, duplex/skewed/one-way/no-link/self-loop connectors, and a
restriction reveal.

Engine fixes the suite forced:

- **`Hide=1` rooms** (`<Hide>` on a Location) are parsed (a5map.cpp) and
  skipped by the box renderer (mapdraw.cpp) — the runner never draws them.
- **Stale per-turn route memo at draw time**: the runner draws its map only
  after PrepareForNextTurn has emptied dictHasRouteCache/dictRouteErrors
  (clsUserSession.vb:5179), so an exit un-blocked *during* the turn (map.xml's
  reveal task fails East while the door variable is 0, then sets it to 1) is
  re-checked fresh by the draw.  Scarier clears the memo at next-command time
  — transcript-equivalent, but stale for between-turn draws — so both map
  entry points (`gsc_a5_map_view` in os_glk.cpp and a5map_dump) now call
  `a5restr_route_cache_clear` before drawing.  The `reveal` view's
  Cellar–Door Room connector accordingly draws as a live route (solid), not
  the mid-turn blocked result.
