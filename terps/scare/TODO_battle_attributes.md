# DONE: mutable battle attributes ("Change <attribute>" task action)

> **Status: implemented and validated.**  Type-7 task actions are now parsed,
> applied to mutable per-character battle state, and serialised.  See
> "Reverse-engineering results" and "Validation" below.  The original plan
> follows for reference.

## Reverse-engineering results (Step 1, resolved)

Decoded authoritatively from the **editor** (gen400, native-compiled, decompiles
cleanly) at `Modules.bas` ~1290-1454 and the action-editor combo box
`Form28.frm` ~875.  The runner (run400) is VB **P-code** and its task-action
executor does not decompile, so the editor is the source of truth; the apply
rules below are confirmed behaviourally (see Validation).

- **Parameter packing.** `Var1` = attribute index, `Var2` = target character,
  `Var3` = value.
- **Attribute index (`Var1`).** `0` Attitude, `1` Stamina, `2` Max Stamina,
  `3` Strength, `4` Max Strength, `5` Accuracy, `6` Max Accuracy, `7` Defence,
  `8` Max Defence, `9` Agility, `0xA` Max Agility, `0xB` Speed.
- **by vs to.** Determined by the attribute, not a flag: Attitude (0) and Speed
  (0xB) are **set to** an enum; every other attribute **changes by** the signed
  delta in `Var3`.  `Var3` is always a literal integer (no expression/variable).
- **Target (`Var2`).** Attitude and Speed are NPC-only: `Var2==0` = referenced
  character, `Var2>=1` = NPC `Var2-1`.  All other attributes may also target the
  player: `Var2==0` = player, `Var2==1` = referenced character, `Var2>=2` =
  NPC `Var2-2`.
- **Attitude enum remap (the subtle bit).** The "Change Attitude" combo is
  ordered **0=Ally, 1=Neutral, 2=Enemy** (`Form28`), but the NPC editor's
  initial-attitude radios and the combat code use the *internal* encoding
  **0=Neutral, 1=Ally, 2=Enemy** (editor `Form1.frm` ~726-752; matches the
  `3 - attitude` ally/enemy opposition in combat).  So the runner must remap
  `Var3`: `0->1, 1->0, 2->2` (see `battle_attitude_from_ui`).  Speed's enum
  (`0..4`) already matches the internal cadence, no remap.
- **Ranged attributes.** Each of Strength/Accuracy/Defence/Agility carries a
  live `Lo`/`Hi` roll range and a separate `Max` cap (confirmed by the .tas save
  dump in run400 `Form1.frm` ~3942-3960, which writes per-NPC
  `[0]Attitude [2]Stamina [6]MaxStamina [8/10/12]StrLo/Hi/Max ...
  [32]Speed [36]counter`).  "Change Strength by N" shifts **both Lo and Hi**;
  "Change Max Strength by N" shifts only the cap.  Combat rolls read Lo/Hi only
  (the cap is for the status display), so this is faithful to the model.

## Validation

- `harness/sctype7probe.c`: type-7 action counts per game match this doc's
  table exactly (Phoenix 42, gateway 36, Spirit's Flight 29, Azra 22,
  Villains 1, inverness 4, ...).  All 17 battle games + 60 general-corpus games
  load (the one pre-existing "Murder Most Foul" load failure reproduces on HEAD).
- `harness/scattrtest.c`: apply rules, clamps (stamina floor/ceiling, Max
  re-clamp, ranged floor at 0), attitude UI remap, Max-via-combat-read,
  player target -- all pass.
- `harness/scsertest.c`: every new field round-trips through
  save->restore (`ScareBattleState/2`); `/1` saves still read.
- `harness/sce2etest.c`: firing real tasks through `task_run_task()` (the full
  runner path) changes an NPC's attitude -- Spirit's Flight task 23 NPC13 and
  inverness task 8 NPC3 both go Neutral(0)->Enemy(2).
- Behaviour-preserving: `status` output and full intro text are **byte-identical**
  to a clean HEAD build across 5 battle games, and 4 non-battle games are
  identical, confirming the read refactor changed nothing for existing play.

(Harnesses live in `~/adrift-battle/harness/`; build recipe in that repo's
README.)

---

## Goal

Implement ADRIFT's task **action type 7** — "Change `<battle attribute>` of
`<character>` by/to `<value>`" — so that a character's combat attributes
(Attitude, Stamina, Max Stamina, Strength, Accuracy, Defence, Agility, Speed)
can be altered at runtime by tasks, instead of being fixed at the values in the
`.taf`. This is the last substantial Battle System gap (see
`TODO_battle_system.md` item 5b: the `.tas` interop dead-end), and unlike that
one it is worth doing.

### Why it matters (corpus usage)

Type-7 actions are common in real battle games — **11 of the 17 corpus games**
use them, several heavily. Counted with a throwaway probe over
`Tasks[*].Actions[*].Type == 7`:

| Game | type-7 actions |
|------|---------------:|
| Phoenix_Destiny | 42 |
| gateway | 36 |
| The_Spirits_Flight | 29 |
| The_Town_Of_Azra | 22 |
| To_Hell_And_Beyond | 13 |
| SecretOfLostWorld | 8 |
| lifesimulation | 6 |
| The_Search_For_Mr_Smith | 5 |
| The_X-Files_A_New_Beginning | 5 |
| inverness | 4 |
| Villains_And_Kings | 1 |

(Orient_Express, Shadow_Of_The_Past, Sun_Empire, Nonsense_Machine, Toxically_Earth,
hyper_b_s: none.) Today every one of those actions is silently dropped.

The single highest-impact sub-case is **Change Attitude** (target 0): it lets a
task flip an NPC neutral↔ally↔enemy mid-game (e.g. provoke a guard). Combat
target-selection already reads attitude, so that sub-case "just works" the moment
attitude becomes mutable, and can ship as a first increment ahead of the full
attribute matrix.

## What is already in place (the hooks)

- **Parser stub.** `sctafpar.c`, the `TASK_ACTION` schema in both the V4.0 table
  (`~line 135`) and the V3.9 table (`~line 225`) ends with
  `?#Type=7:iVar1,iVar2,iVar3`. The `i` prefix (`PARSE_IGNORE_INTEGER`) parses
  the three parameters for stream alignment but **discards** them. The fix is the
  same `i`→`#` change already made for the `BATTLE` / `OBJ_BATTLE` / `NPC_BATTLE`
  tables — `i` and `#` consume identically, so alignment is unchanged.
- **Executor stub.** `sctasks.c`, `task_run_task_action()` has
  `case 7: /* Battle options, ignored for now... */ break;` ready to fill in.
- **Display.** `lib_print_battle_status()` (`sclibrar.c`) already shows the
  attributes; once they are read from mutable state it reflects changes for free.
- **Live stamina + counters + wield** are already mutable game state and
  serialised (see `scserial.c` `SER_BATTLE_MARKER`). Max Stamina and the other
  attributes are NOT yet — they are still read straight from the bundle.

## Implementation plan

### 1. Reverse-engineer the action semantics (the one real unknown)

Decode the three integers `Var1/Var2/Var3` of a type-7 action, and the apply
rule. Sources, both already on disk:

- **gen400 (editor)** `/tmp/petter2/Modules.bas` ~lines 1290–1389: prints
  "Change `<attr>` of `<character>` by/to `<value>`". Establishes the attribute
  enum and the by/to + target wording:
  `0 Attitude, 1 Stamina, 2 Max Stamina, 3 Strength, 4 Max Strength,
   5 Accuracy, 6 Max Accuracy, 7 Defence, 8 Max Defence, 9 Agility,
   0xA Max Agility, 0xB Speed`. Target is "Referenced Character" vs a specific
  character; value is a literal or (possibly) a referenced variable.
- **run400 (runner)** `/tmp/petter/Form1.frm` (strip NULs / `grep -a`): find the
  type-7 **executor** — the code reached from the task-action dispatcher that
  applies the change. This gives the authoritative apply rule. Anchor by
  searching for the attribute write into the player(40) / npc(172) battle
  sub-structs, near where attitude/stamina indices are assigned.

Questions to answer here:
- Exact packing of `Var1/Var2/Var3`: which is attribute index, which is target
  character (player vs referenced-NPC vs specific NPC), which is the value, and
  where the **by (delta) vs to (set)** flag lives.
- For the ranged attributes (Strength/Accuracy/Defence/Agility): "Change Strength"
  vs "Change Max Strength" are separate targets. Determine whether "Change
  Strength by N" shifts **both Lo and Hi** of the roll range (and is clamped to
  Max), and whether "Change Max Strength" raises/lowers the cap. The `status`
  command shows "Range  Max  Current", confirming each attribute carries a
  Lo–Hi range plus a Max cap.
- Whether the value may be an expression / variable reference (type 3 "Change
  variable" uses `$Expr`/`#Var5`; type 7 might only take a literal — confirm).

### 2. Parser: store the parameters

`sctafpar.c`: change `?#Type=7:iVar1,iVar2,iVar3` → `?#Type=7:#Var1,#Var2,#Var3`
in **both** the V4.0 and V3.9 `TASK_ACTION` tables. (If RE shows a value can be a
variable/expression, mirror the type-3 pattern with `$Expr,#Var5` instead.)

### 3. Mutable per-character battle state

Today `battle_attribute_range()` / `battle_attitude()` / `battle_get_property()`
in `scnpcs.c` read straight from the prop bundle, so attributes are constant.
Add a runtime override layer:

- New fields in `sc_game_t` (player) and `sc_npcstate_t` (NPC), `scgamest.h`:
  per character, current **Lo/Hi/Max** for Strength, Accuracy, Defence, Agility
  (≈12 ints), plus **Max Stamina** and **Speed**, and for NPCs **Attitude**.
  (Live Stamina / staminacounter / attackcounter already exist.)
- Mirror the existing battle fields: add to `gs_copy()` (undo-safe), zero/seed in
  `gs_create()`, and add `gs_(set_)*` accessors + prototypes in `scprotos.h`.
- **Seeding.** `battle_start()` (`scnpcs.c`) already runs at game start and on
  restore-without-state; extend it to seed every mutable attribute from the
  bundle (the configured Lo/Hi/Max/Speed/Attitude), exactly as it already seeds
  Stamina. A clean way: keep `battle_attribute_range()` reading the bundle for
  the *initial* seed only, and read the mutable state everywhere else.

### 4. Read refactor

Point the combat reads at the mutable state:
- `battle_attribute_range()` → return the stored current Lo/Hi for the named
  attribute (and `battle_attribute_max()` → stored Max).
- `battle_attitude()` → stored attitude (NPCs).
- `battle_speed_roll()` → stored Speed.
Keep this surgical; it is the hot combat path. Everything downstream
(`battle_eff_*`, target selection, `status`) then reflects changes automatically.

### 5. Executor

Fill in `case 7:` in `task_run_task_action()`: read `Var1/Var2/Var3`, resolve the
target character (player / referenced NPC / specific NPC), and apply the decoded
change to the mutable state with by/to semantics and Max clamping. Factor the
apply into a `battle_change_attribute()` helper in `scnpcs.c` next to the other
`battle_*` code. Re-clamp current Stamina to the new Max Stamina when that
changes.

### 6. Serialisation

Extend the `SER_BATTLE_MARKER` block in `scserial.c` to write/read the new
mutable fields (player block + per-NPC block), alongside the stamina/counter/wield
values already there. Bump the marker to `ScareBattleState/2` and keep reading
`/1` for backward compatibility (older saves: seed the new attributes from the
bundle via `battle_start`, read the v1 fields). Update `gs_copy` first so undo
stays correct.

### 7. Testing (behavioural — no golden possible)

- Add the type-7 count probe to the harness set (load game; walk
  `Tasks[*].Actions[*].Type`).
- Drive the heavy users (Phoenix_Destiny, gateway, Spirit's Flight, Azra) and a
  light one (Villains_And_Kings); trigger the tasks that carry type-7 actions and
  confirm `status` shows the changed attribute, that combat math uses it, and
  that an NPC whose Attitude is changed starts/stops attacking.
- Confirm the new state round-trips through save/restore and through `#undo`.
- Re-run the full corpus smoke for no crashes, and confirm non-battle games are
  untouched (type 7 never fires there).

## Risks / open questions

- **Main risk:** the exact `Var1/Var2/Var3` decode and the by/to + Max-clamp rule
  for ranged attributes (Step 1). Bounded — both the editor and runner decompiles
  are available — but must be confirmed, not guessed.
- Whether a type-7 value can reference a variable/expression (affects Step 2).
- Interaction with the per-turn `#undo`/`synchronizeSave` footgun (see
  `comprehend-synchronizesave-undo-footgun` for the shape of that class of bug):
  ensure attribute changes are applied once per action, not re-applied on the
  per-turn undo snapshot.

## References

- `TODO_battle_system.md` — items 1–6 done; item 5b (.tas Runner interop) parked.
- `adrift-battle-formulas-re` memory — battle sub-struct field indices
  (`[0]Attitude [2]Stamina [6]MaxStamina [8/10]Str Lo/Hi …`).
- `scare-battle-system-port` memory — `.tas` save layout RE and decompile
  locations (`/tmp/petter` run400, `/tmp/petter2` gen400; archives
  `~/Desktop/Petter.7z` / `Petter2.7z`).
- Estimate: ~2–4 focused days; mostly state-plumbing boilerplate (Steps 3–4)
  plus the Step-1 RE. The Attitude-only slice is a few hours and can land first.
