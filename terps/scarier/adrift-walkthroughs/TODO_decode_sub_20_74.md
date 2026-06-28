# TODO: decode the ADRIFT 4 Runner's task room-gate (`Sub_20_74`) to settle the SCARE "No Rooms" divergence

> **CLOSED 2026-06-25 — verdict: faithful, do NOT patch. CONFIRMED on the real
> Win32 `run400.exe` Runner** (Petter): the first-prompt No-Rooms probes
> (`east`/`up`/`down`, `say through adversity to the stars`, `examine outhouse`)
> all returned the faithful responses ("You can only move south.", the generic
> `say` reply, "You see no such thing.") — the Runner does not run No-Rooms tasks
> for the player either. The premise below is
> wrong: `Sub_20_74` is **not** the task room-gate. It's a command-reference /
> exit *scope* filter (switches on a reference-type 0/1/2 + sub-types 0–5, indexes
> the object/character arrays, accessibility via `General.Sub_22_54`, `0x9C`=156
> "nowhere" sentinel), called only by the pattern builders `Sub_20_64`/`Sub_20_75`.
> The "conditional where-type-0 path" is the reference-type-0 branch, unrelated to
> `ROOMLIST_NO_ROOMS`. The Runner's task-match path is `Sub_20_12` → executor
> `Sub_20_11` (the sole caller of `Sub_20_11`) with no No-Rooms enablement.
>
> Decisive evidence that No-Rooms tasks must stay non-player-runnable (so SCARE is
> faithful and *Through time* is unplayable-by-design, an authoring error):
> structural — Through time's No-Rooms movement tasks are identical in shape to
> Melbourne Beach's No-Rooms *subroutine* tasks; empirical — flipping
> `NO_ROOMS→TRUE` is byte-identical for FunHouse / To_Hell_And_Beyond / Sun_Empire
> but **breaks Melbourne Beach** (No-Rooms task 60 hijacks `get dry clothes`).
> Reverted; tree clean. Full write-up in `WALKTHROUGH_TODO.md` (Through time entry).
> Everything below is the original (now-superseded) investigation plan.


**Why.** *Through time* (native 4.0) is unplayable past the opening house in
SCARE because 82 % of its tasks have `Where = No Rooms` (`ROOMLIST_NO_ROOMS`,
type 0) and `sctasks.c task_can_run_task_directional` returns `FALSE` for that
case unconditionally. The ADRIFT 4 Runner appears NOT to hard-block No-Rooms
tasks — its gate `mdlSpreadTheLoad.Sub_20_74` has a *conditional* path for
where-type 0. We must learn the Runner's **exact** No-Rooms rule before touching
the engine, because the whole bundled corpus relies on
`NO_ROOMS → not player-runnable` and a wrong change would silently break it.
See the "Through time" entry in `WALKTHROUGH_TODO.md` for the full context and the
reverted blanket-enable experiment (which mis-fires task 91 `south` everywhere).

**Goal.** Produce a precise, English description of "when is a task runnable in
the current room?" as the Runner computes it — covering all five `Where.Type`
values (0 No-Rooms, 1 One-Room, 2 Some-Rooms, 3 All-Rooms, 4 NPC-Part) — then
decide: faithful (Runner also can't play it ⇒ document unwinnable) vs a SCARE
divergence (⇒ fix `task_can_run_task_directional`, keep corpus byte-identical).

## Files / ground truth

- Disassembly: `~/Desktop/run400.txt` (P32Dasm of `run400.exe`, VB6 P-code).
  3.9 runner for cross-check: `~/Desktop/run390.txt`. Symbol map:
  `~/Desktop/run400.map` (only names form handlers + Form29; the core
  `mdlSpreadTheLoad`/`General` subs stay numbered).
- Read with `LC_ALL=C` (`grep -a`, `sed -n`). The disasm interleaves
  `ADDR: opcode  Mnemonic operand` lines; routine headers end in ` ----`.

## Routine map (line numbers in run400.txt)

| line  | routine | role |
|------:|---------|------|
| 58731 | `mdlSpreadTheLoad.Sub_20_11` | **task executor** (runs a task's restrictions+actions; the recursion guard "Taskno: " / "…recursive task" is here) |
| 62190 | `mdlSpreadTheLoad.Sub_20_12` | **dispatcher**: nested `ForI2` over tasks → calls `Sub_20_64` (predicate) then `Sub_20_11` (execute) |
| 62415 | `mdlSpreadTheLoad.Sub_20_13` | **field/list accessor** (loops a list, compares `param_10`; likely "is room N in the task's room list" or a generic record getter — used by Sub_20_74's later branches) |
| 79957 | `mdlSpreadTheLoad.Sub_20_64` | **predicate** the dispatcher calls per task; does command-pattern matching ("  " concat) AND calls `Sub_20_74` |
| 83287 | `mdlSpreadTheLoad.Sub_20_74` | **the room-gate** (returns I2 boolean 255/0); switches on a where-type-like arg |
| 99808 | `General.Sub_22_54` | called inside Sub_20_74's where-type 0/1 branches (decode it too) |

Task-command indexing (by verb, not room): `mdlSpreadTheLoad.Sub_20_6`
(line 48782, "Indexing tasks…") — confirms No-Rooms tasks ARE found by the
parser; the room gate is applied later, in the Sub_20_12→64→74 path.

## What is already decoded about `Sub_20_74`

Entry (run400.txt 83287+):
```
var_86 = False                 ; result
var_88 = param_C               ; the SWITCH value (0,1,2,3,4 → matches ROOMLIST_*)
if (var_88 == 0):              ; --- No Rooms ---
    if (param_10 == 0): result = True
    else: if (arr[param_10-1].[1E] == param_14 - 1): result = True
    goto exit
if (var_88 == 1):              ; --- One Room ---  (same shape: param_10==0 → True; else arr[param_10-1].[1E]==param_14-1)
    ...
; later branches (var_88 == 2/3/4 …) call Sub_20_13(LitI4 2|3, 1) and test a
; returned field against 156 (0x9C) — see below
```
Caller (`Sub_20_64`, run400.txt ~80002–80016) pushes three values taken from the
**same task record's `Where` sub-record**, at sub-indices **2, 1, 0** (each via
`LitI4:n; FMemLdRf; AryInRecLdPr; MemLdI2 [0]`), then `Call Sub_20_74`.

Sentinel `156 (0x9C)` recurs (e.g. `MemLdI2 [1A]` compared `== 0x9C` / `<> 0x9C`)
— almost certainly ADRIFT's "no room / hidden / nowhere" marker (analogue of
SCARE's `-1`). Field offset `[1E]` (=30) is read off `arr[param_10-1]` — `arr` is
likely the rooms or tasks array; identify it.

## Open questions to nail (in priority order)

1. **Param ↔ Where-field mapping & VB6 arg order.** `var_88 = param_C` is the
   switch and takes values 0–4, so `param_C` *is* `Where.Type`. But the caller
   loads Where sub-indices in order 2,1,0 — so determine the VB6 P-code arg
   convention here (push order vs. `param_C/param_10/param_14` offsets) to know
   which Where sub-field is `param_10` and which is `param_14`. Method: it's
   self-consistent — `param_C` must be the field that holds 0..4 (the Type), so
   whichever load yields Type maps to `param_C`; back out the other two from
   there. Cross-check against SCARE's parse order
   (`sctafpar.c {ROOM_LIST0}`: Type, then Room (ONE_ROOM) or the Rooms[] bool
   array (SOME_ROOMS)).
2. **No-Rooms (Type 0) condition.** Decode exactly when the Type-0 branch returns
   True: what is `param_10` (is it 0 for a genuine No-Rooms task, making it
   *always* runnable, or is it a room/char index so the gate is location-aware?)
   and what does `arr[param_10-1].[1E] == param_14-1` test? This is the crux: if
   Type-0 is unconditionally True ⇒ Runner runs No-Rooms tasks everywhere (then
   why isn't *Through time* chaos in the Runner? — reconcile with the task-91
   problem), if conditional ⇒ describe the condition.
3. **Identify `arr` and field `[1E]`/`[1A]`.** Dump the UDT by finding the .taf
   loader that fills the tasks/rooms arrays (search for the routine reading task
   records during load) and map offsets → field names. `[1A]`(26)/`[1E]`(30) and
   the `0x9C` sentinel are the keys.
4. **`Sub_20_13` and `General.Sub_22_54`.** Decode both — Sub_20_13 looks like
   "is value in list / get Nth", Sub_22_54 is called only in the Type 0/1
   branches. They likely implement the actual room-membership test.
5. **Dispatcher ordering (`Sub_20_12`).** Confirm task iteration order and
   whether a *first-match-wins* rule (plus restriction/where gating) explains why
   task 91 (`south`, no restriction) does NOT hijack every `south` in the real
   Runner — i.e. the gate must be excluding it by location.
6. **3.9 cross-check (`run390.txt`).** Find the analogous gate (anchor: the
   " can't go in any direction!" LitStr at run390.txt line 41720; walk back to
   its movement/dispatch routine). If 3.9 and 4.0 differ on Type 0, that's the
   conversion-damage angle: a 3.9 "general task" may map to type 0 with different
   semantics than 4.0.

## Decisive shortcut if static RE stalls

Run the real **Win32 `run400.exe`** on `~/adrift-battle/games/Through time.taf`
under **Wine** (it's a VB6 GUI app, not DOS — dosbox-mcp won't run it). Type the
opening (`<enter>`, `out`, `south`, …). Ground truth in two commands:
- If the Runner moves past the porch and the game plays coherently ⇒ **real SCARE
  divergence**: No-Rooms tasks must be runnable under the Runner's condition →
  port that condition into `task_can_run_task_directional` (and prove the bundled
  corpus stays byte-identical: run all `harness/*_solution.txt` before/after and
  `diff`; rebuild with `sh harness/build.sh`).
- If the Runner is *also* stuck on the porch / incoherent ⇒ **faithful**:
  document *Through time* as unplayable-by-design and move on.

## Deliverable

Either an engine fix in `terps/scare/sctasks.c` (`task_can_run_task_directional`
No-Rooms case) that matches the decoded Runner rule + a corpus byte-identity
proof, **or** a written "faithful, unplayable" verdict with the Runner evidence.
Update the "Through time" entry in `WALKTHROUGH_TODO.md` with the resolution.
