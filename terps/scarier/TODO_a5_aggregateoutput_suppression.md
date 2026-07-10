# DONE — Halloween hole-examine listing suppression (LANDED 2026-07-07)

**Status 2026-07-07: RESOLVED and committed.** Both the `%object%`→key fix and
the `.Children`/`.Contents` **entity-type** filter landed; Halloween holds
**MATCH 0|0**, ISummonThee improved **xoshiro 2→1**, and the whole 42-game corpus
stayed all-MATCH (unit tests green). The last-mile fix turned out to be **simpler
and different** from the ref-snapshot theory the rest of this file describes — see
**"THE ACTUAL FIX"** below; the historical trace is kept for the record.

## THE ACTUAL FIX (what really made Halloween match)

The ref-lifecycle / object2-snapshot theory below was a **partial misread**.
`dk_ListOnlyFi` (`.Contents`, Inside) renders **fine** — object2 *is* still bound
(`dk_Krog`) when its eager completion emits, it renders `Inde i hullet er krogen.`,
and Scarier's `replace_alrs` chain-blanks it to `""` exactly like FD. That line
was never the problem.

The garbage `Inde i hullet er 0.` came from a **different** sub-task,
**`dk_ListFirstL1`** (`Execute dk_ListFirstL1 (%object%|%object%.Children(Characters, In))`),
which must NOT fire (its gate is `%object%.Children(Characters, In).Count>0` — the
*characters* inside the hole, of which there are none). Scarier's `.Children`/
`.Contents` handler ignored the **entity-type** half of the arg and always
returned **objects**, so `.Children(Characters, In)` picked up the hook `dk_Krog`
(an object *inside* the hole) → `.Count = 1` → the gate wrongly passed → the task
ran with `%character%` unbound → `Inde i %object%.dk_BestemtBøj er %character%…`
rendered `Inde i hullet er 0.`.

**Fix (a5expr.cpp):** `oo_children_set()` now mirrors FD's `ReplaceOOProperty`
`Children`/`Contents` dispatch (Global.vb:826-911) faithfully across **both**
dimensions — entity type (Objects / Characters / Both) *and* where (In / On /
OnAndIn) — and returns the **empty set** for any arg combination FD's `Select
Case` doesn't match (a bare `On`, `Characters,On` on a hook-only container, …).
Added `chars_children()` (characters on/in an object via `char_onobj`/`char_in`).
With `.Children(Characters, In)` correctly empty, `dk_ListFirstL1`'s gate fails
and the garbage line never renders — **no ref-snapshot needed**.

The `%object%`→key half (a5text.cpp `eval_function`) is still required and still
FD-faithful (`%objectN%` → `MatchingPossibilities(0)` = the entity KEY,
Global.vb:1792 / `PossibleKeys` clsUserSession.vb:3931): it lets the SetTasks
reference-list arg `%object%.Children(...)` chain from the real key (so the list
tasks fire at all), and separately fixes ISummonThee's standalone `%object%`
(keys `Nymphs`/`Door` == their nouns → "The Nymphs"/"The Door", not "The Comely
Nymphs"/"The Vault Door"). Zero corpus blast radius; the two synthetic unit tests
that used a bare `%object%` probe with arbitrary keys (`Key1`/`Coin1`/`Lamp1` ≠
their nouns) were updated to expect the faithful key rendering.

---

## Historical trace (the ref-snapshot theory — superseded by the fix above)

## The case (unchanged)

Halloween (`test/adrift5-games/Halloween.blorb`), command **`undersøg hullet`**.
The generic `ExamineObjects` task shows the hole's custom description and then
runs four Danish-library auto-listing sub-tasks via `SetTasks Execute` (game XML
~line 7101):

```
Execute dk_ListOnlyFi (%object%|%object%.Contents)              -> "Inde i ... er ..."
Execute dk_ListFirstL (%object%|%object%.Children(Objects,On))  -> "Oven på ... er ..."
Execute dk_ListFirstL1 (%object%|%object%.Children(Characters,In))
Execute dk_ListCharSI (%object%|%object%.Children(Characters,On))
```

The hook `dk_Krog` is *inside* the hole (`dk_Hul2`, a Container). `dk_ListOnlyFi`
(`.Contents`, Inside) legitimately passes — the hook is inside — and FD's own
restriction trace confirms it passes in FD too. FD nonetheless prints **no**
listing line. Reproduce with:

```sh
cd terps/scarier
awk '/undersøg hullet/{print; exit} {print}' test/Halloween_walkthrough.txt > /tmp/hw_short.txt
FD_DLL=$(find ~/frankendrift/FrankenDrift.Headless/bin -name fd-headless.dll | head -1)
FD_SEED=1234 dotnet "$FD_DLL" test/adrift5-games/Halloween.blorb /tmp/hw_short.txt
```

---

## THE ANSWER (verified byte-exact by instrumenting FD)

`dk_ListOnlyFi` is `AggregateOutput=True`, so FD stores its completion template
**raw** in `htblResponsesPass` (clsUserSession.vb:1210-1211: `If Not
AggregateOutput Then render` — aggregate keeps the raw). At the parent
(`ExamineObjects`) flush the flush loop does `NewReferences = refs : Display(sMessage)`
(clsUserSession.vb:851-855) with the raw template as `sMessage`.

`Display` (clsUserSession.vb:298) itself does **not** run ReplaceFunctions — but
it calls `ReplaceALRs(sText)` (:308), and **`ReplaceALRs` renders functions
FIRST** (Global.vb:519):

```vb
Public Function ReplaceALRs(sText, ...)
    sText = ReplaceFunctions(sText)        ' :523   %object1%/%LCase[..]% -> real text
    sText = ReplaceExpressions(sText)      ' :524
    For Each alr In htblALRs               ' :529   then match ALRs against the RENDERED string
        If sText.Contains(alr.OldText) Then
            sText = sText.Replace(alr.OldText, ReplaceALRs(alr.NewText, False))   ' recurses -> chains
```

So the raw `Inde i %object1%.dk_BestemtBøj er %LCase[%object2%.dk_BestemtBøj]%.`
renders to **`Inde i hullet er krogen.`**, and then two **chained author
TextOverrides** fire (both in the game XML, keys `dk_IndeIHulle` / `dk_IndeIHulle1`):

| ALR | OldText | NewText |
|-----|---------|---------|
| `dk_IndeIHulle`  | `Inde i hullet er krogen.`   | `Inde i hullet er en krog.` |
| `dk_IndeIHulle1` | `Inde i hullet er en krog.`  | *(empty)* |

`Inde i hullet er krogen.` → `Inde i hullet er en krog.` → **`""`**. The author
deliberately blanks this auto-generated line. `AggregateOutput=True` matters only
in that it keeps the template unrendered until Display, so the render+ALR match
happen together on the rendered string; a non-aggregate task would pre-render at
emit and the *same* ALR would still match the *same* rendered string.

**One-paragraph rule (success criterion #1):** *FD does not suppress the passing
AggregateOutput sub-task at all — it renders its completion (`Inde i hullet er
krogen.`) at the parent's Display flush, where `ReplaceALRs` runs
ReplaceFunctions/ReplaceExpressions before matching ALRs; the game author's own
chained TextOverrides (`Inde i hullet er krogen.` → `Inde i hullet er en krog.`
→ empty) then blank that exact rendered string. It is author content
suppression, not engine output suppression.*

Trace evidence (instrumentation reverted): `DISPLAY` on the raw template →
`ReplaceALRs` logged `in=[Inde i hullet er krogen.] … out=[]`, matching
`dk_IndeIHulle`+`dk_IndeIHulle1`; committed `sOutputText` never contained the
line.

---

## Why Scarier still can't match (the real remaining bug)

Scarier's `replace_alrs` (a5text.cpp:1743) **already** renders
functions/expressions first (via `process_inner`) and chains recursively with
FD's `sText = sALR` identity stop. So if Scarier rendered the aggregate to the
exact string `Inde i hullet er krogen.`, the same two author ALRs would blank it.
**The suppression path is not missing.** The blocker is that Scarier renders the
line as `Inde i hullet er 0`, which no author ALR matches.

Two coupled defects produce that:

1. **The `%object%`→key fix (bug #15 / I-Summon-Thee row) is needed to make
   `dk_ListOnlyFi` fire at all,** and it also feeds `%object2%` into the
   completion. Verified 2026-07-07: applying it (bare `%objectN%` returns the
   entity key, mirroring FD `ReplaceFunctions` = `MatchingPossibilities(0)`,
   Global.vb:1792) has **ZERO corpus blast radius** on the vanilla/golden pass —
   the entire 42-game corpus stays byte-identical; the *only* regression is this
   Halloween line. So the fix is well-scoped and safe *except* here (this
   contradicts the earlier "corpus-wide blast radius" fear).

2. **`ReferencedObject2` is UNBOUND when `dk_ListOnlyFi`'s completion renders,**
   so `%LCase[%object2%.dk_BestemtBøj]%` → `0`. Root cause (traced): the four
   `SetTasks Execute` list tasks bind `object2` in turn; `dk_ListOnlyFi` binds
   `object2 = dk_Krog`, then the sibling `dk_ListFirstL`
   (`%object%.Children(Objects,On)` — an empty set after the bug-#15 `.Children`
   filter fix) binds `object2 = ""`, and `dk_ListOnlyFi`'s completion renders
   **after** that clobber (Scarier's eager, `run->resp == NULL`,
   `emit_completion` path — confirmed: `EMIT o2=[] m=[Inde i hullet er 0.]`).
   FD is immune because it stores the raw template *with its NewReferences* and
   **restores `NewReferences = refs` at the Display flush**
   (clsUserSession.vb:851-854), so object2 is `dk_Krog` again → `krogen`.

If object2 were still bound at render, Scarier would render `Inde i hullet er
krogen.`, `replace_alrs` would chain-blank it to `""`, and the turn would match
FD — no new suppression logic required.

## The remaining fix (to actually land it)

Give the **eager SetTasks-Execute AggregateOutput completion** a per-response
reference snapshot restored at render time, mirroring FD's
`NewReferences = refs` at Display. Notes for whoever picks this up:

- Scarier already has the snapshot primitive: `ref_snap_take` / `ref_snap_restore`
  and `resp_entry.snap` (a5run_action.cpp:575-607), used by the *aggregate resp*
  flush (`resp_flush`, the `e.aggregate` branch at ~887-921 restores the snap
  before `a5text_describe`). The single-item aggregate case there (obj_keys
  empty) already restores the snap and renders — so that flush would render
  `krogen` correctly.
- The problem is `dk_ListOnlyFi` does **not** reach that path: `run->resp` is
  `NULL` for this command, so its completion goes through the eager
  `emit_completion` (run_task, a5run_action.cpp ~3491-3506 for a Before
  completion), which renders against live (clobbered) bindings. `resp_add_comp`'s
  aggregate branch is additionally gated on a non-empty `rm->cur_item`
  (a5run_action.cpp:714) which a SetTasks-Execute never sets.
- So either (a) route SetTasks-Execute AggregateOutput completions through a
  snapshot-bearing deferral even on the `resp == NULL` path, or (b) snapshot the
  refs in the `SetTasks Execute` `run_one` (a5run_action.cpp ~2862) and restore
  them around the completion render. Whichever, **verify the whole corpus stays
  all-MATCH** — the move/Date "AggregateOutput but render-at-flush-is-fragile"
  cases (see the comment at a5run_action.cpp:707-714) are the ones to watch.

## Success criteria

1. ✅ **DONE** — the exact FD rule: chained author ALR in `Display → ReplaceALRs`,
   not an engine AggregateOutput drop. (`dk_ListOnlyFi` renders `Inde i hullet er
   krogen.` and `replace_alrs` chain-blanks it, exactly like FD.)
2. ✅ **DONE** — Scarier reproduces it. The garbage line was a *different*
   sub-task (`dk_ListFirstL1`, `.Children(Characters, In)` wrongly returning the
   object hook); the entity-type filter in `.Children`/`.Contents` fails its gate,
   so `undersøg hullet` prints no listing — matching FD. No object2-snapshot
   needed. Corpus stays all-MATCH; unit tests green.
3. ✅ **DONE** — the `%object%`→key fix landed cleanly: Halloween MATCH 0\|0 holds,
   ISummonThee xoshiro **2→1** (nymph + "The Door" name).
4. Last ISummonThee hunk is the completion-message **RNG draw-order** (`The Door
   quite simply explodes.` vs FD `… is swallowed up by roaring flames.`) — see the
   I-Summon-Thee row. Separate TODO, still open.
