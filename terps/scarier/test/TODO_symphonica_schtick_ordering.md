# TODO — Symphonica64 residual 263-hunk xoshiro divergence (DONE)

Status: **DONE** (2026-07-25). Fixed — **263 → 0 hunks, Symphonica64 is now
MATCH 0|0** (byte-identical to FrankenDrift under the aligned xoshiro stream).
The residual came apart into THREE engine gaps, not one:

1. **The schtick draw-order swap diagnosed below** — but the safe fix was NOT
   the feared global `StartDescriptionWithThis` deferral. The real FD
   mechanism is `AggregateOutput` (default True, FileIO.vb:1617): an
   event-fired task (`bChildTask=False`) stores its completion RAW and expands
   it — drawing the `<# OneOf #>` — in the Display loop at the END of its own
   `AttemptToExecuteTask` (clsUserSession.vb:782,851-856,1208-1213), after
   every SetTasks-Execute'd sibling's eager restriction rand. Scarier already
   had exactly this deferral on the player-command path (`comp_defers` /
   `display_defers`); the event/walk/LocationTrigger path simply never armed
   it. Fix: `attempt_event_task_impl` arms the sink around `run_task` and
   partially flushes its own entries (`a5run_flush_display_defers_from`) at
   the end of the attempt. Scoped to the event path — no other draw moved.

2. **Deferred sexpr draws must flush in TEXT order, not push order** — the
   ZoneXxxx teleport completions embed `Player.Location.Description` AFTER a
   `<#OneOf#>`; Rory's `CharHereDesc` taunt `<#OneOf#>` inside that view was
   pushed to the sink during the OO pass, i.e. before the textually-earlier
   teleport OneOf. FD's ReplaceExpressions scans the substituted message left
   to right (Global.vb:510-516,523-524). The flush now orders untagged sexpr
   entries by sentinel text position (`\001`/`\002` entries keep their slots).

3. **`.Isscore(False)` SelectionOnly list filter** ignored its argument — the
   two inventory hunks. `(False)`/`(0)` inverts the filter (ReplaceOOProperty,
   Global.vb:971 vs 1040). Fixed in `oo_prop` (`a5expr.cpp`).

Golden re-blessed, MAP `Symphonica64|symphonica.blorb|0|0`. Full suite:
**116 MATCH / 11 DIVERGE, 0 FAIL**, no other game moved in either column;
save/restore, a5 unit tests, v4 corpus all green. Full write-up in
`A5_WALKTHROUGH_FINDINGS.md`. Original diagnosis kept below for the record —
note its "why it was not fixed" reasoning assumed the deferral had to be
global; the AggregateOutput event-path scoping is what made it safe.

---

Original parked diagnosis (2026-07-25, superseded):

This is the leftover after the Barry-follower loader fix (`resolve_carrier`,
2116 → 263 hunks — see `TODO_following_you.md`).

## What the 263 hunks are

All 263 hunks are on **Symphonica64 only** and are **271↔271 pure line
substitutions** — never insertions/deletions. Every one is a schtick `OneOf`
variant landing on a different index (Rory: "glaring at you angrily" /
"being generically vicious" / "chewing your toe"; the exposed Karateka's 5
lines; Sir Mart's 12 lines). The text Scarier prints is always a **valid**
alternative random taunt — just not the same index FD picked.

## Root cause — a per-turn RNG draw-ORDER swap (not a count bug)

Both engines draw an **identical 4138-value** xoshiro stream (same multiset,
verified). They diverge only in the *order* of two draws per turn, which then
shifts which value each schtick `OneOf` consumes. Fresh traces
(`A5_TRACE_RAND=1` vs `FD_RNG_TRACE=1`, both `*_RNG=xoshiro seed=1234`):

```
idx   SCARIER              FD
211   RAND(0,4)=1          RAND(1,100)=52     <- schtick OneOf vs Bystander restriction
212   RAND(1,100)=94       RAND(0,4)=3
213   RAND(0,4)=0          RAND(1,100)=6
...   (same swap repeats at 431/614/927/... every schtick location)
```

- **Scarier** draws `schtick OneOf` **then** `Bystander rand(1,100)`.
- **FD** draws `Bystander rand(1,100)` **then** `schtick OneOf`.

### Why

Parent task **`Schtick1`** (priority 219, System, Repeatable) `SetTasks Execute`s
its children *in list order* (a5dump.xml ~line 30074):
`SchtickThe, SchtichMar, SchtickExp, SchtickDod, SchtickThe1, SchtickGen, Bystander1`.
`Bystander1` (priority 50274, restriction `rand(1,100)<4`) is **last**.

FD `SetTasks/Execute` (`clsUserSession.vb:2158-2310`) runs sub-tasks
synchronously in that list order, so `Bystander1` runs *after* the schticks.
Yet FD draws Bystander's rand *before* the schtick OneOf. The reason: a
**`StartDescriptionWithThis` CompletionMessage** (all the schticks use it) is
treated by FD as a **location-description override whose text — and its `OneOf`
— is rendered later, at room-display / `ToString` time**, i.e. AFTER every
sibling System task has eagerly evaluated its `rand` restriction. Restriction
rands are eager; the schtick's descriptive `OneOf` is deferred.

**Scarier** renders a task's CompletionMessage **eagerly inside `run_task`**
(`a5run_action.cpp` `run_task` / `render_look_string` path), so the schtick
`OneOf` is drawn the moment `SchtickExp` executes — before `Bystander1` runs its
restriction. Hence the swap.

## Why it was NOT fixed (safety)

Matching FD means deferring the `OneOf` evaluation of every
`StartDescriptionWithThis` System-task completion until room-render time
(after sibling task restriction draws). That reshuffles the xoshiro stream for
**every one of the ~125 corpus games** that uses description-appended System
tasks with random text — a high-risk global engine change for a **cosmetic,
single-game, already-valid-text** residual. Guidance was to document it as an
irreducible task-render-ordering / RNG artifact rather than risk the whole
corpus. The `FD_RNG=xoshiro` alignment is itself a testing artifact (real FD
uses `System.Random`); the residual text is correct either way.

## If ever resumed — the shape of a safe fix

Only a change that defers *exactly* the `StartDescriptionWithThis`
System-task-completion `OneOf`/`RAND` render to the same phase FD uses, while
leaving all other draws untouched, could close this. It MUST pass the whole
suite (`./test/run_a5_walkthroughs.sh`) with **zero** previously-MATCH game
regressing, and any game that legitimately re-orders would need re-blessing.
Verify the deferral against FD's actual description-override render path first
(`Global.vb` `Description.ToString` StartDescriptionWithThis case, line ~3914,
plus wherever a System task's completion is registered as a location override).

## Repro

```
cd terps/scarier
A5_RNG=xoshiro A5_TRACE_RAND=1 ./test/a5run_dump test/adrift5-games/symphonica.blorb \
    test/Symphonica64_walkthrough.txt >/tmp/scar.txt 2>/tmp/scar_rand.txt
DOTNET_ROLL_FORWARD=Major FD_RNG=xoshiro FD_RNG_TRACE=1 FD_SEED=1234 \
    dotnet <fd-headless.dll> "$PWD/test/adrift5-games/symphonica.blorb" \
    "$PWD/test/Symphonica64_walkthrough.txt" >/tmp/fd.txt 2>/tmp/fd_rand.txt
# grep 'RAND(' each stderr, diff -> first swap at draw #211
```
