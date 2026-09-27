# ADRIFT Runner fidelity: arbitration record

Every question Scarier has taken to the **real** ADRIFT Runners, and how it was
answered. Lives at `test/adrift4/notes/RUNNER_TESTS_TODO.md` (moved from
`terps/scarier/RUNNER_TESTS_TODO.md` and pruned to conclusions on 2026-09-28;
the full narrative history is in git:
`git log --follow -p -- terps/scarier/test/adrift4/notes/RUNNER_TESTS_TODO.md`).
Companion to `ADRIFT4_vs_ADRIFT5.md` (in `terps/scarier/`, semantics already
settled) and `test/adrift4/notes/WALKTHROUGH_TODO.md` (route derivation, not
engine fidelity).

**Every section is closed** (§§1–8 2026-08-01..09, §9 and §10 2026-08-17);
the few untaken probes left are marked **OPEN** in §4 and the closure log. What
stays live is the method — how to stage a probe against each Runner (§ *Running
the Runners*, §5's `where` probe, §6's 3.70 codec, §7, §8) — and the **§4
divergence table**, the standing list of every known Scarier/Runner difference
and why it is kept, ported or ignored. Read §4 before changing engine
behaviour; add a row when a new divergence is settled. Source comments cite
this file by section number — keep the numbering.

All Runners execute locally under Wine: prefer a live run; fall back on the
P-code/decompile only when a question can't be staged.

## Running the Runners

- **Harness**: `~/adrift-battle/runner/wine/` (README, `winlist.swift`,
  `winpos.swift`, `click.swift`, `cmd.sh`). x86_64 Wine under Rosetta.
- **Versions**: `run400.exe` = 4.0; `run390.exe` = 3.9 (from
  `~/Downloads/ADRIFT39/run390.CAB`, same prefix, no `regsvr32`). run400
  **refuses** a 3.9 file ("Incorrect version") — identify the file first
  (`sctaffil.cpp:53-65`, bytes 8 and 10). `run380.exe` / `run370.exe` from
  delron.org.uk's `adrift38.zip` / `adrift37.zip` (Wayback), same prefix,
  Generators beside them. run370 also needs `COMCTL32.OCX` in `syswow64`
  (extract that member from the cached `VB60SP6-KB2708437-x86-ENU.msi`,
  `regsvr32` it; winetricks' `comctl32ocx` hangs on a 7z overwrite prompt).
- **gen400.exe** (beside run400): its UTF-16 UI strings list the Generator
  dropdown enums *in order* — fastest way to decode any `Var` mapping — and it
  can convert 3.9 → 4.0 (§3).
- ⭐ **Generators upconvert → same-game cross-version cells.** `gen400.exe`
  opens a **3.80** file directly and `File → Save As` writes 4.00; `gen390.exe`
  does the same to 3.90. Downward does not work (`gen370` given 3.80 opens
  Untitled). Recipe: `wine 'C:\adrift\gen400.exe' 'C:\adrift\game.taf'` (arg
  honoured, but dismiss the modal "Tip of the Day" first), `File → Save As…`,
  type a **bare** filename (the dialog rejects `/`; osascript `keystroke` turns
  backslashes into slashes). **Diff the deobfuscated plaintexts** to confirm the
  field you measure survived: gen390's 3.8 → 3.9 pass rewrites `SizeWeight`,
  and the 3.90 copy of `microwaveman.taf` lost its noun aliases.
- ⭐ **3.7/3.8 files: edit rather than convert.** Plain CRLF text XOR'd with
  the VB6 PRNG from seed `0x00a09e86` — no signature, length header or trailer;
  the stream is indexed by absolute offset, so decode, rewrite a line,
  re-encode; length may change freely (§6). E.g. `arlo.taf`'s bone prefix
  `a` → `some` gave run370's `You pick up some bone.`
- **P-code**: `~/Desktop/run400.txt`, `~/Desktop/run390.txt` (`grep -a`; stray
  binary inside).
- ⚠️ **Check Options → Display & Media… → Appearance → "Room names in
  descriptions" before measuring anything with a room block.** While it is off
  the Runner prints no room-name heading anywhere (movement, start, `look`,
  `ShowRoomDesc`). It defaults **ON** (`Proc_21_24_4747F8` reads
  `showshortroom` with default 1) and **is persisted** (`"showshortroom"` under
  `Software\VB and VBA Program Settings\ADRIFT\Runner` in `pfx/user.reg`);
  measurements before 2026-08-15 were taken in a prefix with a persisted
  untick. A preference, not engine behaviour; Scarier's
  `game->bold_room_names` (default TRUE) = ticked. No working keyboard path
  under Wine: menu `swift click.swift 95 72`, "Display & Media…" `127 240`,
  tick `176 239`, OK `110 300` (default window position).
- **Probe games**: hand-author a one-restriction / one-task variant, repack
  with `test/adrift4/harness/taftool.py` or the Runner rejects it (recipe and
  field offsets: memory `scare-restriction-statics-run400`).
- ⭐ **Read the Runner through its own transcript, not screenshots.** Wine never
  delivers Alt, so *click* the menu title (+25/+35 from window top-left) and pick
  the item by accelerator letter.
  - **run390 / run400**: `Adventure → Start &Transcript` → dialog pre-filled
    `C:\adrift\Adrift_<N>.txt`; `key code 36` accepts; appended live. Scripted:
    `~/adrift-battle/runner/wine/runner_transcript.sh <game.taf> <cmdfile>
    [runner.exe]`.
  - **run370 / run380**: `Adventure → Save &Transcript` dumps the scrollback to
    `C:\adrift\Adven_<N>.rtf` *at click time* plus a MsgBox — play first, save
    last (`run380 Form1.frm`, `transcript_Click` at `42A378`). Scripted:
    `runner_savetranscript.sh`; read with `textutil -convert txt -stdout
    Adven_1_marooned.rtf`.
  - Answer load-time modals (name InputBox, gender prompt) **before** clicking
    the menu. `drive_ckpt.sh` aborts on an unexpected mid-script window.
- Screenshot fallback: `screencapture -x -o -l<winID>`; classify by ink-pixel
  count in the response band, not OCR.
- **Input is unreliable**: first keystroke often lost, MORE bar eats a key,
  Auto complete rewrites the box before the echo. Read the echo before
  believing "the Runner doesn't support X". For turn-timed/RNG games write a
  `.tas` from Scarier and transplant it instead of scripting a replay.

---

## 1. Battle System

Ported from `Battles.bas` (DotFix decompile, `~/Adrift_decompile/run400-analysed/`)
and the v4 manual; synthetic regression game
`test/adrift4/harness/battle_test.taf` (`Lo == Hi` everywhere). Every item below
measured live and ported; **§1 CLOSED**.

- **Upgraded-3.9 games stalemate in run400.** A converted player has
  Accuracy/Agility 0-0, and `0 > 0` never passes (Azra `.tas` transplant,
  `attack bandit` ×11 all misses). `SCR_ASSUME_COMBAT` stays opt-in.
- **Hit test is strictly `effAcc > effAgi`** (probe `pEQ2`). A held weapon is
  auto-selected by a generic `attack` and adds Accuracy — strip weapons when
  probing.
- **Attribute roll has exclusive Hi**: `lo + Int(Rnd*(hi-lo))`; damage re-rolls
  per attack (probes `pXH`, `pZ1`).
- **Damage floor**: roll 0 vs Def 0 → "…hits Player, but it doesn't seem to do
  any damage.", no stamina change (`pZ1`).
- **Worn armour** adds ProtectionValue to defence (`pAR`: exactly 5/hit less;
  status Defense "0-0 0 5 (5)").
- **Shoot (Method 3) strength — version split**: run400 *replaces* base Str with
  HitValue (`pM3`, two hits); run390 *adds* HitValue regardless of Method
  (`make_39_probe.py`, one-shot). Gated on `battle_legacy` (`7a4cb7c2`).
  run390 extras: second-person battle messages, no corpse line on NPC death, a
  parse-error turn ticks combat.
- **Speed / cadence**: Speed N → first attack on turn N, then every N; Speed 1
  → `rnd(1..2)` gaps.
- **Recovery**: +1 stamina every Recovery turns, phase matching Scarier (`pRC`).
- **Target select**: enemy picks uniformly per turn among player/allies; allies
  attack the foe; neutral NPCs never act or get targeted (`pTS`). Fixed a
  Scarier bug: `scr_randomint` (`scutils.cpp`) now multiply-shifts the full
  31-bit value like VB6 `Int(Rnd*N)` instead of `% range` (LCG low bits have
  period 2); same fix in `scexpr.cpp`'s EITHER().
- **Death path (no KilledTask)**: "Robot falls down, dead."; corpse leaves scope
  (location `0xFB`). run400's out-of-scope wording ("Robot isn't here!") differs
  from Scarier's; semantics match.
- **StaminaTask/KilledTask** (probe `pKT` in `make_arena_probe.py`; 3.9 via
  `make_39_ktprobe.py`): a KilledTask **replaces** the "falls down, dead." line;
  StaminaTask fires on **every** hit leaving `0 < stamina < max/10` (float
  divide; ported as `stamina * 10 < maximum`), not on the killing blow; the
  corpse's held/worn objects re-home to the death room *before* the KilledTask.
  run390 prints **nothing** when a task-less NPC dies (default corpse line gated
  `!battle_legacy`). Ported in `scbattle.cpp`.
- **Battle-task dispatch is gated on player task-eligibility** (Del Sol + probe
  `KT2`): run400 runs KilledTask/StaminaTask through its general run-task
  routine, silently dropping ineligible dispatches — wrong room (Del Sol's
  Moreland: game faithfully UNWINNABLE) or done non-repeatable (no re-fire, no
  corpse line). `battle_kill`/`battle_apply_damage` gate on
  `task_can_run_task_directional`. run400 strips all leading `#` from typed
  input, so `#` tasks are untypeable (SCARE's SPECIAL_PATTERN exclusion is
  equivalent).
- **Wield model** (probes `pWS`, `pWS2`), ported in `scbattle.cpp` /
  `sclibrar.cpp` / `scgamest.cpp`:
  - Persistent wield ref, starts "nothing"; every armed player blow persists it
    (before the hit test, so misses too).
  - Bare `attack X`: wielded weapon if set; else exactly one held weapon is
    auto-selected **and wielded**; none → bare blow; 2+ held → rhetorical "What
    do you want to attack X with?" (admin, no combat tick; bare-noun reply is a
    parse error).
  - `attack X with Y` and `wield Y` set the wield. **No `unwield` verb.**
    Weapon leaving the player's hands (drop/throw/give/put/wear —
    `gs_carried_track`) clears to nothing; re-taking does not re-wield.
  - Wrong-method wielded weapon → "Player can't cut with the axe!" (ticks).
  - `status <unseen npc>` falls back to plain player `status`.
  - `attack <typo>` → "Who do you want to attack?" ticks combat; "I don't
    understand." does not.
  - Status table: header `Range / Max / Current value (inc weapons/armour)`,
    labels `Stamina: / Hit strength: / Accuracy: / Defense value: / Agility:`,
    no "You have:"; Stamina row live/max/live; equipment rows
    `lo-hi / max / current / (equipment share)`; Agility no paren; trailer
    "Player is wielding a sword." (article prefix; "nothing" unarmed); NPC
    status same, "Robot is wielding nothing." (`lib_print_battle_status`,
    `battle_attribute_bonus`).
  - Refusals: `wield <non-carried>` "…aren't carrying the rock!" [sic] vs
    `attack X with <non-carried>` "…is not carrying the rock!"; both tick;
    " is not a weapon!".
- **RNG — won't-fix.** run400 seeds from `Timer` at `Form_Load` and re-seeds
  `Randomize Timer` on the load path; identical input gives different combat
  per session (`probeRNG`). Load-time attribute "current" rolls come from the
  deterministic `Randomize 1976` stream, so they are reproducible per file.
  `taftool.py` carries the VB6 LCG (`s' = (s*0x43fd43fd + 0xc39ec3) &
  0xffffff`, post-1976 state `0x00a09e86`).

### Arena-probe recipe (2026-08-01) and surface observations

- **`test/adrift4/harness/make_arena_probe.py`**: parameterized 4.0 probes
  (rooms/exits, NPCs with attitude/speed/recovery, weapons/armour, tasks,
  variables; configs inline; `persp` sets Globals/Perspective, default 2 —
  `TK` uses `persp=1`). Base: `make_battle_taf.py`, packed with
  `taftool.py pack <body> <any real 4.0 taf> <out.taf>`.
- **`test/adrift4/harness/make_39_probe.py`**: genuine V390 files — VB-PRNG
  obfuscation from absolute offset 14; `sPassword` must be
  `pw[0:4]+"Wild"+pw[4:8]` (run390 checks `Mid(5,4)`); `"    Wild    "` = no
  password.
- One probe ≈ one 2-minute Wine session; `Lo == Hi` stats make single sessions
  conclusive. Settle-Return first; count event lines rather than trusting the
  intended turn count.

Surface facts (engines agree unless noted):

- **Method-verb narration** (ported, identical in run390 and run400): armed hit
  "You shoot Robot with the blaster." (+"s" for NPCs); method 5 "You throw the
  knife at Robot."; armed player miss "<npc> manages to avoid your attack with
  <weapon>."; armed NPC miss "<npc> attacks you with <weapon>, but you manage to
  avoid it.". Bare hands keep plain forms.
- **Thrown (method 5) weapon** (probes `pTD`, `p39td`, `TDM`, `TDZ`), ported in
  `battle_resolve`: a player throw that lands moves the weapon to the room in
  both Runners, even if it does no damage; a *missed* throw keeps it. Damage:
  run400 Str only (wielded ref cleared before the roll), run390 Str+HitValue.
  No accuracy penalty; weaponless `attack` doesn't pick a floor weapon up; NPC
  throws neither drop nor lose HitValue. (3.9 has no miss step.)
- run400 battle messages use the player's *name* with 2nd-person verbs
  ("Player manage to avoid Robot's attack." sic); Scarier uses "you".
- `i` consumes a battle turn in both engines.
- `save` during active battle is refused; write probe saves before the enemy.
- Unrecognised NPC in `attack <x>` → "Who do you want to attack?".

## 2. Wildcard / any-turn task turn-ordering divergence — SETTLED 2026-08-01

Wildcard tasks are input-matched in both engines; same-turn firing comes from
always-restarting events, and "sole response" oddities from author ALRs. Probes:
`test/adrift4/harness/make_39_wildprobe.py` (+ gen400-converted twins run in both
Runners). Fixed in `scevents.cpp`, `scrunner.cpp` (`run_event_task`),
`sclibrar.cpp`.

- **No end-of-turn wildcard pass.** A `*` task gated on "rock held" fires on the
  command *after* `take rock`, in run390 as in Scarier.
- **Event TaskAffected dispatch — version split.** run390 dispatches the
  affected task *by command text* through the matcher: first task in list order
  that matches (`*` matches anything) and passes fires — a runnable earlier
  wildcard **steals** the execution; a restricted match is skipped silently;
  restrictions see post-library state. run400 runs the affected task directly
  and prints a failing FailMessage every turn. Gated: `< TAF_VERSION_400` →
  `run_event_task()`, else direct `task_run_task`.
- **Author ALRs over joined output.** The Runner joins a turn's output into one
  paragraph (two spaces) and applies ALRs to the whole; Scarier applies per
  string, so combined-pattern ALRs (thetest's "Nice try fish face!") can't
  match. Residual cosmetic divergence.
- **Named `drop` of a worn item implicitly removes it** in both Runners; `drop
  all` leaves worn items. `lib_drop_named_filter`.
- **Completed non-repeatable task claims the command below 4.0 — PORTED
  2026-09-13** as `run_spent_task_390()` (`scrunner.cpp`), at run390's
  `checktask` (44B6E8), before movement and the standard library, any pattern
  including bare `*`. The done arm (44B4DD) writes the RepeatText (or the
  load-time default, 465A8B..465AB9) into the answer buffer (44B537) and jumps
  to 44B6CC *inside* the loop, so the scan continues: a later passing task still
  runs (44B663; Journ2 T5), and a later failing restriction's non-empty
  FailMessage overwrites the buffer (452BB8, 44B636). inverness soft-locks
  exactly as in run390 (`Adrift_1030_inverness.txt`).
- Residual small divergences: run390 appends task text to `i` output where
  Scarier's wildcard replaces it; run400 names the player with 2nd-person verbs;
  the joined-paragraph ALR point above.
- **Multi-turn walk stop runs CharTask/ObjectTask on the arrival tick only**
  (walk probe H, both Runners) — `npc_tick_npc_walk` `is_arrival` gate, fixed
  and follow-player stops. **Roomgroup stops re-run every tick** (Ticket to No
  Where's lost girl) and keep that behaviour.
- **Follow-player stops** (probes K/L/M, both Runners): the walker warps to the
  player only on the walk-counter refresh tick (Times=1 = classic trailing);
  CharTask fires on an arrival tick even if the walker didn't move; **run400
  only** also fires the CharTask when the *player* enters the walker's room
  (any stop, every re-entry) — SCARE's existing check, now gated >= 4.0. That
  re-check is CharTask-only (no ObjectTask). `look` fires nothing.
- **run390 drops a walker's leave line when no exit links the rooms** (probes
  H, J); arrivals still print directionless. run400 prints a directionless
  leave line. Gated in `npc_announce`.
- **Walk CharTask/ObjectTask dispatch** (`make_39_walkprobe.py` /
  `make_400_walkprobe.py`, variants E/F/G): run390 = wildcard-interceptable,
  list order decides, restricted task skipped silently (same P-code as
  `checkevent`); run400 = direct run, loud FailMessage (`METFAIL.`).
  `run_npc_walk_task()` gated: pre-4.0 → `run_task_command_dispatch()`, 4.0 →
  `task_run_task`.
- **Walk ticking**: walks never tick at startup (walk handler called only from
  `Form1.evaluate`); counter seeded ΣTimes+1; arrivals fire on exact suffix-sum
  matches; a non-looping walk's final decrement expires it silently. Scarier's
  startup `npc_tick_npcs` call removed. **1-stop non-looping game-start walk:
  version split** — run390 never runs it (wkC_390), run400 runs it (arrives
  turn 1, CharTask once; wk4C_400). Gated narrowly (StartTask=0 only;
  task-started one-stop walks keep running — "deaths" needs it).
- **Empty input matches a `*` task** in Scarier and both Runners (the
  settle-Return fires it and ticks). No divergence.

## 3. 3.9 → 4.0 conversion

### (a) Scarier's own V390 parse fixups

`sctafpar.cpp` 3.9-only rewrites: `V390_TASK_ACTION: Type>4?#Type++`,
`V390_TASK_RESTR: Var1>0?#Var1++`, `V390_OBJECT: _Openable_,Key`,
`V390_TASK: $RestrMask`, `V390_V380_ROOM_EXIT`,
`V390_V380_ALT_TYPEHIDE_MULT = 10`, `V390_TASK_ACTION:_BattleAttr_`.

- **Method — gen400 as structural oracle**: open the 3.9 `.taf` in gen400, Save
  As 4.0, run both under Scarier with `SCR_DUMP_TASKS=1` and diff the dumps
  (`scdump.cpp`, ALT and OPENABLE sections exist for this). All 28 3.9 corpus
  games done. **gen400 is a second opinion, not ground truth** — both disputes
  were settled by run390 against the Generator.
- **Verified, zero mismatches**: `_Openable_,Key` (5 ↔ 6 swap), `Var1++`,
  `$RestrMask`, `Type++`, `ROOM_EXIT`.
- **Room-alt ordering — fixed** (`make_39_altprobe{,2,3}.py`).
  `lib_find_starting_alt()` scans backwards, so
  `parse_fixup_v390_v380_room_alts()` emits least specific first:
  `[LastDesc catch-all (disp 2), Task1 alt, Task2 alt, object alt (disp 0)]`.
  A completed gating task prints the AddDesc *instead of* LastDesc; Task2 beats
  Task1; an applicable object alt beats both; an alt whose Task1/Obj is 0 is
  ignored (Scarier's zero-guards right; gen400 over-converts — residual
  `cv13`/`cv20`/`cv23` diffs).
- **"Change battle attribute" index — fixed** (`make_39_battleattr_probe.py`).
  3.9 characters have only Stamina/Strength/Defence (+Attitude/Speed for NPCs),
  eight dropdown entries; mapping **0-4 unchanged, 5 → 7 (Defence), 6 → 8 (Max
  Defence), 7 → 11 (Speed)**. run390 `status` shows three lines `current (max)`.
  gen400 wrongly maps 5 → 11 (un-chained `If` cascade); the 17 remaining dump
  mismatches are that Generator bug.

### (b) Author-side conversion damage (the parked deep-dive)

Question: are any 4.0 games unwinnable because the author's Generator
conversion broke tasks? Classify as *faithful data damage* (document) vs
*Scarier divergence* (fix). **CLOSED — no candidate remains.**

- **`Through time`** — faithful, unplayable-by-design demo; do NOT patch. The
  "`Sub_20_74` has a where-type-0 True path" theory was a misread (reference
  type, not where type; that label now resolves to `457AC8`, the room-alt
  evaluator). See `test/adrift4/notes/Through_time_walkthrough.md`; old decode
  plan: `git show aa30ba4f^:terps/scarier/adrift-walkthroughs/TODO_decode_sub_20_74.md`.
- **`Les Feux de l'enfer`** — native 4.0, never converted (51 battle-attr
  actions use indices 5/9/10, impossible from 3.9); unwinnable by design.
- No 4.0 game has an out-of-range task/event reference, so any conversion damage
  would be subtle (off-by-one, wrong field meaning), never wholesale.

## 4. Semantics arbitrated against the Runners — the standing divergence table

Every row is settled — measured live, ported, deliberately kept, or refuted —
except those marked **OPEN**. *Restriction evaluation order* rests on the
P-code alone, because no ADRIFT 4 restriction has a side effect to probe. This
table is the live part of the file: read it before changing engine behaviour,
and add a row when a new divergence is settled. Rows that cite "the closure
log" refer to the dated batches now summarised at the foot of this file (full
text in git history).

| Item | Scarier | Runner | Status |
|---|---|---|---|
| Negated `Var2` inside the any/no-object quantifier | negates once around the whole quantifier | per-object switch handles `Var2` 0–5 only, so "any" always fails, "no" always passes | **Deliberate**, confirmed live. No corpus game authors it. |
| Dynamic-object index past the end (`Var1 ≥ 3 + ndynamics`) | clamps to the last object | "Subscript out of range", dies | **Deliberate.** Unreachable in shipped games. |
| Body-part statics in a `Var1 = 2` restriction | positioned at `OBJ_PART_NPC` | identical: with parent present, is-hidden FAILS, visible-to/not-hidden PASS; visible-to tracks the parent's room | **No divergence** 2026-08-01 (probes `pBP`/`pBP2`, `make_arena_probe.py`). Parent absent → scope filter (next row). |
| Object scope when matching a task command | **PORTED 2026-09-20**: `uip_match_entity()` runs a two-round scope loop, gated `>= TAF_VERSION_400` | 4.0: two passes, present-and-seen then absent-but-seen; FIRST namesake in index order wins the binding pass (458E6C, `Proc_21_53_44B578`). Pre-4.0: no scope test, LAST seen namesake wins; 3.80 binds even an unseen absent object | **Ported** (probe `p4OBJREF`, `harness/make_objrefprobe.py`; `Adrift_objref390.txt`, `Adrift_objref380.rtf`). Not a refusal: an unmatched name just fails to match. Zero corpus exposure (`SCR_TRACE_SCOPE` audit). |
| Optional `{word}` whose look-ahead fails after consuming text | **FIXED 2026-08-03**: `uip_match_optional()` rewinds to `start_posn` on failure | matches | **Ported.** Witness: *Monsters (Release 2)* `shine flashlight on the brainsucker` (`{brain}` prefix-ate "brainsucker"). |
| A `/` outside any `[]`/`{}` group | **FIXED 2026-08-04**: `uip_parse_list()` tracks group depth; at depth 0 `/` is a literal word | literal character — `Proc_9_4_45D940` (`NewParse.bas`) only splits on `/` inside `[]`/`{}` | **Ported.** Witness: *Ba'Roo!* TASK 62 `take/get/eat stew` (was prefix-matching every `take`). |
| Input synonyms whose replacement is itself synonymed | **FIXED 2026-08-04** (`pf_filter_input()`): after one fires, a later synonym fires only when its original is the *whole* replacement, and replaces all of it | same: later synonyms act on an earlier one's output only as a whole | **Ported.** Witnesses: *Lair of the Vampire* (`harris`↔`steve`), *Yak Shaving* (`flags`/`line`/`clothes` → `clothes line`; naive apply-all never terminates). |
| `%object%` given a *partial* prefix | **FIXED 2026-08-03**: `uip_build_candidate()` stores the prefix with leading words dropped one at a time; the Short is never cut | matches a partial prefix | **Ported.** Witnesses: *Monsters* `examine the four poster bed`; *Shadrick's Travels* `climb oak tree`. `SCR_DUMP_TASKS` `OBJNAME` prints `prefix=`/`alias=`. |
| 3.9 shoot-Method strength | 3.9 adds `HitValue` to base Str; 4.0 replaces | both confirmed live | **Fixed 2026-08-01** (`7a4cb7c2`). |
| Upgraded-3.9 combat | `SCR_ASSUME_COMBAT` opt-in | stalemates (converted acc/agi all 0-0) | **Settled 2026-08-01** — opt-in stays. |
| Restriction evaluation order | evaluates all, no short-circuit | `Sub_20_65` substitutes T/F into a bool string — no short-circuit | **Believed matched** (P-code only; ADRIFT 4 restrictions have no side effects, so unprobeable). |
| Integer division rounding | `Round((a/b) + 0.000001)` (scexpr.cpp) | same: −5/2=−2, −7/2=−3, −1/2=0, 5/2=3 | **No divergence** 2026-08-01 (probes `pDIV`/`pDIV2`). |
| Unary minus in expressions | folded into the literal: `-5/2` = **−2** | operator reducing after `/`: 0−(5/2) = **−3** | **Documented, not fixed** (`pDIV`). Zero corpus exposure; reshaping scexpr's parser risks more than it buys. ADRIFT 5 side: **no divergence** (symmetric `Math.Round(AwayFromZero)`), battery in `test/adrift5/harness/a5sexpr_test.cpp`. |
| ADRIFT 5 paren group ahead of a `*`/`/` chain | a5sexpr: uniformly left-associative, `(100+0)/10/5` = 2 | clsVariable's run-based reducer: a group needing run 2 can't collapse on run 1, so the trailing chain right-associates (→ 50); bare `(A)/B/C` stays left (clsVariable.vb:866 in 5.0.36.5, :826 FrankenDrift) | **Documented, not fixed** 2026-08-22 (ralphmerridew, intfiction 81359/37). Zero exposure across 174 a5 games (`harness/a5dump`). Pinned in `a5sexpr_test.cpp`. |
| ADRIFT 5 identical `<#…#>` bodies in one text block | **PORTED 2026-08-29**: `replace_expressions` (a5text.cpp) — a repeat still draws but emits the first value; deferred path via `a5run_draw_defer_entry` (a5run_action.cpp / a5run_resp.cpp) | `Global.ReplaceExpressions` (Global.vb:510) does replace-**all** per match, so the first value fills every identical slot; later matches still draw | **Ported** (saabie via ralphmerridew, 81359/39). Witnesses: Lost Coastlines, The Last Expedition. Pinned by `a5_exprdup_test.cpp` (`make -f Makefile.headless a5duptest`). |
| ADRIFT 5 `<#…#>` tag inside an expression's own result | never re-scanned | also unevaluated — except when the same tag appears directly elsewhere in the block, where replace-all sweeps the copy too (81359/41) | **Documented, not fixed** 2026-08-29. Zero exposure. |
| ADRIFT 5 draw order of two `rand`/`urand` when one has a compound argument | recursive descent: left call draws first | run-based reducer: the later *simple* call reduces (draws) first | **Documented, not fixed** 2026-08-29 (81359/39). Zero real exposure. `if()`/`AND`/`OR` evaluate every operand in both engines. |
| Combat RNG | own generator | VB6 `Rnd`, `Randomize Timer` on load | **Won't-fix** (§1): per-turn combat differs across identical Runner sessions. |
| Battle messages | second person | run400: player's name with 2nd-person verbs ("Player manage to avoid…"); run390: second person | **Presentational, kept** (matches run390). Method-verb weapon narration ported 2026-08-01, both Runners. |
| Wield model | **PORTED 2026-08-01**: persistent wield ref | persistent ref; single held weapon auto-selected and persisted; asks with 2+ held; no `unwield`; drop clears to nothing; status folds only the wield | **Ported** (probes `pWS`, `pWS2`; §1). Status table layout and "aren't carrying"/"is not carrying" wording ported too. |
| Thrown (method 5) weapon | drop + version-split damage | player throw moves weapon to the room (both Runners); run400 Str-only, run390 Str+HitValue | **Ported 2026-08-01** (probes `pTD`/`p39td`; §1). |
| Enemy target selection | uniform per-turn pick | same | **Fixed 2026-08-01** (`scr_randomint` multiply-shift). |
| Event TaskAffected execution | 3.9 = matcher dispatch (wildcard steal, silent restricted skip); 4.0 = direct run (loud FailMessage) | run390 and run400 genuinely differ | **Ported 2026-08-01** (§2). |
| Named `drop` of a worn item | implicitly removes then drops (named only; `drop all` skips worn) | same, both Runners | **Fixed 2026-08-01** (`lib_drop_named_filter`). |
| Completed non-repeatable `*` task | **PORTED 2026-09-13**: `run_spent_task_390()` claims ahead of movement and library | run390 `checktask` (44B4DD..44B537): done arm writes RepeatText (or load-time default) into the answer buffer and the scan *continues* — a later passing task still runs, a later failing restriction's message overwrites | **Ported** (§2). Soft-locks inverness as run390 does. |
| A **spent** task whose restrictions now FAIL with a message | version-gated 2026-08-23 in `run_game_commands_common()` | 4.0: restrictions before done state — fail message prints (library-callback path too); 3.9: done state first — "You have already done that." | **Ported** (probe `DONE` / `make_39_doneprobe.py`; `Adrift_14.txt`, `Adrift_18.txt`). |
| An **unspent** task failing with a message, matched from the library take/drop callback | `run_game_task_commands()` passes `include_restrictions` only from 4.0 | 4.0: fail message wins; pre-4.0: library wins silently | **Ported 2026-08-23** (`Adrift_20.txt`, `Adrift_18.txt`; run380 `haunt.taf` `take fish`, `Adven_2.rtf`). |
| Below 4.0, a task match that says nothing still **claims** the command | **PORTED**: spent half 2026-09-13 (`run_spent_task_390()`), silent half 2026-09-20 (`silent_task_390` in `run_all_commands()`, skips library and tick) | pre-4.0 claims it: spent → "You have already done that."; silent run → "I don't understand."; run400 falls through | **Ported** (`Adrift_18.txt`, `Adrift_19.txt`). Witness: `the_hangover` filing cabinet. |
| A 4.0 task whose **only** action is `End game`, no CompleteText | `task_run_end_game_action()` returns `var1 != 3`, so the ending claims the command | falls through like any silent match (ending prints at end of turn); run400 prints the **global** DontUnderstand, then the WinText | **Measured 2026-08-23, NOT ported. OPEN:** why run400 reaches the *global* refusal rather than the verb-object refusal with the referenced object in hand — probe an unrecognised verb + unambiguous held object before porting. Returning FALSE alone lets `lib_cmd_verb_object` claim it silently. Witness: `relojero.taf` task 5 (`pfx/drive_c/adrift/relojero.txt`). Cosmetic, one line. |
| `drop <thing> in/on <container>` | **FIXED 2026-08-02**: six patterns in `PRIORITY_COMMANDS[]` (`scrunner.cpp`) ahead of plain drop | routes to put-in / put-on handlers | **Ported.** Witness: *Ticket to No Where* `drop <litter> in bin`. |
| What `all` ranges over | **FIXED 2026-08-02**: `lib_take_all_filter()` excludes anything already held; named take still reaches into a carried open container | same | **Ported.** Witness: *Ticket to No Where* `get all` with the bag. |
| `get all` with nothing takeable | version-gated 2026-08-15 (`lib_cmd_take_all`, `lib_take_multiple_common`) | pre-4.0 "There is nothing to pick up here."; 4.0 "There is nothing worth taking here." (no "else" form) | **Ported** (run370 `p37pos`, run380 `marooned`, run390 `p39held`, run400 `TK`). |
| `%character%` / `%object%` as last element inside a `[...]`/`{...}` group | **FIXED 2026-08-02** (`scparser.cpp`): a NULL `right_sibling` means the remainder is vacuously satisfied | matches | **Ported.** Witness: *ADRIFTMAS Party* `kiss mystery`. |
| `*` or `%text%` **inside** a group | matches two cells run400 refuses: group-trailing `*` on zero words (`[eat *]` on `eat`), mid-group `%text%` (`[quip %text% hard]`) | run400 treats a pattern with either token in a group as dead; `%character%` in a group and top-level `%text%` are fine | **Documented, not fixed** (probes `TX`/`TX2`; echo a capture as `[%text%]`). Zero corpus exposure (`test/adrift4/harness/taf_pattern_scan.py` — re-run before deciding parser questions by exposure). |
| `*` matching **zero** words | matches | matches | **No divergence** 2026-08-02 (probe `ST`). |
| Which failing restriction's FailMessage prints | lowest-indexed failing; empty → fall through to library | same, incl. mixed masks | **No divergence** 2026-08-02 (probes `FM`/`FM2`). |
| Put-family precedence over a matched-but-failing task | **PORTED 2026-08-02**: `put` patterns in `PRIORITY_COMMANDS`; `lib_put_in_is_valid`/`lib_put_on_is_valid` defer (`run_priority_defer()`), STANDARD_COMMANDS duplicates print refusals. Canonical prefixed retry (`lib_try_game_command_common`) gated `>= 4.0` since 2026-09-20 | a put/drop the library can **complete** beats the fail message; one it would refuse, or with unresolvable nouns, leaves the message. Pre-4.0 has no prefixed canonical retry | **Ported** (probes `FM4`–`FM7`; pre-4.0 `make_3738_pretryprobe.py`, `make_39_pretryprobe.py`). Witnesses: *TheADRIFTProject* (author's 2004 transcript), Wax Worx `get * head`. |
| Single-object library success vs failing explicit-verb task | library acts (`take rock` takes it) — run390's behaviour | run400: fail message (`TFAIL`), and no `take`→`get` rewrite; run390: library take. `wear` prints the fail message in both | **Documented, not fixed** (`FM4`; `make_39_fwprobe.py`). Only the run400 take half diverges; zero corpus impact. A *passing* task claims take/wear/put outright in run390. |
| Zero-length always-restarting event | see next row | fires once at game start, never restarts — both Runners | **Fixed 2026-08-02**, superseded by the shape table below (probe `EV`, `make_39_fwprobe.py` `b`). |
| Zero-length events, the whole shape table | **PORTED 2026-08-02** (`scevents.cpp`: `evt_is_zero_length()`, ES_WAITING/ES_RUNNING handling, finish gate) | starter decides: (1) load-start → start+finish on turn 0, once; (2) clock-start (delay / restart-after-delay) → StartText then **parks** for ever (LookText stays, FinishText/TaskAffected never run); (3) task-start → start+finish on the trigger turn, once. RestartType=1 restarts then parks; RestartType=2 on immediate/task starter goes quiet | **Ported** (probes `EV2`–`EV5`). Only live corpus instance of (2): Del Sol EVENT 11 (room-gated). |
| When immediate events start relative to the opening room description | **FIXED 2026-08-02**: `evt_start_load_events()` (silent, +1 clock) before `DispFirstRoom`, `evt_finish_load_events()` after | start during load, before the description: LookText in it, StartText never seen; finish half lands under it. Both Runners | **Ported** (probes `EV5`, `EV6`; `make_39_fwprobe.py` `e`). Rolling lengths at load moves them ahead of `battle_start()` — accepted, unverifiable (§1). `scdump.cpp` EVENT line ends `texts=SLF`. |
| Where an event's LookText sits inside the room block | **FIXED 2026-08-02**: after `lib_print_room_contents()`, joined via `pf_buffer_join()` (`scprintf.cpp`) | dead last, after objects and characters | **Ported** (probes `EV7`/`EV8`, run390 agreeing). |
| Startup event tick, pre-3.9 games | **FIXED 2026-09-04**: startup tick and the StarterType 2 `+1` gated `>= TAF_VERSION_390` (scrunner.cpp / scevents.cpp) | run370/380 tick events only from the generaltasks tail: a delay-N event starts on turn N | **Ported** (run380 `haunt.taf`, `Adven_1_haunt.rtf`). See `notes/WINE-TRANSCRIPTS-TODO.md` haunt section. |
| Administrative turns, pre-3.9 games | **FIXED 2026-09-04**: `lib_set_admin()` makes `is_admin` 3.90+ only | run370/380 tick NPCs+events after every command with output (only save/restore/restart/quit bypass); run390 flags only history/score/count/information/end/turns | **Ported** (run380 `haunt.taf` turn 83). **OPEN:** `hint`/`help`/`clear`/`where` under run390 (not in its 468219 list) unmeasured. |
| Pre-parse verb rewrites (`take`→`get` etc.), pre-3.9 | BUILTIN rewrite table in `pf_filter_input()` (`scprintf.cpp`), after SYNONYMs, version-gated per row | run380: `everything`→`all`, `slap`→`hit`, `take`→`get`, `except`→`but` (441C3F..441C72); run370 only the first two; run390 none; run400 take→get only inside its get handler. So take→get is **3.80-only** and cuts both ways | **Ported 2026-09-04** (jb2000, Crime_Adventure, great — 0 differences). |
| Object-ambiguity prompt `Which <term>.  <list>?`, pre-3.9 | **PORTED 2026-09-04 for 3.7/3.8**: `lib_co_ambiguity_prompt()` after the tick when no task ran, gated `< TAF_VERSION_390` | run380: co() every command; >1 present object answering to the term (unless the Prefix's last word was typed) replaces the whole turn's output unless a task ran — state changes stand. run370: co() only via therest()/insides(). run390: only from characters()/sitstand(), plus handler-scoped take/drop/wear prompts | **Ported** for 3.7/3.8 (mikes `Adven_8_mikes.rtf`). run390's handler-scoped prompts **not ported**. |
| An event's **length** when `Time1 ≠ Time2` | **FIXED 2026-08-17**: `scr_randomint_exclusive()` (`scutils.cpp`) at the length roll, restart re-roll and StarterType=2 delay; draws even when `hi <= lo` | exclusive upper bound: `1..3` gives only 1 or 2, both Runners | **Ported** (§10; config `EL`, `make_39_evlenprobe.py`). Fixed in callers, not `scr_randomint`. |
| Put-family precedence, 3.9 half | same port, ungated | run390 agrees with run400 | **No divergence** 2026-08-02 (`make_39_fwprobe.py`). |
| Container-listing style (postfixed vs prefixed) | **FIXED 2026-08-03** | purely a count: 1–2 contents → postfixed ("An X is inside Y."), 3+ → prefixed ("Inside Y is …"); no static/dynamic test | **Ported** (run400.txt + real transcript; *It's Easter, Peeps!*). |
| `take <object loose in the room>` | version-gated 2026-08-15 via `lib_is_version_400()` (`sclibrar.cpp`) | pre-4.0 "You pick up the X."; 4.0 "You take the X."; container case identical | **Ported** (all four Runners; probes `p37pos`, `p39held`, `TK`). |
| What `g`/`again` echoes | nothing extra | the same, unless *References in brackets* (off by default, never persists — see correction in the ShowRoomDesc row) is ticked | **No divergence** 2026-08-15. Repeat words `!!`, `again`, `last`, `previous`, `!`, `g` (00089FE2) — `last`/`previous` ported (`scrunner.cpp`). Probe note: `last` right after `previous` repeats the word `previous`. |
| End-of-game score summary | **PORTED 2026-08-15**: `task_print_end_game_summary()` (`sctasks.cpp`) | `You scored N out of the maximum M!` / `That is P% of the game!` (`Int(score*(100/Max))`, truncates) and, on a win only, `Well done - you scored maximum points!` or `You finished (M−N) points short.`; "just stop" (Var1=3) prints nothing. 4.0 guards on `MaxScore > 0` (see MaxScore 0 row) | **Ported** (probes `SC`/`SC0`, `make_39_endprobe.py`; run380 `microwaveman`, run370 `castle`). Corroborated by author ALRs (`panic.taf`, French games). |
| `(Your score has increased by N)` | **PORTED 2026-08-15**: flag starts FALSE; `notify` command toggles | run370/380/390 never print it; run400 only with the (user-persisted, default-off) "Notify when score changes" menu item; TAF `NoScoreNotify` ignored | **Ported.** |
| Pre-4.0 loss and death messages | death: `lib_get_death_message()` (`sclibrar.cpp`), used by `task_print_end_game_message()` and `battle_kill()` | pre-4.0: `"I'm afraid " & I/you & " am/are dead!"` by perspective; run400: fixed `I'm afraid you are dead!` (battle deaths too). `Better luck next time.` absent before 3.9 | **Ported 2026-08-15** (run370 `castle`, run380 `wrecked`, run390 `p39end`, Perspective flipped). Loss branch unreachable pre-3.9 (`sctafpar.cpp` synthesizes Var1 0 or 2 only). |
| Where a pre-4.0 EndGame WinText goes | **PORTED 2026-08-15**: `pf_undo_auto_break()` (`scprintf.cpp`) for pre-4.0 | `Form1.endmessage` appends with no separator (0005DDF8): pre-4.0 flows it onto the task text; 4.0 has already terminated the block | **Ported** (run390 `scwin`, run380 `microwaveman`, run400 `ptbad`). |
| Empty `WinText` → `Congratulations!` | **FIXED 2026-08-15**: prints nothing | nothing — `Congratulations!` is a status-bar caption (0x000571AC) | **Ported** (run400 `TheAmulet`, run370 `castle`, run390 `ECOD3`). Win markers in `run_v4_walkthroughs.sh` now point at game text. |
| Summary when `MaxScore` is 0 | version-gated 2026-08-15 | 4.0 suppresses; pre-4.0 prints it anyway as 100% (and a negative "points short") | **Ported** (run390 `ECOD3`, `chicago`). 3.7/3.8 untested but unexposed — treated like 3.9. |
| When the end-of-game message is printed | **PORTED 2026-08-15**: `pending_endgame` (`scgamest.h`), emitted by `task_print_end_game_message()` at end of turn (`scrunner.cpp`); first EndGame wins | end of turn: `Form1.checkx` → `evaluate` → `endmessage` (0x0005C681); later actions in the turn land before the summary | **Ported.** Witnesses: `chicago.taf` task 23 (run390 75 vs 65); run380 `marooned`. |
| A trailing `AdditionalMessage` the Runner never prints | **PORTED 2026-08-15**: `task_suppresses_additional_message()` (`sctasks.cpp`) + `pf_ends_with_double_space()` | **run380 only** drops it when the turn's text already ends in two spaces (a typo'd combined `If`, 0004D001–0004D074); run370/390/400 print it | **Ported.** Witness: `jb2000.taf` task 14 (42 trailing spaces). |
| A task's `ShowRoomDesc` prints the room **name** | heading, then description | identical when "Room names in descriptions" is on (`Sub_20_64`, 000723EA) | **No divergence** 2026-08-15 — earlier measurement was taken with the box unticked. **Correction 2026-08-24:** the setting *is* persisted (`showshortroom` in `pfx/user.reg`) and defaults **ON** (`Proc_21_24_4747F8`); the prefix had a persisted untick. Runner puts one newline before the heading where Scarier opens a paragraph (§3's accepted joined-paragraph difference). |
| `some` prefix normalization; take-from-container prefix | **PORTED 2026-08-15**: `>= TAF_VERSION_390` guard on the `some` arm of `lib_print_object_np()`; `parent == -1 \|\| lib_is_version_400()` chooses the printer in the take handler | (i) `some`→`the` from 3.9; 3.7/3.8 normalize only `a`/`an`. (ii) the *taken* object's prefix is raw pre-4.0, normalized in 4.0; the container is always normalized | **Ported** (one game across three versions via generator upconversion: `microwaveman.taf`; run370 edited `arlo.taf`). |
| Blank lines between ending text and summary | **PORTED 2026-08-15** | unconditional `vbCrLf`s: empty WinText still costs a blank line; loss/death open with two | **Ported** (run400 `microbe_willie`, `QuestI`; run390 `ECOD3`). Harness `cat -s` means only presence of a blank line is tested. |
| TAF 3.8 object Size/weight class | `SizeWeightClass` kept verbatim, pooled burden (`obj_get_burden`/`obj_get_player_burden_limit`, `scobjcts.cpp`); `SizeWeight` normalized to `22` (`\|V380_OBJECT:_SizeWeight_\|`) | single pooled burden, class costs `0→1 1→3 2→7 3→3 4→7`, limit `MaxCarried`. Carried container's contents free; `Capacity` = plain object count, 0 = full; only a *held* dynamic container can be filled ("You are not holding a saucepan."), statics exempt; refusals "Your hands are full." / "The box is full." | **Ported 2026-08-03** (run380 probes; `~/adrift-battle/runner/wine/taf38schema.py` + `make38probe.py`). gen390's table is off by one step. See `test/adrift4/notes/Marooned_walkthrough.md`. |
| Matched task with FAILING restrictions swallows the command | prints FailMessage, ends turn (even a placeholder `x`) | identical | **No divergence** 2026-08-03 (`wrecked.taf` task 96). Recorded so the next `x`-shaped mystery isn't re-investigated. |
| ADRIFT 4 `$RestrMask` operator precedence | **FIXED 2026-08-03** (`screstrs.cpp` `restr_expr()`): equal precedence, left-associative | `A`/`O` equal precedence, left-assoc: `#O#A#` = `(1 OR 2) AND 3` (`Sub_20_57`, 00055CAC..00055EB9) | **Ported** (P-code). Witness: *Three Monkeys One Cage* T21 `winnable` self-check. 20 corpus games author mixed levels. |
| Task command typed **outside** the task's `Where` rooms | **PORTED 2026-08-10**: `run_where_refusal()` (`scrunner.cpp`), last in `run_all_commands()`, over `task_is_room_refused()` (`sctasks.cpp`) | pre-4.0: `You can't do that here.` (3.7/3.8) / `…here!` (3.9), consumes a turn, only when the room list alone blocks it; run400: DontUnderstand | **Ported** (§5). Regression `make -f Makefile.headless wheretest`. |
| Completed non-repeatable task retyped, **empty** RepeatText | **PORTED 2026-08-10**, gated `< TAF_VERSION_400` | pre-4.0: `You have already done that.`, consumes a turn; run400: DontUnderstand | **Ported** (§5). Distinct from the `*`-task row above. |
| Completed non-repeatable task retyped, **non-empty** RepeatText | **PORTED 2026-08-10**, ungated | prints the RepeatText, consumes a turn — every version incl. 4.0 | **Ported** (§5). `SCR_DUMP_TASKS` `rpt=` column. |
| Perspective 2 in a pre-4.0 game | **PORTED 2026-08-10**: `lib_get_perspective()` returns second person for any non-zero Perspective pre-4.0 | pre-4.0 has only first (0) and second (1–3) person | **Ported.** No pre-4.0 corpus game authors Perspective 2. |
| Remaining actions after an action that ends the game | in-line actions run (print muted); `task_run_task()` returns early once the game is over | in-line actions still run; an Execute-Task after the ending is a no-op (`Sub_20_22` opens `If gameOver > 0 Then Exit Sub`, 0005F750) | **Ported 2026-08-09** (probe `EG`, cells scorefirst/scorelast/execlast/printfirst/printlast). *Three Monkeys* 98/100 visible ceiling is a game fact. |
| Article for an **empty `Prefix`** | `lib_print_object_np()` defaults `the `, `lib_print_object()` defaults `a ` | identical (4.0) | **No divergence** 2026-08-14 (witness `relojero.taf` in run400). Pre-4.0 empty-prefix on the from-container path unmeasured, unexposed. |
| `getdynfromroom()` | **PORTED 2026-08-17**: evaluated only in `run_task_run_by_index()` (scrunner.cpp), over alternate commands | by-index runner (`Sub_20_22`) only; squeeze spaces, require `#%object%=getdynfromroom(` and raw `)`; first non-static object directly in the named room; run390 has none | **Ported** (probes `GD1`–`GDR`; §9). **Deliberate:** two run400 fenceposts not reproduced (last room unreachable; rooms with spaces unreachable). Only user: Humbug EVENT 45 / TASK 310. |
| Can a task action move a **static** object? | **PORTED 2026-08-17**: `task_move_object()` returns early for statics | never, for any selector or destination | **Ported** (probe `SM`; §9). `evt_move_object()` is the only mover of statics. |
| Weight of an object an **event** puts in the player's hands | recomputed from positions: counted, droppable | half-moved: listed by `inventory`, but `count` stays 0 and `drop` refuses; stale in the other direction too | **Deliberate divergence, not ported** (probe `SM`; §9) — would need a second held-state in saves to inherit a capacity bug. Statics weigh 0 (`obj_get_size`/`obj_get_weight`). |
| Which characters join "X, Y and Z are here." and order | **PORTED 2026-08-17** (`lib_print_room_contents()`): any text ending `" is here."` joins, suffix trimmed, sentence first | loader (00091EDF) rewrites `#` to `<name> is here.`; lister (00072944) tests `Right(text,9) = " is here."`, exact and case-sensitive | **Ported** (probe `NH`; §9). **Deliberate:** no blank line before the joined sentence. |
| A walk's `StoppingTask` | **PORTED 2026-08-17**: `npc_tick_npc()` (scnpcs.cpp) calls `npc_start_npc_walk()` every stopped tick | held at the top of its cycle; un-completing starts a fresh cycle (arrives at stop 0 that turn) | **Ported** (probe `S`, `make_400_walkprobe.py`; §9). 4.0 only — V390 cannot un-complete a task. |
| Worn objects vs `MaxCarried` (3.7/3.8 pooled burden) | **FIXED 2026-08-23**: `lib_carried_burden()` skips worn | worn is free | **Ported** (`pworn`, `qworn`). |
| `count` under the 3.8 pooled model | **FIXED 2026-08-23** | `You have 0 objects.  The most you can hold is 2.` (no singular; `I have` in first person) | **Ported** (`pcount1`). |
| Article in the **wear** success message | **FIXED 2026-08-23**: pre-3.9 `lib_print_object`, 3.9+ `lib_print_object_np` | run370/380 indefinite printer with own prefix (`You put on a rusty w3.`, `a apple`); run390/400 normalize | **Ported** (`pwear`, `pwearv`). |
| 3.9 leaky running carried total | **FIXED 2026-08-23**: `obj_uses_running_load()` true for 4.0 only | run390 adjusts per handler, arithmetic exact; run400's `Proc_21_54` leaks | **Ported** (`p39leak`, `p39lim`). |
| Size refusal `" at the moment"` | **FIXED 2026-08-23**: never | never — `" hands are full."` has the period baked in (all four binaries) | **Ported.** `is_portable` removed from `lib_object_too_large()`/`lib_object_too_heavy()`. |
| Weight refusal wording | **FIXED 2026-08-23**: `lib_print_too_heavy()` | run390 "That is too heavy for you to carry."; run400 "The X is too heavy for Player to carry at the moment." | **Ported** (`p39wt`). No `" are too heavy"` in any binary. |

## 5. `Where` = "No rooms" on a player-typed task — SETTLED 2026-08-04, NO divergence

A task's `Where` room list Type is `ROOMLIST_NO_ROOMS = 0`, `ONE_ROOM = 1`,
`SOME_ROOMS = 2`, `ALL_ROOMS = 3` (`scprotos.h:215`). Scarier's
`task_can_run_task_directional()` never runs a Type 0 task, and run400 agrees:
**Type 0 means "runnable nowhere"** — the ADRIFT authoring idiom for "disable
this task".

- **Probe.** `test/adrift4/harness/make_400_whereprobe.py` — two rooms, tasks
  `alpha` (Type 0), `beta` (Type 3, control), `gamma` (Type 1, room 2). Pack
  with `python3 taftool.py pack p4WHERE.plain <donor>.taf p4WHERE.taf` (the
  donor supplies the "Wild" trailer). run400: `alpha` / `gamma` →
  "I don't understand.", `beta` → `BETA FIRED.`
- **Game-level confirmation.** *The Plague - Redux* with `#StartRoom` patched
  `0` → `15` (plain-body line 80, after the `bd d0` separator) answers `f`
  with "That didn't make any sense!" in run400 — same as Scarier.
- **Corpus casualties (faithful, do not patch):** *The Hangover* (`give the
  doctor some french fries`, `give approval notes to platypus`; confirmed in
  run390; ceiling 5/7) and *The Plague - Redux* (whole `[F]/[E]` combat
  system; unfinishable as shipped).
- **Diagnostic:** when a walkthrough command is flatly not understood, dump
  the task table and read `where=` before suspecting the parser.

### Follow-up (2026-08-10): the two task refusals — "You can't do that here!" and "You have already done that." — PORTED

Pre-4.0 Runners have a dedicated out-of-room message; run400 does not. The
strings live in the .exe as UTF-16 (`strings` misses them; decode as
`utf-16-le`).

- **Probes.** `test/adrift4/harness/make_39_whereprobe.py` (3.90, XOR codec,
  args `out.taf [perspective] [variant]`; variant `e` adds a one-turn `TICK.`
  event to show turns; rooms joined north/south). Tasks: `alpha` Type 0,
  `beta` Type 3 control, `gamma` Type 1 room 2, `delta echo` Type 2 room 2,
  `epsilon` non-repeatable empty RepeatText, `zeta` always-failing restriction
  with empty FailMessage, `eta` non-repeatable with RepeatText, `theta` Type 1
  room 1 non-repeatable (both blockers). `make_400_whereprobe.py` gained the
  4.0 pair `delta` (empty RepeatText) / `epsilon` (RepeatText "EPSILON REPEAT.").

| Runner | out-of-room task command | Type 0 task | silently-failing restriction | done non-repeatable |
|---|---|---|---|---|
| run370 / run380 | "You can't do that here." | — | — | — |
| run390 | "You can't do that here!" (Types 1 **and** 2) | same refusal | DontUnderstand | "You have already done that." |
| run400 | DontUnderstand | DontUnderstand | — | DontUnderstand (empty RepeatText) |

- **Room refusal rule:** pattern matched and the `Where` list is the only
  blocker. Restrictions never raise it; anything the library answered
  suppresses it. Punctuation `.` for 3.7/3.8, `!` for 3.9; leading word
  follows `Globals/Perspective` ("I" for `LIB_FIRST_PERSON`, else "You").
  **Consumes a turn.**
- **Already-done rule:** an authored RepeatText displaces the message (run390
  `eta`); RepeatText survives into 4.0 but the bare message does not. When
  both blockers apply the **room wins** (`theta`). Consumes a turn;
  perspective applies ("I have already done that.").
- **Ported** as `run_task_refusal()` (`scrunner.cpp`), last in
  `run_all_commands()`, over `task_is_room_refused()` / `task_is_done_refused()`
  (`sctasks.cpp`); `task_can_run_task_directional()` split into
  `task_state_allows_run()` / `task_where_allows_run()`, with cached
  `task_is_repeatable()` / `task_repeattext_is_empty()`. Room half checked
  before done half per task (reproduces `theta`); the room predicate does not
  consult task state. **Empty input returns early** (otherwise a bare `*` task
  out of room turns every blank press-a-key line into a refusal).
- **2026-09-13:** below 4.0 the already-done claim moved earlier —
  `run_spent_task_390()` claims in `run_all_commands()` *before* movement and
  the library (where run390's `checktask` does), for any pattern incl.
  wildcards, with RepeatText or the load-time default. `run_task_refusal()`'s
  done arm is now a pre-4.0 fallback plus the 4.0 RepeatText path. See §2.
- **Regression is synthetic** (no solved route exercises either refusal):
  `make -f Makefile.headless wheretest` replays
  `harness/where_refusal_script.txt` against the probe built at Perspective 1
  and 0, diffing `where_refusal_expected.txt` / `..._1p_expected.txt`; part of
  `make test` and `make sanitize`. A third build at Perspective 2 types `i`,
  and `where_refusal_3p_expected.txt` must stay **byte-identical** to
  `where_refusal_expected.txt` — the regression for the pre-4.0 two-perspective
  port (§4).
- **Known gap, accepted:** the 3.7/3.8 period wording (live: run370 *Castle
  Quest*, run380 *Marooned*; gated `version < TAF_VERSION_390`) has no
  synthetic test — the generator writes 3.90 only.

## 6. ADRIFT 3.70 — every inferred semantic measured, SETTLED 2026-08-04

`V370_PARSE_SCHEMA` (`sctafpar.cpp`) was inferred from the two surviving 3.70
games; every guess was then measured against `run370.exe`.

- **Probe method.** 3.70 `.taf` = CRLF plaintext XOR'd with the VB6 PRNG from
  seed `0x00a09e86`, indexed from offset 0, no signature/trailer — round-trips
  losslessly, length may change. `~/adrift-battle/runner/wine/taf37schema.py`
  (= `taf38schema.py` + the 3.70 TASK record and trailing 17-word block)
  records each field's line index, so a probe is "parse, `L[idx] = value`,
  re-encode". Probes in `probes37/`, all patching `castle.taf`. Turn off
  Options → **Auto complete** first.

| probe | question | answer |
|---|---|---|
| `mkprobe37f.py` | extra header integer? | the **winning task**, 0-based |
| `mkprobe37.py` | flat movement destination list | `0` hidden, `1` held by player, `2` player's room, `3+n` room n |
| `mkprobe37c.py` | object initial-position list | `0` hidden, `1` held, `2` in/on `#Parent`, `3..3+R-1` room n, `3+R` worn by player; beyond = out of play |
| `mkprobe37b.py` | `#Parent` for held/worn start | **ignored** — always the player; 3.7 cannot start an object on an NPC |
| `mkprobe37d.py` | burden model | identical to 3.80: pooled, class costs `0→1 1→3 2→7 3→3 4→7`, capacity `#MaxCarried`, "Your hands are full." |
| `mkprobe37e.py` | 17 renameable built-in command words | **additive** (old word keeps working) — maps to 4.0 synonym `{Original: author's word, Replacement: standard word}` |

- **Fixed in `sctafpar.cpp`:** `parse_fixup_v370_movement()` now maps "held by
  the player" to `var3 = 0` (1 is the referenced character); the shared
  3.8/3.7 initial-positions fixup normalises an unset `#Parent = -1` on a held
  or worn object to the player (was "object worn by nonexistent NPC, -2";
  also fixed `tra.taf`'s `i`, now byte-identical to run380).

## 7. Whitespace between adjacent `[]` / `{}` groups — FIXED 2026-08-04, ARBITRATED LIVE: NO divergence

Two node types, two rules, confirmed in run400:

- **Invented separator between adjacent groups** (`NODE_JOIN`,
  `uip_match_join()`, `scparser.cpp`): eats whitespace if present, never
  fails — a space is allowed, not required. Evidence: `[open/pull/push]{the}{wooden}[door]`
  must accept "open door"; `[s]{outh}{ /-}[w]{est}` spells its space out, so
  adjacency must not imply one. Fixed *ImagiDroids* exits
  (`{go/walk/move}[n/escape/out]{orth/out}`, `[d/out/in]{own}`) and *The
  Forum*'s `{wooden}[clog]{s}`.
- **Whitespace the author wrote** (`NODE_WHITESPACE`,
  `uip_match_whitespace()`): **required** in the input, except at end of
  string (a trailing `space + optional group` may be omitted).
- **Probe:** `test/adrift4/harness/make_400_wsprobe.py`, run400 vs Scarier
  identical on every cell:

| input | pattern | result |
|---|---|---|
| `alpha` / `al pha` / `al` | `[al]{pha}` | fires |
| `beta` | `[be] {ta}` | **I don't understand.** |
| `be ta` / `be` | `[be] {ta}` | fires |
| `gamma` / `ga mma` | `[ga][mma]` | fires |

## 8. The 3.9/3.8 immediate-restart fixup — ARBITRATED LIVE 2026-08-04: run390 re-arms **silently** and keeps the **full** period

- **Rule (pre-4.0, `RestartType=1`):** the restart is **silent** (no
  StartText) for all three starter types, and the period is the **full**
  authored length. Only `RestartType=2` (restart after delay) re-runs the
  start actions, via `ES_WAITING` and the normal start. run400 on the same
  event prints "E FINISH.  E START." each period — the text is version-gated,
  the timing is not.
- **Probes:** `test/adrift4/harness/make_39_evtimeprobe.py` (self-packing
  V390; variants base / `b` Time 1 / `c` starter 2 / `e` starter 3 / `f`
  StartText+LookText "Priest Coughs" shape / `d` restart 2) and its 4.0 twin,
  config `EV9` in `make_arena_probe.py`.
- **Ported:** `evt_fixup_v390_v380_immediate_restart()` (`scevents.cpp`) calls
  `evt_start_event (game, event, TRUE)` — `silent` suppresses StartText only —
  and does not touch the clock; the length roll comes from
  `evt_start_event()` alone (no second `scr_randomint()`, which would churn
  the stream). Obj1's move and the start resource on a 3.9 restart are
  **not measured**.
- **Don't trust the *Panic!* walkthrough** (`panic.taf`): it shows the priest's
  cough 66 times, but run390 on the same file prints it once. It was the
  wrong oracle.
- **Refuted: posture affects event visibility.**
  `test/adrift4/harness/make_39_evseeprobe.py` (surface `chair` and container
  `crate`, `SitLie = 3`): run390 prints the room-limited event's FinishText
  every turn whether on, in or beside them, and LookText in `look` while
  parented. `evt_can_see_event()`'s room-only check is right.
- **Footgun (thetest):** `#` comment lines in a route are not free where the
  game has keypress waits — `os_ansi.cpp` only skips comments at a line
  prompt. `test/adrift4/harness/thetest_rederive.py` re-grows its
  dice-rolling pad blocks.

## 9. Probe backlog (2026-08-17) — the code-comment TODOs; all five closed the same day

Also from the sweep: all of run370–run400 say `I don't think X would be a very
affective weapon!` (ported, from the string tables).

- **Static objects moved into the inventory — SETTLED LIVE AND PORTED.**
  run400, arena probe `SM`:
  1. An event-placed object contributes nothing to `count` totals (dynamic or
     static), so a static never weighs anything; `obj_get_size` /
     `obj_get_weight` keep 0.
  2. **A task action cannot move a static, by any selector** (run400's mover
     skips `Static = 1` first; completion text still prints). Ported into
     `task_move_object()`. So `evt_move_object()` is the only place a static
     moves.
  3. `deliberate:` run400's event mover "held by player" is a half-move —
     listed by `inventory` but `count` stays 0 and `drop` refuses it; moving
     a hand-taken object away by event leaves its size occupied forever.
     Scarier recomputes from positions and stays self-consistent. Not ported.
- **`#` in-room text for NPCs — SETTLED LIVE AND PORTED** (§4 row). The
  **loader** (@00091EDF) replaces `#` with `<name> is here.`; the lister
  (@00072944) tests `Right(text, 9) = " is here."`:
  1. Author-written full text ending " is here." joins the shared sentence
     with the text minus those nine characters (`The stranger is here.` →
     *The stranger*).
  2. Exact and **case-sensitive** (`Golf is here!`, `Hotel IS HERE.` print
     verbatim in the second group).
  3. The joined sentence comes **first**, before characters with their own
     text. Empty text drops the character.
- **Walk `StoppingTask` — SETTLED LIVE AND PORTED** (§4 row). A completed
  stopping task holds the walk; un-completing it starts a **fresh cycle**,
  arriving at stop 0 that very turn (neither pause nor finish). Probe `S` of
  `make_400_walkprobe.py`, three sessions. Ported as a re-arm via
  `npc_start_npc_walk()`. 4.0-only question: V390 has no "unset task" action.
- **`getdynfromroom` — SETTLED LIVE AND PORTED** (§4 row; probes GDA..GDR).
  Not a matcher: run400 evaluates it only as a preamble to running a task
  **by index** (`mdlSpreadTheLoad.Sub_20_22`), over that task's **alternate**
  commands only; Scarier's `run_game_functions()` scan-every-command pass had
  no counterpart and is gone. Selection: squeeze all spaces, require
  `#%object%=getdynfromroom(` and a **raw** trailing `)`, compare the argument
  case-insensitively to the room `Short`, take the first non-static object
  directly in that room. 4.0 only. `deliberate:` two run400 fenceposts not
  reproduced — the last room is unreachable, and so is any room whose name
  contains a space. Sole corpus user: Humbug (EVENT 45 runs TASK 310 by index).
- **Negative resource lengths in 4.0 TAFs — DECODED AND FIXED.** `-N` is a
  back-reference to entry N (1-based) of the resource table: distinct
  resource **names** in parse order, trailing `##` loop flag stripped,
  zero-length (named-but-unembedded) resources counted too. Matches all 535
  negative records in the corpus. Scarier resolves by name (same entry); the
  one place name and index diverged was a live bug, now fixed — every
  embedded resource lands on its own first byte.
- **Lesson:** "no corpus exposure" by grepping *execute task* actions misses
  an event's `TaskAffected`, which also runs a task by index.

## 10. The event-length roll when `Time1 ≠ Time2` (raised, measured and ported 2026-08-17)

- **Rule:** every event-timing roll uses an **exclusive** upper bound,
  `lo + Int(Rnd*(hi-lo))` — the event length, the StarterType=2 start delay,
  and the restart-after-delay re-roll — in **both** run400 and run390. (The
  only inclusive `Rnd` form in run400 is `Express.bas:648`, the author-facing
  `rand(x,y)` expression function.)
- **Probes:** config `EL` in `make_arena_probe.py` (run400, three sessions)
  and its V390 twin `test/adrift4/harness/make_39_evlenprobe.py` (run390, two
  sessions; docstring carries both Runners' numbers). Three events with
  `1..3` ranges separate exclusive {1,2} / inclusive {1,2,3} / `+1` {2,3}:
  every draw in {1,2} in all families, no version split.
- **Ported:** `scr_randomint_exclusive()` (`scutils.cpp`) at
  `evt_start_event()`'s length roll, `evt_finish_event()`'s
  restart-after-delay re-roll, and `scgamest.cpp`'s load-time StarterType=2
  delay; `scr_randomint` itself unchanged. It draws once even for
  `hi <= lo`, mirroring VB6's unconditional `Rnd`, so `Time1 == Time2` events
  keep their stream cadence.
- **The corpus witness:** *Provenance* EVENT 7 (`Time1=0 Time2=1`, runs
  `#Run Gender Task`) — length is always 0, so turn 1's `i` shows the brown
  tweed suit (4/4 live run400 sessions). Wins on the default `SCR_SEED=1`.

## Closure log

How each item closed, one entry per batch. The *rules* live in §4 and at the fix
sites; this is the index of dates, probes and the lessons worth not re-learning.
Full prose: `git log --follow -p` on this file (pre-2026-09-28 path
`terps/scarier/RUNNER_TESTS_TODO.md`).

### 2026-08-01/02 — the original five items (§1, §2, §3a, §3b, §4)

- §1 battle, §2 wildcard/event dispatch, §3(a) V390 fixups, §3(b) Les Feux, §4
  body-part/division rows: all closed 2026-08-01; see those sections.
- `TheADRIFTProject` was **not** a zero-word-`*` problem: run400's put-in/put-on
  family runs ahead of a matched-but-failing task when the library can complete
  it (probes `ST`, `FM`–`FM7`, `.tas` transplant). Ported via
  `PRIORITY_COMMANDS` + deferred refusals.
- 3.9 halves (`make_39_fwprobe.py`): wear fail-message and put precedence agree
  with run400; take is a version split (run390 runs the library take).
- Zero-length events: three behaviours by starter (load → once; clock → start
  then park forever with LookText; starter task → once); RestartType=1 restarts
  then parks. Probes `EV2`–`EV5`; ported in `scevents.cpp`
  (`evt_is_zero_length()`).
- Immediate events start at load, before the opening description, in BOTH
  Runners (`EV6`, fwprobe `e`): ported as `evt_start_load_events()` /
  `evt_finish_load_events()` around `DispFirstRoom`. Event LookText prints last
  in the room block (`EV7`/`EV8`), ported via `pf_buffer_join()`.

### 2026-08-02 — "held by the player" reaches into a CLOSED carried container

- run390 (`make_39_heldprobe.py`): openness is never consulted; matches
  `restr_object_in_place()` case 1/7. No change.
- 3.9 `InitialPosition = 2` takes a 0-based container-sublist `Parent`, as in
  `gs_create()`.

### 2026-08-03 — size/capacity matrix: two globals, and capacity is a volume

- GLOBAL `iUnk1`/`iUnk2` are the **size and weight scale bases**
  (`#SizeMultiple`/`#WeightMultiple`, V390+V400; 3.8 falls back to 3). Dimension
  = `base ** index`; player limit = `tens × base ** units`. The earlier "run390 is
  stricter" anomaly was probes writing 0 for both.
- Container Capacity is a **volume** spent by direct contents (not recursive,
  unlike weight). "too big to fit" = size > total volume; "can't fit … at the
  moment" = size > remaining. Both Runners agree (`make_sizeprobe.py cap2/cap3`).
  Ported: `obj_get_container_free_space()`, `lib_put_in_backend()`;
  `make -f Makefile.headless capacitytest`.
- Method: run400 `Help → Debugger…` Player tab shows decoded Size/Weight
  (opens without prompt when password is `"    Wild    "` or empty). Auto
  complete defaults ON — turn it off, read the echo. `Edit Mode` kills run400.
- Perspective-2 3.9 = second person (ported 2026-08-10). **OPEN (cosmetic):**
  run400 writes "%player% **put**" where Scarier writes "puts" in third person.

### 2026-08-03 — container-listing style is a count of two

- run400 listing helper @0006A418: 1 or 2 contents → "X (and Y) is/are inside
  C."; 3+ → "Inside C is …". Static/dynamic is irrelevant. Confirmed by the
  shipped `EasterWalk.txt`. Ported in `lib_list_in_object()`.
- Lesson: an author's ALR that only matches Runner wording is an independent
  witness to that wording.

### 2026-08-04 — §8 closed (see §8)

- 3.9 immediate restart is silent and keeps the full period; posture does not
  hide event text. Probes `make_39_evtimeprobe.py`, `EV9`,
  `make_39_evseeprobe.py`. Recipe: `thetest_rederive.py` regrows
  try-until-it-happens blocks by prefix replay.

### 2026-08-14 — the empty-`Prefix` article is not a divergence

- `relojero.taf` in run400: both empty-prefix defaults in `sclibrar.cpp`
  (`"the "` np path, `"a "` otherwise) are faithful.
- Method: when a localised game is the witness, run *that game*. An ALR pair
  that does **not** fire is not evidence.

### 2026-08-15 — the take wording is a 4.0 rewording (all four Runners)

- 4.0: "You take the rock." / "There is nothing worth taking here." (no "else"
  form); pre-4.0: "You pick up …" / "There is nothing to pick up here." Probe
  `TK`, `p39held.taf`, `p37pos.taf`. Ported via `lib_is_version_400()`.
- Method: when a row names a pair of Runner strings, UTF-16LE-census all four
  binaries before designing the probe.

### 2026-08-15, second batch — end-of-game score summary and score notification

- Summary from `Form1.endmessage`: `Str()` spacing, `Int(score * (100 /
  MaxScore))` float-truncating (3/8 → 37%). Not version-gated (`SC`/`SC0`,
  `make_39_endprobe.py`). `task_print_end_game_summary()`.
- "(Your score has increased by N)" is a Runner menu setting (`NotifyScore`,
  default off), not the TAF global — `scgamest.cpp` starts it FALSE.

### 2026-08-15, second batch — the pre-3.9 half of the same summary

- Replays (`microwaveman.taf` run380, `castle.taf` run370) confirm the summary
  pre-3.9. *Replay a short corpus game rather than author a probe when one
  exists.*
- "Congratulations!" is a **status-bar** literal, never game text — removed
  (win markers that relied on it re-pointed).
- The `MaxScore > 0` guard is 4.0-only; run390 prints "That is 100% of the
  game!" at 0/0 and negative shortfalls (`chicago.taf`, `ECOD3.taf`).

### 2026-08-15, third batch — the ending moves to the end of the turn

- EndGame only sets a byte; `Form1.endmessage` runs after the whole turn, so
  later actions still change the score. Ported as `pending_endgame` printed by
  `task_print_end_game_message()`. The EndGame handler @0008D66E permutes Var1
  0..3 → 1,3,2,4 and writes unconditionally (last EndGame in a turn wins).
- False lead (jb2000 missing sign-off ≠ "ending silences the task") — see the
  eighth batch. *When a fix makes unrelated goldens lose text, suspect the fix.*

### 2026-08-15, fourth batch — the endgame message is concatenated

- No separator in any version (`out & WinText & vbCrLf`; losses `& vbCrLf &
  vbCrLf & msg`). 4.0's turn text is already terminated; pre-4.0's is not
  (run380 runs "You win the game.You have…" together).
- Ported: `pf_buffer_paragraph_line()` records `auto_break_at`;
  `pf_undo_auto_break()` removes it for pre-4.0 endings. Measured on ptbad,
  microbe_willie, QuestI (run400), microwaveman (run380), ECOD3 (run390).
- *A rule measured on 3.8/3.9 is a rule about 3.8/3.9 — run the 4.0 Runner.*

### 2026-08-15, fifth batch — the room-name heading is a checkbox

- "ShowRoomDesc swallows the room name" was the Runner preference **Room names
  in descriptions** (off, not persisted; `Sub_20_64`). Not a divergence;
  harness section carries the warning.
- (Room block joined as one string — ported 2026-09-07, see
  WINE-TRANSCRIPTS-TODO.md.)

### 2026-08-15, sixth batch — one game, four Runners: generators as a version bridge

- gen400 / gen390 upconvert 3.80 files; 3.7 files are hand-editable (PRNG XOR).
  Recipe in *Running the Runners*.
- `some` joins the normalized prefix at 3.9 (`>= TAF_VERSION_390` in
  `lib_print_object_np()`); take-from-container normalizes the *taken* object
  only from 4.0.
- Lesson: a self-blessed synthetic golden (`capacity_nest_expected.txt`) is only
  as good as the last Runner run of that probe.

### 2026-08-15, seventh batch — the death sentence has a perspective

- Pre-4.0: `"I'm afraid " & pronoun & " " & copula & " dead!"` (Perspective 0 →
  "I am"); 4.0 a fixed literal. Battle death is a second site. Shared
  `lib_get_death_message()` (sctasks.cpp, scbattle.cpp).
- Pre-3.9 loss message is unreachable (no flag maps to Var1 1).
- *A string census shows presence, not use — go to the P-code.*

### 2026-08-15, eighth batch — the jb2000 trailer: a one-line typo in run380

- run380 only (@0004D001): AdditionalMessage appended only if the turn text
  does **not** already end in two spaces. run370 has no check; 3.9/4.0 fixed it
  in `pspace`. Ported: `task_suppresses_additional_message()` (== 3.80),
  `pf_ends_with_double_space()`.
- Method: four-slot probe (four unrestricted tasks in one game) bisected it in
  five rounds; when every property of the missing thing is ruled out, vary what
  comes before it.

### 2026-08-15, ninth batch — the `g` echo was a checkbox

- The "(hit pinata with umbrella)" echo is **References in brackets**
  (Appearance tab; registry `showbrackets` is ignored on restore). Not a
  divergence.
- Real gap: the Runner's again-words are `!!`, `again`, `last`, `previous`, `!`,
  `g` — `last`/`previous` added in `scrunner.cpp`.
- Footgun: the history scan skips only entries identical to the typed word, so
  typing different synonyms back to back repeats the synonym — interleave real
  commands.
- *An author transcript records the author's settings; reproduce with defaults
  before believing it.*

### 2026-08-17 — SYNONYM runs before task matching in run390 too

- `make_39_synprobe.py` (gitignored, local only): synonym rewrite precedes
  matching; a lower-indexed task ending in `*` steals the line; a medial `*`
  matches zero words. croft tops out at 147 in its own Runner.
- Harness: first scripted command after launch is often lost — pad with `look`s.

### 2026-08-17 — `getdynfromroom` is not a matcher

- Evaluated only by the by-index task runner `Sub_20_22` @0005F750 (battle, walk,
  event TaskAffected, execute-task), over the task's **ALTCMD** entries only.
  Criteria: squeezed, opens `#%object%=getdynfromroom(`, raw ends `)`,
  case-insensitive room match, first non-static object directly in the room.
- Not ported (Runner fenceposts): last room never matches; rooms with spaces
  never match. Scarier scans all rooms, squeezes both sides.
- Ported as `run_task_run_by_index()`; the spontaneous pass is gone. Humbug's
  EVENT 45 is the one corpus user. Probe `GD` (`%theobject%` distinguishes set /
  unset / never evaluated).

### 2026-08-17, second batch — a static in your hands

- A task action never moves a static (run400 @0008C360); ported as an early
  return in `task_move_object()`. `evt_move_object()` is the only static mover.
- **Deliberate:** run400's event mover lists an object as held without updating
  held-state (count/drop disagree); Scarier recomputes consistently. Probe `SM`.

### 2026-08-17, third batch — the `#` is gone before the lister runs

- Loader replaces `#` NPC text with `<name> is here.`; lister folds any text
  ending exactly (case-sensitive) in `" is here."` into one joined sentence,
  printed **first**; empty text drops the NPC. Probe `NH`. Ported in
  `lib_print_room_contents()`; raw-text tail test since 2026-09-07.

### 2026-08-17, fourth batch — a stopped walk is rewound

- While a StoppingTask is complete the walk is held at the top of its cycle; on
  un-completion it restarts and arrives at stop 0 that turn. Probe `S`
  (`make_400_walkprobe.py`). Ported: `npc_tick_npc()` calls
  `npc_start_npc_walk()`. No 3.9 half (V390 has no unset-task action).

### 2026-08-17, fifth batch — the negative resource length is a back-reference

- `-N` = entry N (1-based) of the resource-name table in parse order; `##` loop
  flag stripped; zero-length named resources still take an entry. 535/535
  corpus matches.
- Real bug fixed: a back-reference to a name only ever seen at length 0 used to
  add a negative-length entry and shift every later offset (MikeDesert endings).
  Now reported as no data. Check: 472/472 embedded resources land on their magic
  bytes. `SCR_TRACE_PARSE` shows both trace lines. **§9 closed.**

### 2026-08-17 — §10: the event-length roll is exclusive of `Time2`

- Both Runners, all three families (length, StarterType=2 delay, restart
  re-roll): `lo + Int(Rnd * (hi - lo))`. Probes `EL`, `make_39_evlenprobe.py`.
- Ported: `scr_randomint_exclusive()` (draws even when `hi <= lo`, keeping the
  stream) at the three event-timing sites only.
- Route recipes that held up: seed sweeps; Shadowpeak via
  `Shadowpeak_walkthrough.md` + `shadowpeak_chase.py`; Vampire and iqsfot
  re-derived by hand. **§10 closed.**

### 2026-08-23 — the pooled burden does not drift (3.7/3.8)

- run370/run380 recompute the pooled burden (`make37leakprobe.py`,
  `make38leakprobe.py`); `glk capacity` no-op for pre-3.9 is correct.
- Ported: worn objects are free of `MaxCarried`; 3.8 `count` wording; pre-3.9
  wear message uses the indefinite printer.
- Harness footguns: run370's window isn't maximised — `CLICK_X`/`CLICK_Y` per
  exe in `drive_ckpt.sh`; author passwords block the generators (write
  `    Wild    `); name probe objects something no game uses.

### 2026-08-23, second batch — 3.9 does not leak; capacity refusals are 4.0 rewordings

- run390 recomputes (size = direct held/worn only; weight recurses). Running
  totals are 4.0 alone: `obj_uses_running_load()` (scobjcts.cpp). Probes
  `make_39_leakprobe.py` (`p39leak`, `p39lim`, `p39wt`) + gen400 twins.
- "hands are full." is never qualified (all four binaries). Weight refusal:
  run390 "That is too heavy for you to carry."; run400 "The X is too heavy for
  Player to carry at the moment." `lib_print_too_heavy()`.
- **OPEN (cosmetic, no corpus exposure):** third-person `count` in run390/run400
  is "Player have 0.  The most he can hold is 90." vs Scarier's "Player has 0.
  The most Player can hold is 90."

### 2026-08-23, third batch — `relojero`: the ending is not output yet

- Task 5 (`arreglar *fenix`, only action End game, no CompleteText): run400
  prints the global "Disculpa pero no te entiendo." before WINTEXT; Scarier
  prints WINTEXT alone. Cause: `task_run_end_game_action()` returns
  `var1 != 3`, claiming output. Returning FALSE is not enough — the library
  `* %object% *` catch-all (`lib_cmd_verb_object`) then claims silently, and
  would print the verb-object refusal, not the global one. Reverted; §4 row.
- **OPEN — untaken probe:** an unrecognised verb with an unambiguous object *in
  hand* in run400 — verb-object refusal or global "don't understand"? One arena
  task, one command. Governs a whole command class; needed before porting.

### 2026-09-05 — the 3.9 bracket echo, and `x me` gets its full stop

- run390 does echo `(a X)` for `it`/`them`/`one` (`Sub its()` @43D968); gate
  removed in `uip_assign_pronouns()`. 3.9 keeps the authored article after a
  take (mode 1), so `uip_definite_form()` stays 4.0-only (`veteran.taf`).
- Examine-self appends "." if missing: 3.9 (`examines()` @44C488) and 4.0 (also
  exempting `! ) % ?`). Ported in `lib_cmd_examine_self()` for 3.9+.
- **OPEN:** 3.9 mode-0/raw antecedent sites `co()` @43B69E and @43B610/@46031E
  unmeasured.
- **OPEN:** 3.7/3.8 `x me` reads from P-code as a lone `.` — needs a run380 probe
  (`x me` on any 3.80 game, plus a `look`). No corpus exposure.
