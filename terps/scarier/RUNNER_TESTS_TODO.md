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
      every later bare attack.  Still diverging (cosmetic): status layout
      (no Max column, no "(bonus)" parens); "You are not holding X." vs the
      Runner's "Player aren't carrying the sword!" [sic] on wielding a
      non-held object.
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
  floor weapon back up; a *missed* throw keeps the weapon (decompile: the
  move is inside the hit branch only, unprobed live); NPC throws neither
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
cloak."); and the ALR-over-joined-paragraph difference above.  From the walk
probes (2026-08-01, unprobed further): run390 prints no ExitText on a walker's
leave turns (wkE_390 shows only the ENTER lines; run400 prints both, as
Scarier does); and Scarier re-fires a walk's CharTask on every co-located tick
of a multi-turn stay (Times > 1, or a follow-player walk) where run400 fires
only on the exact boundary turn -- invisible with the corpus's Times=1 walks.

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
| Unary minus in expressions | folded into the literal: `-5/2` = (−5)/2 = **−2** | tokenised as an *operator* that reduces after `/`: `-5/2` = 0−(5/2) = 0−3 = **−3** | **NEW divergence, found 2026-08-01 by `pDIV` (its only diverging cell — the `pDIV2` variable forms all agree, which is what pins the cause to the tokeniser, not the rounding).** Zero corpus exposure: none of the 47 authored expressions uses a unary minus (`SCR_DUMP_TASKS` now prints `expr=[...]` on type-3 ACT lines). Documented, not fixed — reshaping scexpr's parser to give unary minus binary-minus precedence risks more than it buys. Open tangent: ADRIFT 5 shares this token engine, so a5sexpr's literal `-5/2` deserves the same one-probe check. |
| Combat RNG | own generator | VB6 `Rnd`, `Randomize Timer` on the load path | Won't-fix confirmed live (§1): per-turn combat differs across identical fresh sessions. |
| Battle messages | second person ("you"/"your") | run400 uses player's name with 2nd-person verb forms ("Player manage to avoid…"); run390 uses second person | Presentational only; "you" kept (matches run390). Method-verb weapon narration **ported 2026-08-01**, verified live in BOTH Runners; noted §1 surface facts. |
| Wield model | ~~per-attack default~~ **PORTED 2026-08-01**: persistent wield ref, matching the Runner | persistent wield ref; single-held auto-select persists; ASKS with 2+ held ("What do you want to attack X with?"); NO `unwield` verb; drop clears to nothing; status folds only the actual wield | **Fully settled live 2026-08-01** (probe `pWS`) and **ported the same day**. Corpus fallout (found late — stale-binary erratum, see §1): 3 wording goldens + the Shadowpeak routes' post-chapel bare attacks, all repaired/re-blessed. Remaining cosmetic gaps: status layout (Max column, "(bonus)" parens), "not holding" wording. |
| Thrown (method 5) weapon | drop + version-split damage ported | moves to the room on a player throw in BOTH Runners; damage = Str-only in run400 (HitValue ignored), Str+HitValue in run390 | **Confirmed live in both Runners and PORTED 2026-08-01** (probes `pTD`/`p39td`; see §1 surface facts). `light_up` route re-derived. |
| Enemy target selection | was pinned to one target per session (LCG low-bit + `% range`) | uniform per-turn pick among ally/player | **Fixed 2026-08-01** (`scr_randomint` multiply-shift); corpus re-blessed. |
| Event TaskAffected execution | version-gated: 3.9 = matcher dispatch (wildcard steal, silent restricted skip), 4.0 = direct run (loud FailMessage) | run390 and run400 genuinely differ — same converted probe, opposite behavior | **Fixed 2026-08-01** (§2); both halves verified live. |
| Named `drop` of a worn item | implicitly removes then drops (named only; `drop all` skips worn) | same, in BOTH Runners | **Fixed 2026-08-01** (`lib_drop_named_filter`). |
| Completed non-repeatable `*` task | skipped by matcher and event dispatch | claims every later command: "You have already done that." — soft-locks inverness for real | **Deliberate divergence.** Do not import; see §2. |

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
table), re-deriving Shadowpeak under the fixed RNG mapping (§1 corpus note),
and the a5sexpr `-5/2` tangent (§4 table).
