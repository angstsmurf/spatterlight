# TODO: remaining ADRIFT Runner fidelity tests

What is left to arbitrate against the **real** ADRIFT Runners. Companion to
`ADRIFT4_vs_ADRIFT5.md` (which records semantics already settled) and
`adrift-walkthroughs/WALKTHROUGH_TODO.md` (which is about route derivation, not
engine fidelity).

The premise changed in 2026-08: **both** Runners now execute on this machine, so
almost everything below is answerable by *playing* rather than by reading P-code.
Prefer a live run; fall back on the disassembly only when the question can't be
staged.

## Running the Runners

- Harness: `~/adrift-battle/runner/wine/` (README + `winlist.swift`,
  `winpos.swift`, `click.swift`, `cmd.sh`). x86_64 Wine under Rosetta.
- `run400.exe` = 4.0. `run390.exe` = 3.9, extracted from
  `~/Downloads/ADRIFT39/run390.CAB`; it runs in the same prefix and needs no
  `regsvr32`. `run400` **refuses** a 3.9 file ("Incorrect version"), so identify
  the file first — `sctaffil.cpp:53-65`, bytes 8 and 10 discriminate.
- `gen400.exe` sits beside run400. Its UTF-16 UI strings spell out the Generator
  dropdown enums *in order* — the fastest way to decode any `Var` mapping — and
  it can also **perform** a 3.9 → 4.0 conversion (see §3).
- P-code: `~/Desktop/run400.txt`, `~/Desktop/run390.txt` (`grep -a`; both contain
  stray binary).
- Probe games: hand-author a one-restriction / one-task variant, repack with
  `~/adrift-battle/runner/wine/taftool.py` or the Runner rejects it. Recipe and
  field offsets in the memory `scare-restriction-statics-run400`.
- Read transcripts with `screencapture -x -o -l<winID>`; classify cheaply by ink
  pixel count in the response band rather than OCR.
- **Input is unreliable.** The first keystroke of a scripted command is routinely
  lost, the MORE bar eats a key, and Auto complete rewrites the box *before* the
  echo. Always read the echo before believing a "the Runner doesn't support X"
  result. For turn-timed or RNG games, don't script a replay at all — write a
  `.tas` from Scarier and transplant it.

---

## 1. Battle System — now partially verified against the running Runner

The whole port was reverse-engineered from `Battles.bas` (DotFix decompile) plus
the v4 manual, and validated against a *synthetic* game
(`test/battle_test.taf`) built so every attribute has `Lo == Hi` and the rolls
collapse. **2026-08-01: the core formulas have now been diffed live against
run400** with authored arena probes (recipe below) — hit test, roll bounds,
damage floor, worn armour and the upgraded-3.9 stalemate all match the port.
The cadence/recovery/death items below are still untested.

Highest value first:

- [x] **Upgraded-3.9 games really do stalemate in run400.** *(2026-08-01, live.)*
      SCARE `.tas` written at Northern Trail (sword bought + wielded, no
      assist), transplanted and restored in run400, `n`, `attack bandit` ×11:
      every turn "a bandit manages to avoid your attack with the hunting
      sword." / "You manage to avoid a bandit's attack." Direct proof from the
      Runner's own `status`: after its conversion the player shows
      **Accuracy 0-0 / 0 / 0 and Agility 0-0 / 0 / 0** (Hit strength 1-1,
      current 6 (5) from the sword). `0 > 0` can never pass. Keeping
      `SCR_ASSUME_COMBAT` opt-in is faithful; the two golden rows keep their
      assist flags.
- [x] **Hit test is strictly `effAcc > effAgi`.** *(2026-08-01, live.)* Probe
      `pEQ2` (acc 5-5 vs agi 5-5 both ways, all weapon flags stripped): four
      clean `attack robot` turns, both directions "manages to avoid" every
      time. SCARE on the same file: identical. Beware the contaminated first
      attempt: with a held weapon in the room the Runner **auto-selects it for
      a generic `attack`** (see surface notes below), and its +Accuracy turned
      the equality case into 25 > 5.
- [x] **Attribute roll has an exclusive Hi**: `lo + Int(Rnd*(hi-lo))`.
      *(2026-08-01, live.)* Probe `pXH` (enemy Str 5-6, guaranteed hits, player
      Def 0): 8 hits, stamina 200→160 — every roll 5 (p ≈ 0.4% if Hi were
      inclusive; damage does re-roll per attack — a 4-hit run with Str 5-15
      summed 35, not divisible by 4). Probe `pZ1` (Str 0-1): always rolls 0.
      SCARE identical on both (160 / 200).
- [x] **Damage floor.** *(2026-08-01, live.)* Probe `pZ1` (Str 0-1 → roll 0 vs
      Def 0): every turn "Robot hits Player, but it doesn't seem to do any
      damage.", stamina untouched. SCARE: same message (with "you"), same
      stamina.
- [x] **Worn-armour defence path.** *(2026-08-01, live.)* Probe `pAR` (enemy
      Str 10-10 guaranteed hits, player Def 0-0, vest ProtectionValue 5 worn at
      start): 9 hits (an `i` turn also ticks combat — in both engines),
      stamina 200→155 = exactly 5/hit; the Runner's `status` shows Defense
      "0-0 0 **5 (5)**". SCARE identical (155). The highest-risk formula is
      confirmed.
- [x] **shoot (Method 3) zeroes base strength — 4.0 half confirmed live.**
      *(2026-08-01.)* Probe `pM3` (robot stamina 35, harmless; player Str 10 +
      blaster HitValue 30 Method 3): run400 kills on the **second** attack —
      30/hit, base Str replaced, exactly Scarier's rule; SCARE identical. The
      **3.9 half settled live too**: `test/make_39_probe.py` authors and
      packs a real V390 file (obfuscation + the `sPassword` field's own
      `Mid(5,4)=="Wild"` check, which run390 validates — same rule as the 4.0
      trailer), and run390 **one-shots** the 35-stamina robot — damage 40 =
      Str 10 + HitValue 30, added regardless of Method. Scarier's zeroing was
      wrong for 3.9 and is now version-gated on `battle_legacy` (commit
      `7a4cb7c2`); only ALEXIS shifted in the corpus (battle flavor, still
      wins, re-blessed). Bonus run390 observations: battle messages use
      second person ("Robot hits you") where run400 prints the player's
      name, no corpse line prints on NPC death ("Robot isn't here!" next
      turn), and a parse-error turn *does* tick combat in run390.
- [x] **Speed / cadence.** *(2026-08-01, live.)* Speed 2 → hits on turns
      2,4,6,8; Speed 3 → 3,6,9 (first attack on turn N, countdown starts at
      Speed); Speed 1 → irregular 1–2-turn gaps (5 hits/10 turns) consistent
      with `rnd(1..2)`. SCARE identical on 2/3 (byte-same hit turns).
- [x] **Recovery counter.** *(2026-08-01, live.)* Probe `pRC` (Recovery 3, take
      3×5 damage, retreat, status per turn): run400 regains +1 at turns 2, 5, 8
      — the same curve SCARE produces (its statuses step 187/188/189 at
      5/8/11). Phase and period match.
- [x] **Target select.** *(2026-08-01, live — and it found a real Scarier
      bug.)* Probe `pTS` (Aly att 1, Foe att 2, Bystander att 0, all
      co-located): run400's Foe picks the player *or* the ally per turn
      (P,A,P,A,A over five turns); Aly attacks Foe every turn; Bystander
      never acts; nobody targets the neutral. Scarier's Foe picked the SAME
      target every turn of a session (12/12), because `scr_randomint` mapped
      the congruential generator with `% range` and an LCG mod 2^32 has
      period-2 low bits — `(state>>1) % 2` alternates strictly, and a fixed
      even draw cadence pins every pick. **Fixed** (scutils.cpp): multiply-
      shift on the full 31-bit value, which is also what VB6's `Int(Rnd*N)`
      does; `scexpr.cpp`'s EITHER() pick had the same modulo. The fix
      re-sequenced every seeded transcript: v4 corpus re-blessed (see note
      below), four rows re-seeded (snakes 2, jason 11, light_up 2, circus
      2→17, les_feux 138), and the three Shadowpeak routes — battle lengths
      threaded too tightly to survive any new sequence (no seed in 1–800
      works) — pin the old mapping via `SCR_LEGACY_RANDMAP=1`, a documented
      harness-only compatibility hook.
- [x] **Death path (no KilledTask).** *(2026-08-01, live, via `pM3`.)*
      "Robot falls down, dead." (byte-same in SCARE), corpse leaves scope:
      run400 answers "Robot isn't here!" / "Player cannot see Robot from
      here." where SCARE says "I don't understand." / "Player sees no such
      thing." — semantics match (location `0xFB`), wording differs.
      **StaminaTask/KilledTask still untested** (needs a probe with authored
      tasks — the generator writes none yet).
- [ ] **Player-facing surface**: `wield`/`unwield`, best weapon = highest
      `HitValue`, "You can't `<verb>` with X!", and "can't get status of a
      character you've not seen yet!". Partially probed 2026-08-01 — the
      `status` layout is now known (columns
      `Range / Max / Current value (inc weapons/armour)`, then
      "Player is wielding …"), and see the surface notes below for
      auto-select-on-attack.
- [x] **RNG — re-opened and closed again: the won't-fix stands.** *(2026-08-01.)*
      run400 has **9 `Randomize` call sites**: `Form1.Form_Load` seeds from
      `Timer` at startup; `Form1.dencode` and `Sub_22_30` use the deterministic
      `Rnd(-1)` / `Randomize 1976` codec idiom; `Sub_20_6` (the load/restore
      machinery) mixes `1976` codec seeds with **`Randomize Timer` re-seeds**.
      Live proof of nondeterminism: probe `probeRNG` (enemy Acc 0-10 vs Agi
      0-10, Str 5-15), three fresh sessions, identical input (8 × `wait`), three
      different hit/miss sequences (H A H A H H A A / A A H H A H A H /
      H A H A A A A A) and staminas (165 / 168 / 187). Per-turn combat cannot
      be regressed byte-exact. **But**: the *load-time* attribute "current"
      roll is drawn from the deterministic 1976 stream *before* the Timer
      re-seed — the same file gives the same value every launch (Agility 0-10
      rolled 7 in all three sessions; a variant file with three extra text
      bytes rolled 9), so any load-time-rolled value is reproducible per file.
      `taftool.py` carries the exact VB6 LCG
      (`s' = (s*0x43fd43fd + 0xc39ec3) & 0xffffff`, post-1976 state
      `0x00a09e86`) if that ever becomes useful.

### Arena-probe recipe (2026-08-01) and surface observations

The probes above were authored from `test/make_battle_taf.py`: copy it, edit
the player/NPC battle stat lines (they are plain `s(lo); s(hi)` pairs), have it
also dump the uncompressed CRLF body, then
`taftool.py pack <body> <any real 4.0 taf> <out.taf>` — run400 accepts the
result. One probe = one ~2-minute Wine session. Degenerate (`Lo == Hi`) stats
make a probe immune to the per-session RNG, so single sessions are conclusive
for formula questions. Remember the settle-Return first, and count event lines
in the transcript instead of trusting the intended turn count.

Now in-repo: **`test/make_arena_probe.py`** (parameterized 4.0 probes — rooms
with exits, multiple NPCs with attitudes/speed/recovery, weapon/armour
objects; the M3/SP*/TS/RC configs are inline) and **`test/make_39_probe.py`**
(a genuine V390 file run390 loads: VB-PRNG obfuscation from absolute offset
14, and `sPassword` must be `pw[0:4]+"Wild"+pw[4:8]` — run390 checks
`Mid(5,4)`, the analogue of the 4.0 trailer check; `"    Wild    "` is the
no-password form).

**Corpus re-bless note (2026-08-01):** the `scr_randomint` low-bit fix (see
Target select above) changed every seeded transcript. All v4 goldens were
re-blessed after triage: 26 rows differed only in random flavor (event timing,
battle-roll variance) with win/score markers intact; five rows needed a new
per-row `SCR_SEED` to re-thread; the three Shadowpeak rows run under
`SCR_LEGACY_RANDMAP=1`. Re-deriving Shadowpeak under the fixed mapping is
open follow-up work — the routes fight several battles whose exact lengths
and a ~50-turn timer must all line up.

Surface facts learned on the way (all consistent between engines unless noted):

- The Runner does **not** auto-wield at battle start (`status`: "Player is
  wielding nothing"), but a generic `attack X` **auto-selects a held weapon**
  and narrates with the weapon's Method verb ("Player shoot Robot with the
  blaster."). SCARE instead auto-wields at battle start (its `status` shows
  the weapon) and its success message is generic ("You hit Robot.") without
  naming the weapon. Same outcomes on every probe; presentational divergence
  only — but it means an authored equality probe must strip weapon flags or
  the accuracy bonus contaminates the test.
- Runner battle messages use the player's *name* where SCARE substitutes
  "you"/"your", with second-person verb agreement kept ("Player manage to
  avoid Robot's attack." — sic). Miss messages otherwise identical.
- `i` (inventory) consumes a battle turn in both engines.
- `save` during an active battle is refused ("That is not an option or
  command." in SCARE); write probe saves in a room *before* the enemy.
- An unrecognised NPC name in `attack <x>` gets "Who do you want to attack?".

## 2. Wildcard / any-turn task turn-ordering divergence

Found 2026-08-01 while correcting the `thetest` walkthrough; **not fixed**.

`*`-wildcard tasks (`ALTCMD [*]`, `where=3`) appear to fire a turn late in
Scarier relative to the Runner.

Evidence, `thetest` `TASK 2 #clothesnot`
(`RESTR type=0 v1=3 v2=8 v3=0` = "clothes NOT worn by player"; actions = move
clothes to worn, score −1):

| input | 3.9 Runner | Scarier |
|---|---|---|
| `drop clothes` | "Nice try fish face!", −1, clothes never come off | "You are not holding your clothes." |
| `remove clothes` | "Lunatic, eh?  Yes?  Well tough.!", −1 | "You remove your clothes." — fish face lands on the *next* command |
| `drop fluff` | "You drop the fluff.  The fluff is dragged away into the machine." (one turn) | split over two turns |

For the Runner to answer "Nice try fish face!" as the *sole* response to
`drop clothes`, the restriction must have been true at evaluation time — i.e.
the library `drop` had already taken the clothes off, and the wildcard task ran
**after** it, same turn. (Neutral commands with the clothes on do *not* fire it,
so the restriction itself is being read correctly.)

To settle:

- [ ] Separate the two candidate causes — they are independently testable.
      (a) Does ADRIFT 4 run `where=3`/`*` tasks at *end of turn*, after the
      library action, rather than as input-matched tasks? (b) Does the Runner's
      library `drop` of a *worn* item implicitly remove it first, where Scarier's
      refuses? Test (b) in a game with **no** wildcard task.
- [ ] Where does the wildcard pass sit relative to events? Cross-check with
      `SCR_TRACE_EVENTS` on the Scarier side.
- [ ] Find the `remove clothes` task in the dump and check its restriction — the
      "Lunatic, eh?" text means a *second* authored task is involved, and its
      behaviour may discriminate between (a) and (b).
- [ ] Ground truth: the turn loop lives inline in `Form1.Text1_KeyPress`;
      `Sub_20_3` = restriction eval, `Sub_20_11` = action executor,
      `Sub_20_33` = events.
- [ ] **Build a minimal probe game** (one `*` task, one library action) rather
      than reasoning from `thetest`. Changing this will re-bless a lot of
      goldens, so it must be probed, not guessed.

Low urgency — no reachable score changed in `thetest` — but high value for
confidence in every `*`-task game.

## 3. 3.9 → 4.0 conversion

Two separate problems that keep getting conflated.

### (a) Scarier's own V390 parse fixups

`sctafpar.cpp` carries a set of 3.9-only schema rewrites, each of which was a
reasoned guess: `V390_TASK_ACTION: Type>4?#Type++`,
`V390_TASK_RESTR: Var1>0?#Var1++`, `V390_OBJECT: _Openable_,Key`,
`V390_TASK: $RestrMask`, `V390_V380_ROOM_EXIT`, and
`V390_V380_ALT_TYPEHIDE_MULT = 10`.

- [ ] **Whole-corpus 3.9 differential.** Now that run390 runs, the cheapest test
      by far: for every 3.x game in `games/` (roughly 23 of ~50 by header byte
      8 = `0x94`), drive the same short input script through run390 and Scarier
      and diff. Each fixup above is exercised by *some* game; a bad one shows up
      as a systematic mismatch rather than a one-off.
- [ ] **Use gen400 as an oracle.** The Generator can load a 3.9 game and save it
      as 4.0. Diff the resulting 4.0 field values against what Scarier's V390
      fixups produce for the same source file. That is a far stronger oracle
      than a transcript diff, and it has never been tried.

### (b) Author-side conversion damage (the parked deep-dive)

Are any untested **4.0** games unwinnable because their author converted them
from 3.9 in the Generator and the conversion broke tasks? The distinction to
draw per game is *faithful data damage* (the real Runner fails too → document,
do not patch) vs *Scarier divergence* (→ engine fix).

- [x] **`Through time` — SETTLED, do not re-open.** It used to be the strongest
      candidate (82% of its tasks are `Where = No Rooms`, which Scarier blocks in
      `task_can_run_task_directional`), on the theory that the Runner's
      `Sub_20_74` had a conditional True path for where-type 0. That theory was a
      misread: `Sub_20_74` is a command-reference / exit-scope filter, not the
      task room gate, and its "where-type 0" branch is really *reference*-type 0.
      `WALKTHROUGH_TODO.md` line 1113: "**Verdict (2026-06-25, RESOLVED —
      faithful, unplayable-by-design; do NOT patch).**" The game is an unfinished
      demo whose porch wall is the author's own in-game message
      (`adrift-walkthroughs/Through_time_walkthrough.md`), and it has a passing
      golden row. The old decode plan lived in
      `adrift-walkthroughs/TODO_decode_sub_20_74.md`, pruned in `aa30ba4f`; read
      it with
      `git show aa30ba4f^:terps/scarier/adrift-walkthroughs/TODO_decode_sub_20_74.md`.
- [ ] **`Les Feux de l'enfer`** (French, 289 tasks) — the only remaining 4.0
      candidate.
- [ ] Established and worth not re-deriving: no 4.0 game has an out-of-range
      task/event reference, so any conversion damage is subtle (off-by-one,
      wrong field meaning), never wholesale.

## 4. Contested semantics — Scarier deliberately differs, or nobody has checked

| Item | Scarier | Runner | Status |
|---|---|---|---|
| Negated `Var2` inside the any/no-object quantifier | negates once around the whole quantifier (the meaningful reading) | its per-object switch handles `Var2` 0–5 only, so "any" always fails and "no" always passes | **Deliberate, confirmed live.** No corpus game authors it. Keep. |
| Dynamic-object index past the end (`Var1 ≥ 3 + ndynamics`) | clamps to the last object | raises "Subscript out of range" and dies | **Deliberate.** Unreachable in any shipped game. Keep. |
| Body-part statics in a `Var1 = 2` restriction | positioned at `OBJ_PART_NPC` | statics have no location field, so they read hidden | **Untested.** Probe with a body-part static; the only surviving gap from the `Var1 = 2` fix. |
| Object scope when matching a task command | `uip_match_entity()` has no scope filter at all — matches anything | won't match an object that isn't present ("I don't understand what you mean!") | **Confirmed divergence, unfixed.** Measure corpus impact before touching it; it changes which task fires whenever a command names a distant object. |
| 3.9 shoot-Method strength | version-gated: 3.9 adds `HitValue` to base Str, 4.0 replaces | both confirmed live (run390 one-shot / run400 two hits) | **Fixed 2026-08-01** (`7a4cb7c2`). |
| Upgraded-3.9 combat | `SCR_ASSUME_COMBAT` opt-in; matches author intent | **stalemates, confirmed live 2026-08-01** (Azra: converted acc/agi all 0-0) | Settled — opt-in stays. |
| Restriction evaluation order | evaluates all, no short-circuit | `Sub_20_65` replaces `#` with T/F in a bool-expr string, so it can't short-circuit either | Believed matched. **Verify** a restriction with a side effect actually runs. |
| Integer division rounding | see `ADRIFT4_vs_ADRIFT5.md` | — | v4-vs-v5 difference already recorded; confirm the v4 half against run400. |
| Combat RNG | own generator | VB6 `Rnd`, `Randomize Timer` on the load path | Won't-fix confirmed live (§1): per-turn combat differs across identical fresh sessions. |
| Battle messages | second person ("you"/"your") | player's name with 2nd-person verb forms ("Player manage to avoid…") | Presentational only; noted §1 surface facts. |
| Battle-start wield | auto-wields best weapon | wields nothing; auto-selects held weapon per `attack` | Same outcomes on all probes; surface divergence only. |
| Enemy target selection | was pinned to one target per session (LCG low-bit + `% range`) | uniform per-turn pick among ally/player | **Fixed 2026-08-01** (`scr_randomint` multiply-shift); corpus re-blessed. |

---

## Suggested order

*(2026-08-01: the old item 1 is done — stalemate, hit test, exclusive Hi,
damage floor, worn armour and the RNG question are all settled live; see §1.)*

1. §1 remainder — *(done 2026-08-01, second batch: cadence, recovery, target
   select + the scr_randomint fix, death path, and the shoot rule in BOTH
   Runners.)* Still open: StaminaTask/KilledTask (needs authored tasks in the
   probe generator) and the player-facing wield/status surface items.
2. §3(a) whole-corpus 3.9 differential — one scripted sweep, broad coverage, and
   it exercises fixups nothing else touches.
3. §2 wildcard ordering — needs a purpose-built probe game, and the fix churns
   goldens, so do it when there's room to re-bless carefully.
4. §4 body-part statics and the scope filter — small, self-contained.
5. §3(b) `Les Feux de l'enfer` — the last conversion-damage candidate, and the
   most work per answer.
