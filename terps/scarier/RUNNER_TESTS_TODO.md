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

## 1. Battle System — never tested against a running Runner

The whole port was reverse-engineered from `Battles.bas` (DotFix decompile) plus
the v4 manual, and validated against a *synthetic* game
(`test/battle_test.taf`) built so every attribute has `Lo == Hi` and the rolls
collapse. Nothing in it has ever been diffed against run400/run390 executing the
same fight. Every formula below is now directly stageable with an authored
arena game.

Highest value first:

- [ ] **Upgraded-3.9 games really do stalemate in run400.** `Town of Azra` and
      `To Hell and Beyond` carry a 4.0 signature but are 3.9 games imported by
      the 4.0 editor: every range degenerate, acc/agi/recovery all zero. The
      claim on record is that the real 4.0 Runner also converts → `acc > agi` →
      permanent stalemate, which is the entire justification for keeping
      `SCR_ASSUME_COMBAT` **opt-in** rather than automatic. Never verified live.
      Settling it decides whether two golden rows can drop their assist flag.
- [ ] **Hit test is strictly `effAcc > effAgi`.** Stage `acc == agi` and confirm
      a miss.
- [ ] **Attribute roll has an exclusive Hi**: `lo + Int(Rnd*(hi-lo))`. Stage
      `lo=0, hi=1` (must always yield 0) and `lo == hi`.
- [ ] **Damage floor.** `dmg = effStr − effDef` applied only when `> 0`, else
      "…doesn't seem to do any damage."
- [ ] **Worn-armour defence path.** `ProtectionValue` summed over worn pieces —
      *unexercised by the entire corpus*, so the only evidence is the synthetic
      test. Highest risk of a silent wrong answer.
- [ ] **shoot (Method 3) zeroes base strength.** A known unfixed divergence:
      that's the 4.0 rule and Scarier applies it to 3.9 too, where 3.9 adds
      `HitValue` regardless of method. No corpus game ships a shoot weapon, so
      author one and run it in *both* Runners.
- [ ] **Speed / cadence.** `0→every turn, 1→rnd(1..2), 2/3/4→fixed`, plus the
      countdown's off-by-one.
- [ ] **Recovery counter.** `if counter==0 { counter=Recovery; if stamina<max
      stamina++ }; counter--` — an off-by-one here is invisible in short fights.
- [ ] **Target select.** `myAtt == 3 − theirAtt`, an enemy additionally targets a
      co-located player, neutral targets nothing. Watch the attitude enum: the
      action combo is `0=Ally,1=Neutral,2=Enemy` but internal/combat is
      `0=Neutral,1=Ally,2=Enemy`.
- [ ] **StaminaTask** fires at `0 < stamina < max/10`; **death** runs KilledTask
      else "…falls down, dead.", sets location `0xFB`, and clears other NPCs
      targeting the dead one.
- [ ] **Player-facing surface**: `wield`/`unwield`, best weapon = highest
      `HitValue`, "You can't `<verb>` with X!", the `status` layout
      (`Lo-Hi   <current incl. equipment>` + "is wielding"), and "can't get
      status of a character you've not seen yet!".
- [ ] **RNG — re-open the won't-fix.** It was written off as unmatchable, but VB6
      `Rnd` is a deterministic LCG and is only randomised by an explicit
      `Randomize`. If run400 never calls it, its sequence is fixed from a known
      seed and **byte-exact combat regression becomes possible**. Worth half an
      hour on `grep -a Randomize ~/Desktop/run400.txt` before anything else in
      this section.

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
| 3.9 shoot-Method strength | zeroes base Str (4.0 rule) | 3.9 adds `HitValue` regardless of method | **Known-wrong, unreachable.** Folded into §1. |
| Upgraded-3.9 combat | `SCR_ASSUME_COMBAT` opt-in; matches author intent | believed to stalemate | Folded into §1 — the one that actually pays off. |
| Restriction evaluation order | evaluates all, no short-circuit | `Sub_20_65` replaces `#` with T/F in a bool-expr string, so it can't short-circuit either | Believed matched. **Verify** a restriction with a side effect actually runs. |
| Integer division rounding | see `ADRIFT4_vs_ADRIFT5.md` | — | v4-vs-v5 difference already recorded; confirm the v4 half against run400. |
| Combat RNG | own generator | VB6 `Rnd` | Was won't-fix; see the §1 re-open. |

---

## Suggested order

1. §1 upgraded-3.9 stalemate + the `Randomize` grep — cheapest, and both unlock
   further Battle work.
2. §3(a) whole-corpus 3.9 differential — one scripted sweep, broad coverage, and
   it exercises fixups nothing else touches.
3. §2 wildcard ordering — needs a purpose-built probe game, and the fix churns
   goldens, so do it when there's room to re-bless carefully.
4. §4 body-part statics and the scope filter — small, self-contained.
5. §3(b) `Les Feux de l'enfer` — the last conversion-damage candidate, and the
   most work per answer.
