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
      works) — initially pinned the old mapping via `SCR_LEGACY_RANDMAP=1`.
      ~~a documented harness-only compatibility hook~~ **Hook RETIRED
      2026-08-02**: the routes were re-derived under the fixed mapping
      (seeds 13/87/657 — clean-upstream sweep + `shadowpeak_chase.py`, same
      scores 700/715/735; see Shadowpeak_walkthrough.md session 25) and the
      hook deleted from `scutils.cpp`/`seed.cpp`.
- [x] **Death path (no KilledTask).** *(2026-08-01, live, via `pM3`.)*
      "Robot falls down, dead." (byte-same in SCARE), corpse leaves scope:
      run400 answers "Robot isn't here!" / "Player cannot see Robot from
      here." where SCARE says "I don't understand." / "Player sees no such
      thing." — semantics match (location `0xFB`), wording differs.
- [x] **StaminaTask/KilledTask — settled live 2026-08-01** (probe `pKT` in
      `make_arena_probe.py`, which now authors tasks; 3.9 half via
      `make_39_ktprobe.py` in run390).  run400: a set KilledTask **replaces**
      the "falls down, dead." line ("Player hit Robot.  KILLEDTASK FIRED.");
      StaminaTask fires on **every** hit that leaves `0 < stamina <
      max/10` — twice in the probe window (hits leaving 8 and 4 of max
      100) — and does NOT fire on the killing blow.  run390 dispatches
      KilledTask identically ("You shoot Robot with the blaster.
      KILLEDTASK FIRED.").  Decompile (Battles.bas Proc_11_0/Proc_11_3)
      pins two boundary details the probes can't: the threshold divide is
      **floating point** (`CDbl(max)/10`), and the corpse's held/worn
      objects are re-homed to the death room *before* the KilledTask runs.
      Three faithfulness fixes ported (scbattle.cpp): re-home before task,
      `stamina * 10 < maximum` (integer-exact float form), and the default
      corpse line version-gated on `!battle_legacy` — **run390 prints
      NOTHING when a task-less NPC dies** (probed live; the string
      " falls down, dead." does not exist in its binary).
      Corpus: secret_of_lost_world (3.9, Ghost death) re-blessed; all
      76 rows PASS.
- [x] **Player-facing surface — settled live 2026-08-01** (probe `pWS` in
      `make_arena_probe.py`: sword Method 1 / HitValue 10 / Acc 15 + axe
      Method 0 / HitValue 20 / Acc 5, both held; unseen Ghost NPC in a second
      room).  run400's model is a persistent wield ref, NOT a per-attack
      default:
      * Start: "Player is wielding nothing" and the status Current values are
        BARE — no would-be weapon folded (Str 10, Acc 20 with both weapons
        held).  Once something is wielded, status folds ONLY that weapon,
        with the bonus in parentheses ("30   (20)").
      * Bare `attack X`: uses the wielded weapon if set; else with exactly
        ONE held weapon it auto-selects it AND SETS the wield (status shows
        it afterwards); else with 2+ held weapons it asks "What do you want
        to attack Robot with?" — rhetorical (a bare noun reply is a parse
        error; no combat tick), you must retype `attack X with Y`.  So the
        player-side "best weapon = highest HitValue" silent pick never
        happens in run400 — Proc_11_12's best-by-HitValue is the NPC picker
        (and the single-held trivial case).
      * `attack X with Y` and `wield Y` ("Player wield the sword.") both set
        the wield.  **There is NO `unwield` verb** ("I don't understand." —
        it is a SCARE invention).  `drop` of the wielded weapon clears the
        wield to NOTHING — no fallback to another held weapon (status back
        to bare values).
      * Method verbs: wrong-method wielded → "Player can't cut with the
        axe!" (and combat DOES tick); matching → normal attack; nothing
        wielded/held → a plain bare blow ("Player hit Robot.") — Scarier's
        unarmed-verb interpretation confirmed.
      * `status <unseen npc>` does NOT print the "can't get status of a
        character you've not seen yet!" string — the %character% simply
        fails to match and it falls back to the plain player `status`.
      * Bonus: `attack <typo>` → "Who do you want to attack?" DOES tick
        combat in run400; "I don't understand." parse errors don't.
      **PORTED 2026-08-01** (scbattle.cpp/sclibrar.cpp/scgamest.cpp): the
      wield is now a persistent ref -- every armed player blow persists it
      (as Proc_11_1 does, before the hit test, so a miss persists too); bare
      `attack` auto-selects a solitary carried weapon, fights bare-handed
      with none, and with 2+ asks the rhetorical "What do you want to attack
      X with?" (is_admin, so no combat turn passes and the reply is not read
      as an answer); the wield clears when the weapon leaves the player's
      hands (gs_carried_track chokepoint -- drop/throw/give/put/wear; and
      re-taking does NOT re-wield) with no best-carried fallback; `unwield`
      removed; `wield` says "You wield the sword." / "You are already
      wielding the sword."; status always prints the wielding line
      ("nothing" when unarmed) and folds only the actual wield.
      **Erratum:** the port's original "all 77 goldens passed unchanged"
      validation ran against a STALE harness binary (run_v4_walkthroughs.sh
      only rebuilt `scare` when missing -- now fixed to rebuild on newer
      sources).  Real fallout, found and repaired later the same day: three
      goldens had wield-wording lines, and the Shadowpeak routes broke
      exactly as this row predicted -- they drop the sword in the chapel
      (clearing the wield), re-take it (no re-wield), and their next bare
      `attack` with two carried weapons ASKED instead of silently picking.
      Repaired with a zero-turn-cost edit: the first post-retake attack is
      now explicit (`attack cat with sword`), which persists the wield for
      every later bare attack.  ~~Still diverging (cosmetic): status layout;
      "not holding" wording.~~  **Cosmetics PORTED 2026-08-01** after a
      second probe (`pWS2` in `make_arena_probe.py`: the Robot always hits
      for exactly 5, so live stamina drops below max and the status table's
      Stamina cells become distinguishable):
      * Status is a four-column table — header `Range / Max / Current value
        (inc weapons/armour)` (indented past the label column; run400 pads
        it with an *invisible* `<0>`-colored "Stamina:" chunk and vbTabs),
        labels `Stamina: / Hit strength: / Accuracy: / Defense value: /
        Agility:`, and NO "You have:" lead-in for player or NPC.  The
        Stamina row is **live / max / live** (no lo-hi, no parens — pinned
        by pWS2's damaged 195/200/195 and the Robot's 10/30/10); the three
        equipment rows are `lo-hi / max / current / (equipment share)`;
        Agility has no paren.  The trailing line is indented to the first
        column and names the weapon with its *article prefix* ("Player is
        wielding a sword."), "nothing" when unarmed; NPC status is the same
        table ending "Robot is wielding nothing."
      * Wielding a non-carried object: "Player aren't carrying the rock!"
        [sic] — but `attack X with <non-carried>` says "Player **is not**
        carrying the rock!" (both probed live; the two paths genuinely use
        different verb forms).  Both tick combat.  The non-weapon refusal
        ends "!" (" is not a weapon!").
      Ported in sclibrar.cpp (`lib_print_battle_status`/`_attribute`,
      `lib_cmd_wield`, `lib_battle_attack_with`) + scbattle.cpp
      (`battle_attribute_bonus`).  Zero golden fallout (no corpus
      walkthrough runs `status` or a failing wield; colony's "not holding"
      line is the untouched `wear` path); all 76 rows re-verified PASS.
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
per-row `SCR_SEED` to re-thread; the three Shadowpeak rows initially ran under
`SCR_LEGACY_RANDMAP=1`. ~~Re-deriving Shadowpeak under the fixed mapping is
open follow-up work~~ **DONE 2026-08-02** — only the Damastus chase (and one
Cerberus block) was actually fragile; the upstream through `press stone
button` is seed-robust (~1 in 26 seeds clean). New seeds 13/87/657, same
scores 700/715/735, and the legacy-randmap hook is deleted (see the Target
select item and Shadowpeak_walkthrough.md session 25).

Surface facts learned on the way (all consistent between engines unless noted):

- The Runner does **not** auto-wield at battle start (`status`: "Player is
  wielding nothing"), but a generic `attack X` **auto-selects a held weapon**
  and narrates with the weapon's Method verb ("Player shoot Robot with the
  blaster."). Scarier's mechanics already matched (per-attack default weapon,
  no state change), and **the Method-verb narration is now ported too
  (2026-08-01)**, from the DotFix `Battles.bas` decompile
  (`~/adrift-battle/decompiled/`, Proc_11_1/Proc_11_2): armed hit =
  "You shoot Robot with the blaster." (+"s" for NPC attackers), method 5 =
  "You throw the knife at Robot.", armed player miss = "<npc> manages to
  avoid your attack with <weapon>.", armed NPC miss = "<npc> attacks you
  with <weapon>, but you manage to avoid it.". Bare hands keep the plain
  forms.  **Verified live in run390 too** (p39 probe: "You shoot Robot with
  the blaster." — identical wording, second person), so no version gate.
  Corroboration from the corpus: les_feux's French ALR table was authored
  against these full sentences — the old generic wording only half-matched
  ("Vous frappez  demon."), the ported wording translates cleanly ("Vous
  tranchez un démon avec une épée longue.").  20 goldens re-blessed (battle
  flavour only; all win markers intact).  **Throw (method 5) mechanics,
  settled live AND ported 2026-08-01** (probes `pTD` in run400, `p39td` in
  run390 — both via `make_arena_probe.py`/`make_39_probe.py` variants): a
  landed player throw **moves the weapon to the current room in BOTH
  Runners** ("Player is carrying nothing." / look: "Also here is a spear.";
  `get` → throw again works), and its damage is version-split: **run400
  deals base Strength only — HitValue never contributes** (10 observed, not
  15; Battles.bas clears the wielded ref `global_78 = &HFF` at
  `loc_45E457` *before* the damage roll), while **run390 adds HitValue as
  usual** (one-shots a 35-stamina robot with Str 10 + HitValue 30 — its
  regardless-of-method rule).  No persistent accuracy penalty (status
  Accuracy 60→60 after throwing); a weaponless `attack` does NOT pick a
  floor weapon back up; **a *missed* throw keeps the weapon — probed live
  in run400 2026-08-02** (`make_arena_probe.py` variant TDM: player
  Accuracy 0-0 with a zero-accuracy spear against Agility 5-5 can never
  clear the strict `accuracy > agility` test, and after two missed throws
  the Runner still answers "Player is carrying a spear." with a bare floor),
  and **a throw that lands but does no damage still drops it** (variant TDZ,
  Defence 50 swallows Strength 10: "Player throw the spear at Robot, but it
  doesn't seem to do any damage." then "Player is carrying nothing." and
  "Also here is a spear.") — the drop sits ahead of the damage roll, as the
  decompile has it, and Scarier matches both transcripts.  The 3.9 half of
  the miss question does not exist: the legacy model has no accuracy/agility
  step, so every 3.9 attack connects.  NPC throws neither
  drop nor lose HitValue (Proc_11_2 has no equivalent).  Ported into
  `battle_resolve` (drop un-gated, Str-only gated on `!battle_legacy`);
  `light_up` was the one corpus casualty — its Chip fight and Death
  gauntlet were re-derived (582-command route, still 73 pts + THE END) and
  its golden re-blessed.
  The `status` "wielding" line matches since the wield-model port
  (2026-08-01): "wielding nothing" (and bare stats) until a wield is set —
  see the "Player-facing surface" item above (persistent wield ref;
  auto-select persists; asks on 2+ held weapons; no `unwield`; drop clears
  to nothing — all ported).
- Runner battle messages use the player's *name* where SCARE substitutes
  "you"/"your", with second-person verb agreement kept ("Player manage to
  avoid Robot's attack." — sic). Miss messages otherwise identical.
- `i` (inventory) consumes a battle turn in both engines.
- `save` during an active battle is refused ("That is not an option or
  command." in SCARE); write probe saves in a room *before* the enemy.
- An unrecognised NPC name in `attack <x>` gets "Who do you want to attack?".

## 2. Wildcard / any-turn task turn-ordering divergence — SETTLED 2026-08-01

**Both candidate causes were real, and neither was what the original notes
guessed.** Wildcard tasks are input-matched in both engines — there is no
end-of-turn wildcard pass at all.  The same-turn firing in `thetest` comes
from its always-restarting one-turn *events*, and the sole-response mystery
was the author's own ALRs.  Six authored probes
(`test/make_39_wildprobe.py` and inline variants), a neutralized-ALR rebuild
of thetest, and cross-checks of the same gen400-converted probe in **both**
run390 and run400 settled everything.  Fixed in `scevents.cpp` /
`scrunner.cpp` (`run_event_task`) / `sclibrar.cpp`; goldens re-blessed
(thetest, gateway, inverness + padded route; Shadowpeak byte-identical again
after the version gate).

What was actually established, each verified live:

- [x] **No end-of-turn wildcard pass.** A `*` task gated on "rock held", with
      no events in the game, does NOT fire on the `take rock` turn in run390 —
      it fires on the next command, exactly like Scarier always did.  Both
      positive (v2=1) and negated (v2=7/8) restrictions behave the same way.
- [x] **Events with a TaskAffected are the real mechanism.** thetest runs an
      always-restarting 1-turn event every turn.  In the **3.9 Runner** the
      event's "execute task" is dispatched by command text through the normal
      task matcher: the first task in list order that matches the text (`*`
      matches anything) *and* passes where+restrictions fires — so a runnable
      wildcard earlier in the list **steals the execution outright** (the
      affected task does not run that turn), a restricted match is passed
      over silently (its FailMessage is NOT printed), and restrictions are
      evaluated against post-library state.  Order decides: with the ticker
      task before the wildcard, no steal ever happens.
- [x] **The 4.0 Runner reverted (or never had) all of that.** The *same*
      gen400-converted probe in run400: no interception ever, the affected
      task runs directly, and a failing restriction prints its FailMessage
      loudly every turn.  That is exactly SCARE's original
      `task_can_run_task_directional → task_run_task` code — unsurprising,
      since SCARE was written against the 4.0 Runner.  Scarier now version-
      gates: `< TAF_VERSION_400` dispatches through `run_event_task()`, 4.0
      keeps the direct run.  Shadowpeak's ambient bell/rat lines are 4.0
      FailMessage prints and survive unchanged; gateway's four
      "execution is about to be start." lines were 3.9 FailMessage prints
      and correctly vanish.
- [x] **The thetest ALRs manufacture the "sole response".** The raw run390
      output for `drop clothes` (ALR patterns neutralized in a rebuilt .taf)
      is "You drop your clothes.  Nice try fish face!" — library message plus
      stolen-event task text, one paragraph.  The author's ALR rewrites that
      exact combined string to "Nice try fish face!", and a second ALR turns
      the `remove clothes` variant into "Lunatic, eh?  Yes?  Well tough." —
      the observed stray "!" is the leftover the pattern doesn't consume.
      Scarier emits the two texts as separate lines, so the combined-pattern
      ALR cannot match: **known residual cosmetic divergence** (the Runner
      joins a turn's output with two spaces into one paragraph and ALRs the
      whole; Scarier ALRs per string).
- [x] **Library `drop` of a worn item implicitly removes it** — in BOTH
      Runners ("You drop the cloak.", inventory empty after), while `drop
      all` leaves worn items alone (also verified).  Scarier used to refuse
      ("You are not holding..."); fixed via `lib_drop_named_filter` (named
      drops accept worn, the drop-all universe stays held-only).
- [x] **Scarier deliberately does NOT implement the Runner's completed-`*`
      claiming.** In run390 a completed non-repeatable `*` task answers every
      later command "You have already done that." — which **soft-locks
      inverness in the real 3.9 Runner**: after the dressing-room scene the
      catch event's execution and every movement command are eaten, and the
      game cannot proceed (verified live to the lock).  Scarier skips
      completed tasks in both the input matcher and the event dispatch, so
      inverness stays winnable (route padded with two `z`; the catch fires
      one turn later than run390 would have, because Scarier's event-start
      scan runs before the steal completes the gating task within the same
      event phase).  Same faithful-vs-playable call as Topaz.

Residual small divergences, noted not fixed: run390 appends task text to
`i`/inventory output where Scarier lets the wildcard replace it; the Runner
substitutes the player's name with second-person verb forms ("Player drop the
cloak."); and the ALR-over-joined-paragraph difference above.

- [x] **A multi-turn stop runs its walk's CharTask/ObjectTask on the arrival
      turn only — settled live 2026-08-02** (walk probe variant H in both
      generators: Times = 3 in the player's room, 2 away, no wildcard).  Both
      run400 and run390 fire `CHARTASK FIRED.` on the turn the walk counter
      hits that step's suffix-sum and never again during the stay: the 4.0
      probe fires on turns 1 and 6 of a five-turn cycle, and the 3.9 probe
      shows Bob examinable on turns 2-4, gone on turn 5, back with the task on
      turn 6.  Scarier used to fire on every co-located tick.  Fixed in
      `npc_tick_npc_walk` with an `is_arrival` gate that covers fixed-room
      stops; **follow-player stops joined it 2026-08-02** (next row).  A
      roomgroup stop does not behave this way -- "Ticket to No Where"'s lost
      girl wanders a roomgroup on a single Times=4 stop and live run400 has
      her speak on consecutive turns, i.e. the whole step re-runs every tick
      -- so roomgroup stops keep the every-tick behaviour.  Corpus fallout:
      four rows re-blessed (shadowpeak ×3 lose repeated "Seeker hums!"
      lines, melbourne_beach shifts RNG).
- [x] **Follow-player stops warp on arrival ticks only, and the player
      walking in on a mid-stay walker is a 4.0-only CharTask trigger —
      settled live 2026-08-02** (probe K = follow stop Times 3/2 with the
      rooms joined north/south, L = the fixed-stop twin, M = the ObjectTask
      twin; all in both generators, run in both Runners).  Findings:
      (1) BOTH Runners move a follow-stop walker to the player's room only
      on the walk-counter refresh tick -- on the stay turns Bob stands
      still even when the player walks away, and no catch-up ever comes
      (K turns 7-8).  Classic every-turn trailing is just a Times=1 follow
      stop, where every tick is an arrival tick.  (2) BOTH Runners fire the
      CharTask on an arrival tick even when the walker never moved -- K
      turn 11 prints no enter line but fires the task.  (3) run400 ALSO
      fires the CharTask when the PLAYER moves into the walker's room --
      at any stop, fixed, follow or the away stop, on every re-entry (L
      turns 3/8/10, K session 1 turn 8) -- while run390 prints only "Bob
      is standing here" on the identical moves.  SCARE already had exactly
      this check (the undo-gamestate block in `npc_tick_npcs`), so the fix
      was to version-gate it >= 4.0, not to add it.  (4) The player-side
      re-check is CharTask-only: probe M's rock (walk MeetObject/ObjectTask)
      does not fire when the player walks in on it, carries it in, or drops
      it beside the mid-stay walker -- object meets happen on the walk's
      own arrival ticks alone, which is what Scarier already did.
      `look` never fires anything (K session 2 turn 2).  Corpus fallout:
      six rows re-blessed (funhouse/donuts_intro/xfiles lose every-turn
      chaser trailing, tcom/inverness/melbourne_beach lose 3.9 player-move
      fires), and the three Shadowpeak routes re-threaded per the usual
      recipe (upstream seed sweep + `harness/shadowpeak_chase.py`; new
      seeds 1/20/155, scores unchanged 700/715/735).
- [x] **run390 drops a walker's leave announcement when it cannot name a
      direction — settled live 2026-08-02** (3.9 walk probes H and J).  The
      earlier note "run390 prints no ExitText at all" was wrong: with the two
      probe rooms unconnected run390 prints `Bob BOB ENTERS..` on arrival and
      *nothing* on departure, but with the same walk over rooms joined
      north/south it prints both `BOB ENTERS. from the north.` and
      `BOB LEAVES. to the north.`.  Since a walk's stops are room indexes
      rather than exits, a walker can step between rooms that share no exit,
      and the pre-4.0 Runner suppresses the leave line for exactly that case
      (arrivals still print, directionless).  run400 prints the directionless
      leave line, so this is version-gated in `npc_announce`.  Corpus
      exposure, all re-blessed: "Melbourne Beach" (Judy, twice), "Lair of the
      CyberCow" (Vluurinik) and "thetest" (the Robot Guard, six times).
      Melbourne's enter/exit texts are `%jwalksin%`-style variables resolved
      through ALRs, which is why the live Runner's departure verbs vary.
- [x] **Walk CharTask/ObjectTask dispatch is wildcard-interceptable in the
      3.9 Runner, and a direct run in the 4.0 Runner — settled live
      2026-08-01** (`test/make_39_walkprobe.py` / `test/make_400_walkprobe.py`,
      variants E/F/G: looping two-stop walk, NPC Bob meets the player every
      other turn, CharTask = an un-typeable `#met` task).  run390's arrival
      turns print `WILDCARD FIRED.  Bob BOB ENTERS..  WILDCARD FIRED.` with
      the `*` task listed first (the second is the stolen walk dispatch;
      `CHARTASK FIRED.` never appears), and `... CHARTASK FIRED.` with the
      task order swapped — list order decides, exactly the event dispatch
      semantics, and statically the same P-code (Form1.characters at
      0005AAD5/0005AB88 = Form1.checkevent at 00048D83: copy
      `tasks[n-1].command[0]`, call `Form1.tasks(1)`).  A restricted walk
      task is skipped *silently* in run390 but prints its FailMessage
      (`METFAIL.`) in run400, whose walk handler (Sub_20_2 at
      00068B8E/00068BED) calls the same direct by-index runner (Sub_20_22)
      as its events.  Fixed: `run_npc_walk_task()` now version-gates —
      pre-4.0 shares the event dispatch (`run_task_command_dispatch()`),
      4.0 runs directly via `task_run_task` (loud FailMessage).  Whole
      corpus unchanged (110 PASS) — no corpus game has a stealable walk.
      The two "noted not chased" tails of this item were chased 2026-08-01,
      and each was half wrong:
      * **The 1-stop non-looping walk is a version split, not a shared
        no-op.**  run390 truly never runs it (wkC_390 screenshot: Bob never
        arrives, "You cannot see Bob from here.") -- but run400, probed live
        with a fresh 4.0 variant-C file (wk4C_400), runs it fine: the NPC
        arrives on turn 1, the CharTask fires exactly once, and nothing
        happens on the expiry turn.  The old "either live Runner" claim had
        no run400 evidence behind it.  Mechanics (P-code): the Runner's walk
        handler `Sub_20_2` lives in `Sub_20_62`, which is called ONLY from
        `Form1.evaluate` -- walks never tick at startup; the walk counter is
        seeded ΣTimes+1 (`Sub_20_12`), arrivals fire on exact suffix-sum
        boundary matches, and a non-looping walk's final decrement to 0
        marks it finished (0xFF) with no arrival processing.  **All fixed in
        Scarier 2026-08-01**: the startup `npc_tick_npcs` call is gone
        (Scarier used to move walkers and fire their CharTask before the
        first prompt, then AGAIN on the expiry tick -- a double divergence),
        a non-looping walk now expires silently, and a pre-4.0 one-stop
        non-looping *game-start* walk never starts (narrow gate: only the
        StartTask=0 case was probed live, and "deaths" (3.9) needs its
        task-triggered one-stop walk to keep running -- the demon at the
        end walks in on one).  Corpus fallout: 21 rows re-blessed (NPC
        arrivals shift one turn later), sun_empire's route gained a `z`
        (Jeriah arrives a turn later than its wait loop allowed).
      * **The empty-input `*` claim was simply wrong**: Scarier DOES match
        wildcard tasks against an empty input line, and did when the note
        was written (verified with the same probe at that commit and at
        HEAD).  Both live Runners agree -- in the wkE_390/wk4E_400 sessions
        the settle-Return itself fired the wildcard and ticked a turn.  No
        divergence exists here in any direction.

## 3. 3.9 → 4.0 conversion

Two separate problems that keep getting conflated.

### (a) Scarier's own V390 parse fixups

`sctafpar.cpp` carries a set of 3.9-only schema rewrites, each of which was a
reasoned guess: `V390_TASK_ACTION: Type>4?#Type++`,
`V390_TASK_RESTR: Var1>0?#Var1++`, `V390_OBJECT: _Openable_,Key`,
`V390_TASK: $RestrMask`, `V390_V380_ROOM_EXIT`, and
`V390_V380_ALT_TYPEHIDE_MULT = 10`.

- [x] **Whole-corpus 3.9 differential — DONE 2026-08-01. Two real bugs found and
      fixed; every other fixup is confirmed.** Method: gen400 as a *structural*
      oracle rather than a transcript one (below), plus live run390 probes for
      the two disputes it raised.
- [x] **gen400 as an oracle — DONE, and it works far better than a transcript
      diff.** File → Open a 3.9 `.taf` in gen400, then Save As 4.0; play both
      files under Scarier with `SCR_DUMP_TASKS=1` and diff the structural dumps
      (`scdump.cpp` — the ALT and OPENABLE sections were added for exactly this).
      All 28 3.9 games in `adrift-walkthroughs/games/` were converted. **Caveat
      learned the hard way: gen400 is not ground truth, only a second opinion.**
      Both times the dumps disagreed, run390 sided with Scarier's guards or
      against the Generator's arithmetic — so treat a mismatch as a question,
      then answer it by playing.

Verdicts, fixup by fixup:

- [x] **`V390_OBJECT: _Openable_,Key` (the 5 ↔ 6 swap)** — verified. The new
      `OPENABLE` dump section prints raw Openable/Key for every object (the
      older `LOCKKEY` line only fires for a resolvable key, and pre-4.0 games
      never have one). Zero mismatches corpus-wide.
- [x] **`V390_TASK_RESTR: Var1>0?#Var1++`, `V390_TASK: $RestrMask`,
      `V390_TASK_ACTION: Type>4?#Type++`, `V390_V380_ROOM_EXIT`** — verified.
      Every TASK / RESTR / EXIT / EVENT / WALK line matches the Generator's own
      conversion in all 28 games.
- [x] **Room-alt *ordering* — was wrong, now fixed (run390-verified).**
      `lib_find_starting_alt()` scans the alt array **backwards** for the first
      matching method-0/1 alt, so the least specific alt must be emitted first
      and the most specific last. `parse_fixup_v390_v380_room_alts()` emitted
      them the other way round. Correct order is
      `[LastDesc catch-all (disp 2), Task1 alt, Task2 alt, object alt (disp 0)]`.
      Three authored probes settled it live (`test/make_39_altprobe{,2,3}.py`):
      (1) after the gating task completes the Runner prints the AddDesc
      **instead of** the LastDesc, and with both tasks done Task2 wins;
      (2) an alt whose Task1/Obj is **0** is ignored entirely — Scarier's
      zero-guards are right and gen400 over-converts, which is all that the
      residual `cv13`/`cv20`/`cv23` ALT diffs are; (3) an applicable object alt
      outranks both task alts. Eight goldens re-blessed, corpus all-PASS.
- [x] **"Change battle attribute" attribute index — was wrong, now fixed
      (run390-verified).** A 3.9 character record is only
      `Stamina / Strength / Defence` (plus `Attitude` and `Speed` for NPCs) —
      no Accuracy, no Agility — so the 3.9 dropdown is eight entries and 4.0's
      twelve are that list with the Accuracy and Agility pairs spliced in:
      **0-4 unchanged, 5 → 7 (Defence), 6 → 8 (Max Defence), 7 → 11 (Speed)**.
      Scarier was feeding the raw 3.9 index to the 4.0 table, so a 3.9 game that
      handed the player armour instead made them a better shot. Fixed as
      `|V390_TASK_ACTION:_BattleAttr_|`; affects six corpus games (Matt's House,
      Phoenix_Destiny, SecretOfLostWorld, The_Spirits_Flight, deaths, gateway),
      three goldens re-blessed (all pure combat flavour — the player now
      correctly stops taking damage, and `deaths` even kills the demon).
      Probe: `test/make_39_battleattr_probe.py` — Robot Strength 20 vs player
      Defence 0, `zop` adds 40 to attribute 6, `zap` adds 40 to attribute 5.
      run390's `status` prints exactly three player lines,
      `Stamina / Hit strength / Defense value`, as `current (max)`; after `zop`
      it reads `Defense value: 0 (40)` and hits still take 20, after `zap` it
      reads `40 (40)` and every hit is "doesn't seem to do any damage".
      **gen400 gets this wrong**: it maps 6 → 8 and 7 → 11 correctly but turns
      5 into 11 as well, which is what a cascade of un-chained `If`s does
      (5 → 7, and then that same 7 → 11). Those 17 remaining dump mismatches are
      Generator bugs, not Scarier bugs, and are the *only* thing left in the
      differential.

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
- [x] **`Les Feux de l'enfer` — CLOSED 2026-08-01: it was never a conversion
      at all.** Data proof: of its 131 "change battle attribute" actions, 51
      use attribute indices 5/9/10 (the Max-Accuracy and Agility families),
      which **do not exist in 3.9** — a 3.9→4.0 conversion can only emit
      {0,1,2,3,4,7,8,11}.  The game was authored natively in the 4.0
      Generator (its header byte already said native 4.0;
      `Les_Feux_de_l_enfer_walkthrough.md` and the 2026-06-26/27
      WALKTHROUGH_TODO entries had separately established that its
      unwinnability is BY DESIGN — zero win-type EndGame actions — and that
      the one unreachable +10 is an authored restriction orphan the real
      Runner fails identically).  Nothing here for the engine.
- [x] Established and worth not re-deriving: no 4.0 game has an out-of-range
      task/event reference, so any conversion damage is subtle (off-by-one,
      wrong field meaning), never wholesale.  With Les Feux reclassified as
      native, **no author-side conversion-damage candidate remains — §3(b)
      is CLOSED.**

## 4. Contested semantics — Scarier deliberately differs, or nobody has checked

| Item | Scarier | Runner | Status |
|---|---|---|---|
| Negated `Var2` inside the any/no-object quantifier | negates once around the whole quantifier (the meaningful reading) | its per-object switch handles `Var2` 0–5 only, so "any" always fails and "no" always passes | **Deliberate, confirmed live.** No corpus game authors it. Keep. |
| Dynamic-object index past the end (`Var1 ≥ 3 + ndynamics`) | clamps to the last object | raises "Subscript out of range" and dies | **Deliberate.** Unreachable in any shipped game. Keep. |
| Body-part statics in a `Var1 = 2` restriction | positioned at `OBJ_PART_NPC` | ~~statics have no location field, so they read hidden~~ **theory refuted live** — the Runner answers exactly like Scarier | **Settled 2026-08-01, NO divergence** (probes `pBP`/`pBP2` in `make_arena_probe.py`): with the parent NPC present, is-hidden FAILS, visible-to PASSES, not-hidden PASSES — for an NPC part and a player part alike, byte-identical to Scarier; visible-to tracks the parent NPC's room. With the parent absent, the Runner's `%object%` scope filter refuses the part ("I don't understand.") — that is the separate scope-filter row below, not a body-part issue. |
| Object scope when matching a task command | `uip_match_entity()` has no scope filter at all — matches anything | won't match an object that isn't present ("I don't understand what you mean!") | **Confirmed divergence, unfixed — corpus impact measured 2026-08-01: ZERO.** `SCR_TRACE_SCOPE=1` (scrunner.cpp, needs `SCARIER_DUMP_TOOLS`) logs any golden turn where a matched `%object%` task binds an absent object (SCOPE-MISS = Runner would refuse the command; SCOPE-BIND = Runner would bind a different, present object): all 76 rows replayed clean, zero hits. Static exposure is small too — only 5 games author `%object%` task commands at all (adriftorama 67, goldilocks 20, Screen Savers 19, SRSintro 2, X-Files 2). So the divergence is reachable only by off-route input. A faithful fix means porting `Sub_20_74`'s scope rules (the Runner's command-reference filter — presence for objects, and per the `pWS` probe *seen-ness* for `%character%`), plus a disambiguation rule for present-vs-absent name clashes; not worth it until some game demands it. |
| 3.9 shoot-Method strength | version-gated: 3.9 adds `HitValue` to base Str, 4.0 replaces | both confirmed live (run390 one-shot / run400 two hits) | **Fixed 2026-08-01** (`7a4cb7c2`). |
| Upgraded-3.9 combat | `SCR_ASSUME_COMBAT` opt-in; matches author intent | **stalemates, confirmed live 2026-08-01** (Azra: converted acc/agi all 0-0) | Settled — opt-in stays. |
| Restriction evaluation order | evaluates all, no short-circuit | `Sub_20_65` replaces `#` with T/F in a bool-expr string, so it can't short-circuit either | Believed matched (ADRIFT 4 restrictions cannot have side effects — no restriction type mutates state — so "verify a side effect runs" is unprobeable and moot; the P-code reading stands on its own). |
| Integer division rounding | `Round((a/b) + 0.000001)` — banker's rounding with a +∞-biased epsilon (scexpr.cpp's asymmetric compare) | same | **Confirmed live 2026-08-01** (probes `pDIV`/`pDIV2` in `make_arena_probe.py`, which now authors variables and set-var actions): with *true* negative dividends (via `%v1%/2`), run400 answers −5/2 = −2, −7/2 = −3, −1/2 = 0, and 5/2 = 3, 7/2 = 4, 1/2 = 1, 22/7 = 3 — byte-identical to Scarier. Only 5 corpus games author expressions at all (47 exprs; `circus` has the only divisions, positive). |
| Unary minus in expressions | folded into the literal: `-5/2` = (−5)/2 = **−2** | tokenised as an *operator* that reduces after `/`: `-5/2` = 0−(5/2) = 0−3 = **−3** | **NEW divergence, found 2026-08-01 by `pDIV` (its only diverging cell — the `pDIV2` variable forms all agree, which is what pins the cause to the tokeniser, not the rounding).** Zero corpus exposure: none of the 47 authored expressions uses a unary minus (`SCR_DUMP_TASKS` now prints `expr=[...]` on type-3 ACT lines). Documented, not fixed — reshaping scexpr's parser to give unary minus binary-minus precedence risks more than it buys. ~~Open tangent: ADRIFT 5 shares this token engine, so a5sexpr's literal `-5/2` deserves the same one-probe check.~~ **Probed 2026-08-01: NO divergence on the ADRIFT 5 side.** A 44-row battery through the REAL `clsVariable.SetToExpression` (scratch C# driver against the FrankenDrift.Adrift Release dll — no adventure loaded, bare `clsAdventure` + registered vars) matches a5sexpr row-for-row on both the raw string and the `SafeInt(Val())` readback. clsVariable *does* tokenise leading `-` as an operator (`GetToken` clsVariable.vb:134) and reduces the dangling `op expr` pair on run 2 (clsVariable.vb:959-972), after `/` rounded on run 1 — but v5's `Math.Round(AwayFromZero)` is symmetric (`round(-q) == -round(q)`), so the operator parse and a5sexpr's folded parse coincide everywhere, including the var-token vs textual-substitution split (`%v1%/2` with v1=−5). The v4 divergence exists only because run400's `+0.000001` epsilon rounding is asymmetric. Sole divergent row: `-2^-1` (= −0.5) reads back −1 in FD under an English locale (`SafeInt` = VB `Int()` floor) vs 0 from scarier's strtol — and even the real runner is locale-dependent there (comma-decimal `Val("-0,5")` → 0). Kept as-is: fractional results need `^` with a negative outcome, zero corpus exposure. Battery banked as unary-minus cases in `test/a5sexpr_test.cpp`. |
| Combat RNG | own generator | VB6 `Rnd`, `Randomize Timer` on the load path | Won't-fix confirmed live (§1): per-turn combat differs across identical fresh sessions. |
| Battle messages | second person ("you"/"your") | run400 uses player's name with 2nd-person verb forms ("Player manage to avoid…"); run390 uses second person | Presentational only; "you" kept (matches run390). Method-verb weapon narration **ported 2026-08-01**, verified live in BOTH Runners; noted §1 surface facts. |
| Wield model | ~~per-attack default~~ **PORTED 2026-08-01**: persistent wield ref, matching the Runner | persistent wield ref; single-held auto-select persists; ASKS with 2+ held ("What do you want to attack X with?"); NO `unwield` verb; drop clears to nothing; status folds only the actual wield | **Fully settled live 2026-08-01** (probe `pWS`) and **ported the same day**. Corpus fallout (found late — stale-binary erratum, see §1): 3 wording goldens + the Shadowpeak routes' post-chapel bare attacks, all repaired/re-blessed. ~~Remaining cosmetic gaps: status layout (Max column, "(bonus)" parens), "not holding" wording.~~ **Cosmetics ported 2026-08-01** (probe `pWS2`; see §1): four-column status table (Range/Max/Current + equipment share in parens, Stamina row = live/max/live, no "You have:" header, article-prefix wielding line), wield refusal "aren't carrying …!" vs attack-with "is not carrying …!". |
| Thrown (method 5) weapon | drop + version-split damage ported | moves to the room on a player throw in BOTH Runners; damage = Str-only in run400 (HitValue ignored), Str+HitValue in run390 | **Confirmed live in both Runners and PORTED 2026-08-01** (probes `pTD`/`p39td`; see §1 surface facts). `light_up` route re-derived. |
| Enemy target selection | was pinned to one target per session (LCG low-bit + `% range`) | uniform per-turn pick among ally/player | **Fixed 2026-08-01** (`scr_randomint` multiply-shift); corpus re-blessed. |
| Event TaskAffected execution | version-gated: 3.9 = matcher dispatch (wildcard steal, silent restricted skip), 4.0 = direct run (loud FailMessage) | run390 and run400 genuinely differ — same converted probe, opposite behavior | **Fixed 2026-08-01** (§2); both halves verified live. |
| Named `drop` of a worn item | implicitly removes then drops (named only; `drop all` skips worn) | same, in BOTH Runners | **Fixed 2026-08-01** (`lib_drop_named_filter`). |
| Completed non-repeatable `*` task | skipped by matcher and event dispatch | claims every later command: "You have already done that." — soft-locks inverness for real | **Deliberate divergence.** Do not import; see §2. |
| `drop <thing> in/on <container>` | ~~the priority `drop %text%` pattern swallowed the `in <container>` tail and answered "Drop what?"~~ **FIXED 2026-08-02** | routes it to the put-in / put-on handlers: `drop wallet in bin` → "You put your wallet inside the rubbish bin.", `drop wallet on bin` → the put-on refusal "You can't put anything onto the rubbish bin!" | **Confirmed live against run400 and FIXED 2026-08-02** while deriving `Ticket to No Where`, whose author-route disposes of five bits of litter with `drop <litter> in bin` (2 points each — the difference between 100 and its full 110). Six patterns added to `PRIORITY_COMMANDS[]` (`scrunner.cpp`), covering `drop`/`put down` × `in`/`on` × plain/`all`/`all except`, placed *before* the plain drop patterns so the `%text%` no longer eats the tail. No corpus fallout. |
| What `all` ranges over | ~~everything a named take can reach, including the contents of a carried open container~~ **FIXED 2026-08-02** | leaves alone anything already in the player's possession; a *named* take still reaches into a carried open container | **Confirmed live against run400 and FIXED 2026-08-02.** In `Ticket to No Where`, holding the open bag of shopping and typing `get all` answers "You take the pamphlet." and leaves the tights, pet food, deodorant and gloves in the bag, while `get paper` still lifts the scrap out of the carried wallet ("You take the scrap of paper from your wallet."). `lib_take_all_filter()` (`sclibrar.cpp`) = `lib_take_filter && !obj_indirectly_held_by_player`, used by `lib_cmd_take_all` and by the `take all except` resolver in `lib_take_multiple_common`. The visible symptom was four bogus items of inventory weight later refusing `get banana skin`. Corpus fallout: the two ALEXIS goldens, re-blessed after proving the change correct there too (its leather bag is player-carried, so the Runner would never have emptied it either). |
| "get all" with nothing takeable | "There is nothing to pick up here." | "There is nothing worth taking here." | **Cosmetic divergence, observed live 2026-08-02, deliberately unfixed.** Wording only; changing it would churn goldens across the corpus for no behavioural gain. |
| `%character%` / `%object%` as the **last element inside a `[...]` or `{...}` group** | ~~can never match: `uip_match_remainder()` built an empty remainder list and `uip_match_list()` fails empty lists by design, so every candidate was rejected~~ **FIXED 2026-08-02** | matches | **Confirmed live against run400 and FIXED 2026-08-02** while deriving `ADRIFTMAS Party`. In the real Runner `kiss mystery` on the Front Steps answers "You lay a Happy Holidays kiss on Mystery.  Turns out that Mystery didn't appreciate it much and belts you right in the kisser." — i.e. it matches TASK 20 `[kiss {the} %character%]`; Scarier fell through to the library's `lib_cmd_kiss_npc`. Root cause in `scparser.cpp`: `uip_parse_list()` appends a `NODE_EOS` **only** on `TOK_EOS`, so a reference that ends a group has `right_sibling == NULL`, and the temporary remainder list `uip_match_remainder()` builds is empty. Fix: when `node->right_sibling` is NULL the remainder is vacuously satisfied — return TRUE. Top level is unaffected (the EOS sibling still enforces end-of-string after the group), and with an empty remainder the existing `max_extent` logic already picks the longest candidate. Two very common idioms were dead: `[kiss {the} %character%]` and `[smack/hit/punch/kick]{the}[%character%]`. Corpus fallout: one row, `ticket_solution.txt`, where TASK 405 `[say][hello][to]{the}[%character%]` now fires on `say hello to john tailer` instead of falling through to the game's default response — re-blessed; the only other transcript change there is RNG drift on a random Trainspotter utterance (it also comes and goes across seeds in the *old* binary, so it is stream shift, not semantics). `SCR_TRACE_MATCH=1` over all 320 commands of that route shows exactly one added MATCH line and no removals. ~~Open tangent: `uip_match_text()` has the same shape and so a trailing `%text%` inside a group is presumably equally dead — unprobed, no corpus exposure found.~~ **Probed 2026-08-02 — half right, and NOT a fix candidate: see the next row.** A trailing `%text%` inside a group really is dead in Scarier, but it is dead in run400 too, so that half is agreement, not divergence. |
| `*` or `%text%` **inside** a `[...]` / `{...}` group | two cells match that the Runner refuses: a group-trailing `*` consuming **zero** words (`[eat *]` fires on bare `eat`), and a **mid-group** `%text%` (`[quip %text% hard]` fires on `quip hi hard`, capturing "hi"). Everything else agrees. | **run400 does not support either token inside a group at all** — a pattern containing one is dead for every input: `[echo %text%]`, `[jot %text%]`, `[mark %text%] now`, `[quip %text% hard]`, `[eat *]`, `[nib/nab *]`, `[snag *]` all answer "I don't understand.", bare or with a tail. `%character%` in a group is fine (`[poke {the} %character%]`, `[prod %character% hard]` both fire), as are literal groups (`[wham]`, `[zap/zop] thing`) and a top-level `mimic %text%`. | **Probed live 2026-08-02** (probes `TX`/`TX2` in `make_arena_probe.py`; note a probe's CompleteText must echo a capture as `[%text%]` — `<%text%>` is eaten as markup and prints empty). This replaces the `uip_match_text()` tangent on the row above: the presumed "dead in Scarier" cells are *shared* dead behavior (`uip_match_text()` builds the same empty remainder list as the old `uip_match_entity()` bug, but returns FALSE, which is what run400 does anyway), and the only real divergences run the other way — Scarier matching where run400 refuses. **Zero corpus exposure, measured**: unpacking all 99 v4-era corpus `.taf` (67 zlib v4.0 + 32 PRNG-XOR v3.9/3.8; `BeThere.taf` is an ADRIFT 5 file and belongs to the a5 corpus) and scanning every bracket group turns up exactly ONE group-embedded token anywhere — `SRSintro`'s `[ask] {the} [woman/trader] [about] [%text%]`, which is the *shared-dead* shape: Scarier answers the game's own "Use the format …" fallback there, exactly as the Runner would. No corpus game puts a `*` inside a group at all. **Documented, not fixed** — suppressing the two cells means teaching `uip_match_wildcard()`/`uip_match_text()` that they sit inside a group, and the zero-word `*` rule they'd have to special-case is *correct* at top level (row below); nothing in the corpus, and no known game, pays for the risk. (Scan script committed as `test/taf_pattern_scan.py` — it de-obfuscates both TAF generations and greps task-command lines. It reads the raw file rather than going through `SCR_DUMP_TASKS` because it needs the *pattern text* of every task in 99 games at once, which no dump prints; `harness/build.sh` does define `-DSCARIER_DUMP_TOOLS`, so `SCR_DUMP_TASKS` itself works fine — an earlier note here claiming otherwise was reading a stale binary. Re-run the scan before deciding any future parser question by exposure.) |
| `*` matching **zero** words in a task command | matches | **matches too — NO divergence** | **Settled live 2026-08-02** (probe `ST` in `make_arena_probe.py`): run400 fires `foo * bar` on `foo bar`, `qux *` on `qux`, `* yop` on `yop`, and a zero-word match with a failing restriction still prints the FailMessage. Both engines identical.  The `TheADRIFTProject` mystery that motivated this row had a different cause entirely — see the put-family precedence row below. |
| Which failing restriction's FailMessage prints | lowest-indexed failing restriction; empty message → fall through to the library | same — lowest-indexed, incl. under a mixed `#A#A#O#` mask | **Settled live 2026-08-02** (probes `FM`/`FM2`): run400 answers AFIRST / fallthrough / CFIRST / DFIRST and ETWO / ZTWO / HTHREE — byte-identical to Scarier on every cell.  No divergence. |
| Put-family precedence over a matched-but-failing task | ~~loud-fail phase ran before the standard put-in/put-on handlers, so the task's fail message always claimed the input~~ **PORTED 2026-08-02** | a `put`/`drop X in/on Y` that the library can resolve **and complete** runs instead of the fail message ("put pill in cup" answers "You put the small pill inside the coffee cup." with a failing `put * pill in cup` task matched); when the library would **refuse** (target not a container/surface) the fail message wins ("drop pill in slime" prints the task message, not the refusal); unresolvable nouns also leave the message ("put pill in goo") | **THE ACTUAL `TheADRIFTProject` divergence — settled live 2026-08-02** (a `.tas` transplant reproduced the author's 2004 comp transcript on our run400, then probes `FM4`–`FM7` isolated the rule; `FM5` vs `FM7` is the minimal pair — same task, the nouns' existence flips the outcome).  Ported: the six `put …` patterns joined their `drop` twins in `PRIORITY_COMMANDS` (scrunner.cpp), and `lib_put_in_is_valid`/`lib_put_on_is_valid` **defer** instead of refusing during that pass (`run_priority_defer()`; the scan stops so the plain-drop `%text%` rows can't swallow the input, and STANDARD_COMMANDS keeps duplicates of all twelve rows to print the refusal when no task claims it).  Also: the two-object canonical retry (`lib_try_game_command_common`) now tries only the fully-prefixed form — the prefix-less "put pill in cup" retry was exactly the string the wildcarded task steals, while Wax Worx's `get * head` still claims "get marie" via the prefixed "get Marie Antoinette's head" (that row regressed under a broader any-wildcard exclusion and pinned the rule).  Corpus: 102/102 PASS; `TheADRIFTProject` reverted to the author's route (former Repair 1 dropped) and re-blessed. |
| Single-object library success vs failing explicit-verb task | library acts (e.g. `take rock` takes the rock when no task passes) — **which is run390's behavior** | **version split, both probed live 2026-08-02**: run400 prints the task's fail message (`take rock` with a failing `take * rock` task answers TFAIL, rock stays put); run390 runs the library take ("You pick up the rock.") — Scarier's exact behavior.  `wear rock` prints WFAIL in BOTH Runners (and in Scarier). | **Documented, not fixed** (probe `FM4` in run400; `make_39_fwprobe.py` in run390).  Scarier sides with run390 on take and with both Runners on wear; only the run400 take half diverges.  run400 also does NOT rewrite `take` to `get` (a passing `get * rock` task did not fire on `take rock` there, where Scarier's canonical-verb retry fires it).  Zero corpus impact; reconciling the run400 half would mean a version-gated rework of the take retry model, deferred until a game demands it.  **Passing-task complement probed in run390 2026-08-02** (`make_39_fwprobe.py` variant `p` — same four tasks, restrictions dropped): `take rock` → TAKEPASS., `wear rock` → WEARPASS., `put pill in cup` → PUTPASS., and the library never runs alongside (rock stays on the floor, `examine cup` shows no pill).  So run390's library-take-on-failing-task is a *restriction fallback*, not a take-family task bypass — a passing task claims all three verbs outright, and Scarier byte-matches the whole session on the same `.taf`.  The passing put cell also completes the put-family precedence picture on the 3.9 side: only a matched-but-**failing** task is demoted below the library put. |
| Zero-length always-restarting event | ~~re-armed each finish, so the checker fired at startup twice and then EVERY turn~~ **FIXED 2026-08-02** | fires its texts/TaskAffected exactly ONCE, at game start, and never restarts — **identical in run390 AND run400** (probed live: `make_39_fwprobe.py` variant `b` / probe `EV` in `make_arena_probe.py`, pill-starts-in-cup so the checker's restriction holds from turn one: one "PILLCHECK FIRED." appended to the opening room description, silence forever after; and IceCream's zero-length "Customer2" event played live in run400 prints its customer paragraph once, not per turn) | **Engine fixed** (`scevents.cpp`: a `Time1=Time2=0` event on the restart-immediately paths goes dormant after its first finish; the unprobed zero-length-with-delayed-restart shapes keep the old re-arm).  Two goldens were carrying the unfaithful per-turn firing and are re-blessed: `icecream` (the per-turn customer nag was Scarier-only) and `TheADRIFTProject` (the radioactive mix now lands on the `take slime` turn via TASK 40's action chain, byte-matching the author's 2004 transcript — the earlier "ticks before the command" reading of that turn was wrong, the event simply never re-fires).  Corpus 104/104 PASS.  **Superseded 2026-08-02 by the row below** — the remaining zero-length shapes were probed, and "goes dormant" turned out to be only one of the Runner's three answers. |
| Zero-length events, the whole shape table | ~~a `Time1=Time2=0` event finishes on the turn it starts, whatever started it, and (before the row above) re-armed for ever~~ **PORTED 2026-08-02** | **what starts the event decides what it does** — run400 has three distinct answers, all probed live: (1) **started at game load** (StarterType=1) → starts and finishes on turn 0, once; (2) **started off a clock** (a StarterType=2 delay, or a restart-after-delay countdown) → prints its StartText and then **parks**: it stays running for the rest of the game, its LookText appears in every later room description, and its FinishText and TaskAffected never run; (3) **started by its starter task** (StarterType=3) → starts *and* finishes on the trigger turn, once.  Restart is a fourth axis: RestartType=1 (immediately) really does start the event again — a second StartText, then it parks per (2) — while RestartType=2 (after delay) on an immediate or task starter goes quiet for good, with no LookText, so it is not sitting in a running state either. | **Probed live 2026-08-02** — `make_arena_probe.py` configs `EV2` (the three starter types, all zero-length, all RestartType=2), `EV3` (the same delayed start with a *non-zero* length, proving the generator's StartTime/EndTime layout and so that `EV2`'s silence is semantics, not a bad file), `EV4` (Del Sol's exact shape with all three texts — this is where the parking showed up: `G1 START.` on the delay turn, `G2 FINISH.` from the length-2 control the turn after, `G1 FINISH.` never, and `G1 LOOK.` still in the room description eight turns later), and `EV5` (all three starters *with* texts and affected tasks, which is what separated "restarts and parks" from "goes dormant": `H1 START. … H1 START.` at turn 0 and `H1 LOOK.` for ever after; `H2`/`H3` silent after their single firing).  **Engine ported** (`scevents.cpp`): `evt_is_zero_length()` names the shape; the ES_WAITING countdown path starts such an event without finishing it; ES_RUNNING leaves a parked event's clock alone instead of decrementing it negative; and the finish-time gate now covers RestartType=2 with starter 1 **or 3** (the starter-task cell used to re-fire the affected task *every turn* after its first trigger) while RestartType=1 is deliberately no longer gated, so it restarts and parks like the Runner.  Corpus: v4 104/104 PASS, a5 unaffected; one golden re-blessed — `TheADRIFTProject`, where the extra length roll on the restart shifts the RNG stream and Joshua's gum tastes of carpet instead of octopus.  Del Sol's EVENT 11 "physics distraction 3" is the corpus's only live instance of shape (2), and it is room-gated: the walkthrough is never in the physics room at turn 25, so no golden moved. |
| When immediate events start relative to the opening room description | ~~they start during the first tick, i.e. **after** the opening description: their StartText prints below it, and their LookText is missing from it~~ **FIXED 2026-08-02** | both Runners start them during load, **before** the description: the opening room text carries their LookText, their StartText is nowhere to be seen (printed into the pre-intro screen and cleared), and only the finish half lands under the description.  Probe `EV5` turn 0, run400: `A bare arena.  H1 LOOK.  H2 LOOK.  H1 FINISH.  H1 TASK.  H1 START.  …` against the old Scarier's `A bare arena.` / `H1 START.` / `H1 FINISH.` / `H1 TASK.` / … | **Probed live in BOTH Runners and FIXED 2026-08-02.**  The probe the old note asked for is `EV6` in `make_arena_probe.py` — a plain **length-3** immediate event carrying all three texts, so the zero-length parking model can't be confused with the start model.  run400 puts `K1 LOOK.` in the opening description, never prints `K1 START.`, and still finishes on the third command turn; `make_39_fwprobe.py` variant `e` gets the same answer from run390, so this is **not** a version split.  Ported in `scevents.cpp` as `evt_start_load_events()` / `evt_finish_load_events()`, called either side of the `DispFirstRoom` block in `scrunner.cpp`: the start half runs before the description (silent — `evt_start_event()` gained a `silent` flag — and +1 on the clock so the startup tick's decrement still lands on the rolled length), the finish half runs just before that tick, because a zero-length immediate event's FinishText/TaskAffected/RestartType=1 restart all land *below* the room text in `EV5`.  **Corpus exposure, measured** (`scdump.cpp`'s EVENT line now ends `texts=SLF`): 590 events in 75 of 121 games, 280 of them immediate-start across 49 games; **7 immediate events carry a LookText** (Shadowpeak, `The Town Of Azra` ×2 file copies, tq3) and **16 carry a StartText** (tq3, Azra ×2, adriftorama, Shadowpeak, Colony, yak_shaving, Main Course, Del Sol); 65 events in 19 games have a LookText at all.  Corpus 128/128 PASS after re-blessing: the direct fallout is Colony/Del Sol/Main Course (turn-0 StartText gone), Azra and villains_and_kings (LookText now inside the opening description), and the rest is RNG drift — rolling immediate-event lengths at load moves them *ahead of* `battle_start()`'s stamina rolls, which churns light_up, circus, melbourne_beach, alexis and Main Course's NPC walks.  That reordering is unverifiable by construction (the Runners seed from `Timer`, so their combat differs run to run — §1), and it is the order the load-start model implies; accepted deliberately. |
| Where an event's LookText sits **inside** the room block | ~~inside the description paragraph, before the object list and the character lines: `A bare arena.  K1 LOOK.` / `Also here is a rock.` / `Robot is here…`~~ **FIXED 2026-08-02** | **dead last, after everything**: `A bare arena.  Also here is a rock.  Robot is here, looking dangerous.  K1 LOOK.` | **Probed live 2026-08-02 (probes `EV7`/`EV8` in run400, run390 agreeing on the 3.9 twin) and FIXED the same day.**  The LookText loop in `lib_print_room_description()` (`sclibrar.cpp`) now runs *after* `lib_print_room_contents()`, joined on with a new `pf_buffer_join()` (`scprintf.cpp`): it removes the single terminating newline our section printers add, then separates with the Runner's two spaces unless the preceding text ends with an author's own break — so `Fetlar the overly fetid is here.  It is raining...` joins on one line, while a `<br>`-led LookText (CyberCow's night-time lines) still starts its own.  A buffer-length guard keeps the join from migrating LookText up onto the room name line when the room has no description or contents.  It also retired a small old wart: a LookText after a `<br>`-terminated description used to print with a stray leading two-space indent (Shadowpeak's `  It is raining...`).  Corpus fallout, all verified to be pure relocation before re-blessing: 14 rows across 10 games (Shadowpeak ×3, CyberCow ×2, villains_and_kings ×2, Azra, orient_express, screen_savers, secret_of_lost_world, ticket, tq3, JGrim); 127/127 PASS, a5 suite untouched, sanitizers clean. |
| Put-family precedence, 3.9 half | same port, ungated | **run390 agrees with run400**: `put pill in cup` with a matched-but-failing task runs the library put ("You put the pill inside the cup.", fail message suppressed) | **Verified live 2026-08-02** (`make_39_fwprobe.py`): the ungated port is faithful on both sides. |

---

## Suggested order

*(2026-08-01: the old item 1 is done — stalemate, hit test, exclusive Hi,
damage floor, worn armour and the RNG question are all settled live; see §1.)*

1. §1 remainder — *(done 2026-08-01, second batch: cadence, recovery, target
   select + the scr_randomint fix, death path, and the shoot rule in BOTH
   Runners.)* The player-facing wield/status surface was settled AND ported
   2026-08-01 (see the divergence table).  *(Third batch, same day:
   StaminaTask/KilledTask settled live in both Runners and ported —
   `make_arena_probe.py` now authors tasks and statics.  §1 is CLOSED.)*
2. §3(a) whole-corpus 3.9 differential — *(done 2026-08-01 via the gen400
   structural oracle plus four run390 probes: room-alt ordering and the battle
   attribute index were both wrong and are now fixed; every other V390 fixup is
   confirmed.)*
3. §2 wildcard ordering — *(done 2026-08-01: no end-of-turn pass exists; the
   mechanism was event task-execution dispatch, version-split between the two
   Runners, plus the worn-drop library rule and thetest's ALRs.  Fixed and
   re-blessed; inverness soft-locks in the real run390 and Scarier
   deliberately doesn't import that.)*
4. §4 body-part statics — *(done 2026-08-01: NO divergence, theory refuted
   live; see the table.)*  Scope filter — *(measured 2026-08-01: zero corpus
   impact, 5 games statically exposed; stays unfixed, see the table.)*
   Division rounding — *(confirmed live 2026-08-01, and the probe surfaced a
   NEW unary-minus tokeniser divergence, zero corpus exposure; see the
   table.)*  §4 is CLOSED except for implementing nothing — every row is now
   settled, measured, or deliberately kept.
5. §3(b) `Les Feux de l'enfer` — *(closed 2026-08-01: its battle-attribute
   actions use 4.0-only attribute indices, so it is native 4.0, not a
   conversion; unwinnability was already established as by-design.  §3(b)
   has no remaining candidates.)*

**2026-08-01: every numbered item in this file is now settled, measured, or
deliberately kept.** What remains open is recorded inline: the scope filter
and the unary-minus tokeniser (both zero-corpus-impact, documented in §4's
table), and ~~re-deriving Shadowpeak under the fixed RNG mapping (§1 corpus
note)~~ *(done 2026-08-02 — seeds 13/87/657, legacy hook retired)*.  The
a5sexpr `-5/2` tangent was probed the same day: NO divergence on
the ADRIFT 5 side (§4 table row) — away-from-zero rounding is symmetric, so
clsVariable's operator-tokenised unary minus and a5sexpr's folded one agree.

**2026-08-02 addendum — the `TheADRIFTProject` zero-word-`*` row is closed,
and it wasn't the wildcard.** Five probe rounds (`ST`, `FM`–`FM7` in
`make_arena_probe.py`) plus a `.tas` transplant of the game itself into
run400 settled it: zero-word `*` and fail-message selection are identical in
both engines; the real rule is that run400's put-in/put-on family runs ahead
of a matched-but-failing task **when the library action can complete**, and
defers to the task's fail message when it would refuse.  Ported (put rows into
`PRIORITY_COMMANDS` with deferred refusals; two-object canonical retries now
prefixed-form-only — Wax Worx pinned the retry rule), corpus 102/102, the
game's route reverted to the author's order.  Two new documented-not-fixed
rows came out of the same probes: run400 lets a failing explicit-verb task
beat single-object take/wear (and does no take→get rewrite), and its
zero-length checker events tick before the command rather than after.

**Same-day follow-up — the 3.9 halves, and the event row was wrong.**
`make_39_fwprobe.py` ran the same cells in run390: `wear` fail-message and
the put-family precedence agree with run400 (the ungated port is right), but
the take half is a version split — run390 runs the library take, exactly as
Scarier does, so only run400's TFAIL diverges.  And the "ticks before the
command" event theory died on a cleaner probe (pill starts in the cup): a
zero-length always-restarting event fires ONCE at game start and never
again, in BOTH Runners.  That one is now FIXED in `scevents.cpp`, with
`icecream` (per-turn customer nag was Scarier-only, confirmed live) and
`TheADRIFTProject` (mix now on the `take slime` turn, byte-matching the
author's transcript) re-blessed; corpus 104/104.

**Same day again — zero-length events have three behaviours, not one.**
Probes `EV2`–`EV5` finished the shape off.  "Fires once and goes dormant" is
what a zero-length event does when the *game start* or its *starter task*
starts it; when a **clock** starts it — a StarterType=2 delay, or a
restart-after-delay countdown — run400 prints the StartText and then leaves
the event running for good: LookText in every later room description,
FinishText and TaskAffected never.  And RestartType=1 genuinely restarts,
printing a second StartText before parking, which is why that path is no
longer gated.  Ported in `scevents.cpp` (`evt_is_zero_length()`, a
non-finishing countdown start, a parked ES_RUNNING clock, and the dormancy
gate widened to the starter-task cell that used to re-fire every turn);
corpus 104/104 with only `TheADRIFTProject` re-blessed for an RNG shift.
One new open row fell out of it: run400 starts immediate events *before* the
opening room description, so their LookText belongs in that description and
their StartText is never seen — a tick-order question with a corpus-wide
blast radius, deliberately left unprobed here.

**Same day, last open row — immediate events really do start at load, in
BOTH Runners.**  `EV6` (a length-3 immediate event with all three texts) took
the zero-length parking model out of the reading, and `make_39_fwprobe.py`
variant `e` got the identical answer from run390: LookText in the opening
description, StartText never seen, length and finish turn unchanged.  Ported
as the two `evt_*_load_events()` halves either side of `DispFirstRoom`
(`scevents.cpp` / `scrunner.cpp`); exposure counted with `scdump.cpp`'s new
`texts=SLF` field (7 immediate events with a LookText, 16 with a StartText,
across 121 corpus games); corpus 128/128 after re-blessing the turn-0 rows
and the games whose battle RNG drifted behind the earlier length rolls.  The
probes that proved it also turned up a *second*, smaller divergence: both
Runners print event LookText **last** in the room block, after the object
list and the character lines, where Scarier printed it inside the description
paragraph.  **That one is now FIXED too (2026-08-02, its own pass)** — the
loop moved past `lib_print_room_contents()` with a two-space
`pf_buffer_join()`, 14 goldens re-blessed as pure relocation, 127/127 PASS
(see the §4 table row).
