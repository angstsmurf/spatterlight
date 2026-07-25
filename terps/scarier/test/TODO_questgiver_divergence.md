# RESOLVED — Quest Giver Scarier↔FD divergence (was DIVERGE 0|396, now MATCH 0|0)

Status: **RESOLVED 2026-07-25.**  The 396-hunk xoshiro gap is closed: QuestGiver
is byte-identical to FrankenDrift in BOTH RNG modes, wired at
`QuestGiver|QuestGiver_v4.blorb|0|0`, golden re-blessed, save/restore OK.
Whole-suite gate: 116 MATCH / 10 pre-existing DIVERGE / 0 regressions; a5 unit
tests green.

## What the root cause actually was

The old writeup ("FD front-loads the plural decrement onto give-completion
turns; Scarier forms it 1–2 turns later") was wrong in the details.  An
FD-side trace (temporary `FD_QG_TRACE` patch, reverted; DLLs restored
byte-identical so the FD cache stayed valid) showed the real pipeline:

1. Every turn the TurnBased event runs `daz6CheckIfAny`, whose group-Execute
   `daz6CheckIfAny4 (daz6ActiveQues.daz7ObjectIsAQ)` passes once per
   active-with-countdown quest.  Its completion is whitespace-only `"\n\n"` —
   which the runner's `AddResponse` **bHasOutput counts as output** — and every
   member's identical raw text **merges into ONE response entry** whose
   reference items accumulate all N quests.
2. At the end of that attempt the Display loop leaves the ambient
   `NewReferences` = the LAST displayed response's items = all N quests.
3. `daz61DaylightC` (0 declared refs) then `CopyNewRefs`s the ambient and
   iterates its `DecVariable` once per item → **daylight −N every daytime
   turn**, N = quests with state=active and countdown>0 at that turn.
4. A silently-failing attempt does NOT leave the ambient alone — the per-item
   `AttemptToExecuteSubTask` ReDim resets it to a task-shaped `[nil]` (so the
   night counter ticks −1, not −N).  Only a Completed&&!Repeatable early-return
   leaves the ambient untouched.
5. The per-item completion is keyed by `CompletionMessage.ToString` — the
   restriction-gated *branch is selected eagerly per item* (that's how
   "Is is now late afternoon.." shows when an intermediate decrement lands the
   counter exactly on 5) — while `<#..#>` function draws happen at Display,
   once per merged entry, in response order.

## What was changed in Scarier (all gated to event/walk/trigger attempts)

- `a5run_internal.h`: new per-attempt response table `ev_resp_tbl`
  (`run->ev_tbl`, installed by `attempt_event_task`; NULL on all command
  paths) + `run->in_ev_attempt` depth flag.
- `a5run_action.cpp run_task`: event-context aggregate After completions
  evaluate the raw branch text eagerly (`a5text_eval_description`, retiring
  DisplayOnce), key the table on (comp node, raw text) — identical raws merge
  (no second render, no second draw), distinct raws each display via
  `a5text_process_frozen` with `<#..#>` draws deferred to the attempt flush.
- `a5run_action.cpp act_set_tasks`: each group-Execute member is attached to
  the table entry its run touched (the refs accumulation).
- `a5run_events.cpp attempt_event_task_impl`: at attempt end the table's final
  entry's items become `st->ref_items` (the post-Display leftover the next
  event task's plural iteration consumes); no displays → `n_ref_items = 0`
  (the `[nil]` reset).  The command `view cards` path is untouched (ev_tbl is
  NULL there) — the per-member card printing the old TODO worried about is
  unaffected.
- `a5run.cpp build_known_words`: object names and character descriptors are
  now added WHOLE (the runner never splits them), so "3" from "quest 3" is
  unknown and the night-rejected menu digit gets `I did not understand the
  word "3".` not the catch-all.
- `a5run_resp.cpp resp_add_comp/resp_flush`: a single-reference AggregateOutput
  completion bearing a text function renders its skeleton eagerly but defers
  the `<#..#>` draw, and `resp_flush` resolves those sentinels in RESPONSE
  ORDER at the entry's slot (the runner's Display draws the movement flavor
  BEFORE the deferred room view's final render).

## Open follow-up (walkthrough, not engine)

The 223-cmd route was derived under the old too-slow clock and hired all 21
adventurers (Grand Total 1199).  Under the corrected clock the same route
night-loses several gives and ends at **9 quests / Grand Total 427** — which is
what real ADRIFT produces for this route; the golden now pins that.  If a
max-score golden is wanted again, the route must be RE-DERIVED against the
faithful clock (hires cost daylight once per active quest, so fewer, earlier,
shorter-countdown quests per day; the old "best4" packing is impossible).
