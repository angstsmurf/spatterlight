# The Plague – Redux — walkthrough

- **Engine:** ADRIFT 4 ("The Plague - Redux.taf", 4.0 format; author contact
  in-game is `advent@turntopage.com`).
- **Result:** ★ **WON.** The game is **winnable as shipped**. An earlier
  verdict here ("UNFINISHABLE, dead-ends at the water vending machine with
  £1.10 of £1.20") was **wrong** — reversed 2026-08-04 after a full
  playthrough to the game's only EndGame-win action (task 94, `open door` at
  the Doorway). The old route was one command short at the cubicle; see
  "The combat system" below.
- Solution file: `goldens/plague_solution.txt` (260 commands, no parser
  errors anywhere in the transcript, deterministic).
- Harness row: `plague_solution.txt|The Plague - Redux.taf|spilling zombie blood once|SCR_SKIP_WAITKEY=1`
- Source document: `downloaded/ThePlagueRedux_walkthrough.doc` (accurate
  end-to-end once the verb quirks below are applied).

## Premise

You are twenty-year-old **Stacie Wright**, waiting at Marble Arch underground
station after a night out with three friends when the plague breaks. A
Thursday-evening prologue, a long Thursday night in the sealed station, an
early-Friday-morning escape with Ray and Candice. There is **no score
system** — `score` prints the game's "notes about the game" text. The win is
the final `open door` (task 94, `ACT type=6 v1=0`).

## The combat system: dead as authored, bypassed as designed

The game's `[F] Fight / [E] Escape` system is seven identical task blocks
(`ZOMBIE 1`…`ZOMBIE 7`), and every task in every block sits at
Where/Type = 0, `ROOMLIST_NO_ROOMS` — 243 of the game's 696 tasks. SCARE and
the real run400.exe agree (probed both in isolation and in this game, see
git history of this file): a where=0 task can never match player input, so
once an `[F] Fight or [E] Escape?` prompt prints, no input answers it.
That part of the old analysis stands.

What the old analysis missed: **the author also wired weapon-based
auto-resolution into ordinary where=1 tasks**, and that path — the one the
shipped `.doc` walkthrough actually takes, pole in hand — works end to end:

| fight (.doc wording) | resolving task | mechanism |
| --- | --- | --- |
| cubicle, Women's Toilets ("kill the zombies feeding") | task 196 `* in *` | needs pole held; moves player into the cubicle and **ExecTasks task 43** `#are zombies killed 4` — the single task of the 243 the author left at where=1 (room 77). No restrictions: kills the zombies with full fight prose, awards the coins, +10p |
| café back office ("kill the zombies as they emerge") | task 373 `give * woman * water` | scripted cutscene; sets block 3's kill flag (var93) itself |
| escalator horde ("first batch") | task 59 `* d *` at Escalators | needs pole held; sets block 1's kill flag (var42) on the way down |
| disused tracks ("second batch", meets Ray) | task 72 `* d *` at Platform | sets block 2's kill flag (var49), places Ray |
| main platform ("zombies on the platform") | task 146 `* s *` at Walkway 4 | sets block 5's kill flag (var122), full pole-fight prose |
| rails east ("kill a second batch") | task 149 `* e *` at Track | sets the combat flag (var43) and moves on — **the flag gates nothing runnable**, and block 6's kill flag (var124) is required by no task |
| carriage (after the hatch drop) | task 176 `* d *` at Carriage Roof 3 | needs pole held → sets block 7's kill flag (var137) and drops in; **without the pole, task 177 ExecTasks GAME OVER** |

So the [F]/[E] machinery is vestigial: with the twisted metal pole (gloves →
handbag in McDonalds; pole → second Station Entrance) every mandatory fight
resolves in narrative. The only way to strand yourself is to *start* a fight
without the bypass — e.g. `open door` at the Women's Toilets cubicle
(task 199), which is exactly what the old route did. `in` (task 196) was the
winning command, and the route already carried the pole.

The author disabled the system deliberately, not accidentally: dead task
variants throughout the file have their command literally renamed to `[dead]`
and parked at where=0, the same idiom.

## Runner ground truth — the WIN reproduces in run400.exe (2026-08-04)

Confirmed in the genuine `run400.exe` under Wine via the SCARE→Runner `.tas`
save-transplant flow (saves written by the harness `scare` at three route
positions, restored with `restore` + bare filename):

1. **Cubicle bypass** — restored just before the toilets, typed `in`: run400
   prints the identical pole-fight prose ("I rammed the pole through the
   head…"), picks up the coins, and `x coins` reads £1.10 — the same value
   SCARE shows at that state (the last 10p comes later, from the Thomas Cook
   desk).
2. **Escalator fight** — restored at the Escalators, typed `d`: identical
   block-1 auto-fight ("Slobbering, lurching… Soon, it was over…").
3. **Carriage fight → WIN** — restored on the carriage roof, typed `d`
   (identical block-7 fight, "Spear like, I thrust the pole…"), then
   `d`, `w`, `e` (full Candice+Ray cutscene, Friday Morning) and
   `open door`: the complete ending runs and the Runner's status bar shows
   **Congratulations!** — the VB6 win state. Even the author's garbled
   "I discarded theped down from the carriages onto the tracks." line
   reproduces byte-for-byte in both engines.

## Route summary

Follows `ThePlagueRedux_walkthrough.doc` faithfully. Skeleton:

1. **Prologue/Thursday night:** wait for the train, flee north, get knocked
   out, wake in the station. Kiosk: cap, till, note, code, stockroom torch.
   Coins: `search rides` (Mall), `search windows` (Ticket Hall). Nick scene
   (`talk to nick` ×4, `search nick` → cigarettes + lighter). McDonalds:
   `search highchairs`, `get handbag`, `open handbag`, `wear gloves`. Station
   Entrance: `get pole`. More coins: `search bench` (Walkway),
   `search condom machine` (Men's Toilets).
2. **Cubicle:** at the Women's Toilets, `in` (NOT `open door`) — pole fight,
   sixth coin cache banked automatically. `out`.
3. Litter bin cable, JJB trainers, Thomas Cook jacket + `search desk` coins
   (drop note+bible first, re-take bible — carry limit), then the vending
   machine: `x coins` (£1.20), `insert coins`, `press button` → water.
4. **Café:** `talk to woman`, `give water to woman` (back-office fight is a
   cutscene). Office: `get poster`, café `search drawers` → screwdriver,
   `open vent`, `u`, crawl to the grill, `s` (auto-pushes the body), `d`,
   `search body` → camera, `break camera` → batteries,
   `put batteries in torch`.
5. **Ray:** parade south, `turn torch on`, `d`,`d` (escalator fight auto),
   `w`,`d` (tracks fight auto, Ray appears), `talk to ray` ×3,
   `give cigarettes to ray` → long cutscene, keys received.
6. **Staff area:** ticket hall `nw`, `unlock door`, `open door`, canteen →
   locker room, `search lockers`, `wear jeans`.
7. **Cutters:** main platform via Walkway 4 (`s` twice at Cross Walkways —
   the first prints the darkness warning; torch must be on), platform fight
   auto, `e`,`d` (needs jacket+trainers+jeans worn), `e`,`e`,`ne`,`e`,`se`,
   **`drop screwdriver`, `drop lighter`** (cutters exceed the carry limit),
   `get cutters`.
8. **Candice:** back to the lounge, `watch tv`, `turn off tv`, `cut chain`,
   `n`,`w`,`smash door`, `talk to girl` ×2, canteen `examine board` +
   `examine photos` (name), back, `talk to girl` (Candice Chapel opens up),
   `give bible to girl` → she storms off.
9. **Endgame:** platform, `throw cable`, `u`, `w`,`w`, `d` (carriage fight
   auto — pole required, else instant game over), `d`, `w` (locked door),
   `e` → Candice+Ray cutscene → **Friday Morning** → `open door` → **WIN**
   ("I was out. I had survived…").

## Derivation footguns

1. **`x` the verb does not match the games' own `x * …` task patterns.**
   `x board`, `x photos`, `x body` all fall through to the library
   ("I couldn't see that around") while `examine board`, `read board`,
   `search body` fire the tasks. Every discovery command in the solution
   uses `examine`/`search`/`read`, never bare `x` — except `x coins`, which
   is a library-level money counter here and works.
2. **The discovery verb is `search`, not `examine`** for containers/bodies:
   `search till`, `search rides`, `search windows`, `search bench`,
   `search condom machine`, `search nick`, `search bin`, `search footwear`,
   `search desk`, `search highchairs`, `search lockers`, `search body`,
   `search drawers`, `search counter`.
3. **Carry limit bites twice:** the Thomas Cook jacket (drop note+bible
   first) and the bolt cutters (drop screwdriver+lighter — both are done
   with by then).
4. The `x coins` immediately after the cubicle reads £1.10 — the +10p
   registers by the time the vending machine is reached (£1.20). The clean
   solution counts only at the machine.
5. The torch switches itself off between areas; the solution re-issues
   `turn torch on` before each descent (harmless if already on).
6. The cubicle toolbox (nails, tape) mentioned by an earlier revision of
   this file is a red herring: `open toolbox` only matches in the Women's
   Toilets proper, its contents are needed for nothing, and the clean route
   skips it.

## Status

Wired and blessed as a ★ WIN;
`golden = goldens/plague_solution.expected.txt`. PASS — win marker
`spilling zombie blood once` (the closing line of the ending).
