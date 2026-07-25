# TODO — Quest Giver Scarier↔FD divergence (`Symphonica`-sibling: the daylight clock phase-shift)

Status: **intentional DIVERGE, not a bug in the winning walkthrough.** Wired at
`QuestGiver|QuestGiver_v4.blorb|0|396` — vanilla 0 (Scarier's own golden), xoshiro
396 (measured FD baseline). The walkthrough itself is a legitimate MAX 21/21,
Grand Total 1199 win. This TODO is about *closing the 396-hunk xoshiro gap*, which
is only worth doing if the fix is safe against the other ~125 corpus games.

Full root-cause writeup lives in memory `questgiver-daylight-stale-ref.md`; this
file is the code-level fix plan.

## Symptom

10-day adventurer-management game. Hiring is daylight-only (daylight starts 20/day;
night rejects gives with "You cannot give quests out at night time"). Scarier hires
~4 adventurers/village, FD only ~3: **FD reaches nightfall earlier**, so its later
gives are night-rejected and the two transcripts diverge and compound over 10 days
(396 hunks). It is **not** an RNG bug (streams are byte-identical through 3 hires)
and **not** a simple daylight-rate constant.

## Root cause — an emergent FD stale-`NewReferences` artifact

The 10-day clock is a global TurnBased event `daz6GlobalRunP` (Length 1, Repeating)
that fires 6 `ExecuteTask` subevents/turn in order:
`daz6CheckIfAny`, `daz61DaylightC` (daylight−1 while `daz6Daytrue==1`),
`daz6IsItNightY` (flip to night when `daz6Daylightco<=0`), `daz61DaylightC1`,
`daz6IsItNightY1`, `daz6GameEndsWh`. Daylight starts 20; only `daz61DaylightC`
decrements it.

On a turn where a quest-give **completes** and ≥1 quest is already active, FD
decrements daylight by **N** (# active quests) instead of 1:

- `daz61DaylightC` declares **0 references**, but FD's `AttemptToExecuteTask`
  (`clsUserSession.vb:757`) runs `InReferences = task.CopyNewRefs(NewReferences)`.
- `CopyNewRefs` (`clsTask.vb:363`) **ignores the task's declared ref count** and
  copies the whole ambient `NewReferences` array (all N items).
- `ExecuteSubTasks` then loops the task's actions once per item in
  `InReferences(0).Items`, so `DecVariable daz6Daylightco="1"` runs N times.
- The ambient `NewReferences` on a give-completion turn is a **single merged
  multi-item response** (the preceding `daz6CheckIfAny` Display loop leaves
  `NewReferences` = the last displayed response's refs = all active quests).

Scarier already models this exact mechanism — `resp_flush`
(`a5run_resp.cpp:444-458`) leaves `st->n_ref_items` equal to FD's post-Display
`NewReferences`, and the event path (`a5run_events.cpp:382-403`) iterates the
0-ref daylight task once per leftover item. Amazon's `get ammo and rifle`
+2-minute tick proves the machinery works.

**The divergence is purely WHEN the plural leftover forms:**
- **FD** front-loads it: the give-completion response is one merged multi-item
  aggregate (refs = all active quests) on the give turn, so daylight −N *that* turn.
- **Scarier** forms per-member responses at give time (leftover singular), and the
  plural leftover only appears 1–2 turns later via a *different* path — the
  EVENT-fired group-Execute in `act_set_tasks` (`a5run_action.cpp:2724-2749`,
  the external `giter` member loop). So the two clocks **phase-shift** and compound.

## Why it wasn't just fixed — the architectural tension

Matching FD means the **event-fired group-Execute must MERGE identical-text child
responses into one multi-item aggregate** (so its leftover is plural on the give
turn). But the **command path** `view cards` (`daz6ViewCardsP`) **explicitly
requires per-member, non-merged iteration** — one card printed per held quest,
each with its own ID stamp (documented at `a5run_action.cpp:2542-2571`; binding the
whole pipe-list to one reference printed all three quests as a single merged card).

Command vs event ARE separate code paths, so in principle the merge can be applied
to one and not the other — that is the crux of the fix. Pure pacing cannot help:
FD's faster clock compounds, so no give-scheduling MARGIN keeps every hire in
daylight under both engines (tested: `qgplay5` MARGIN=6 still diverged, FD 13 vs
Scarier 20 hires).

## Fix approaches (ranked)

### A. Merge the event-path group-Execute leftover (recommended, highest value/risk)
Make the EVENT-fired `act_set_tasks` group loop leave the same plural
`st->n_ref_items` leftover FD does, WITHOUT touching the command `view cards` path.

- Site: `a5run_action.cpp:2724-2749` (the `giter >= 0` member loop) and the
  `resp_flush` leftover computation (`a5run_resp.cpp:436-458`).
- Idea: when a group-Execute runs under an **event** context (not a player command)
  and its members produce identical-text completions, accumulate their reference
  items into ONE aggregate entry (mirroring `resp_add`'s `obj_keys` merge that
  already exists for genuine plural `%objects%` commands) so `lo_multi` becomes 1
  and the leftover is plural on the give turn.
- Gate strictly on event-context so the command `view cards` path
  (`a5run_action.cpp:2542-2571`) stays per-member. There is already a
  command-vs-event distinction to hang this on — thread a flag from the event
  dispatcher (`a5run_events.cpp`) through `act_set_tasks` rather than inferring it.
- Risk: **high**. This reworks the response-merge layer (`resp_flush` / `lo_multi`)
  that all ~125 corpus games flow through. Any game whose event-fired group-Execute
  currently leaves a singular leftover would start ticking per-member TurnBased
  events N times. Must re-run the WHOLE suite and diff every previously-MATCH game.

### B. Special-case the 0-reference declared task (surgical, lower blast radius)
FD's multiplication only bites because `CopyNewRefs` ignores the **declared** ref
count of a 0-ref task. Rather than reshaping when Scarier's leftover goes plural,
replicate FD's over-copy narrowly: when an EVENT-fired ExecuteTask targets a task
that declares 0 references but the ambient leftover is plural, iterate its actions
once per leftover item (FD's exact `CopyNewRefs` behaviour).

- Site: `a5run_events.cpp:382` — the guard is already `if (st->n_ref_items > 1)`.
  The gap is the *timing* of `n_ref_items`, so B alone doesn't fix the phase-shift
  unless combined with making the leftover plural at give time (i.e. still needs A's
  merge). B is really a **correctness cross-check** to confirm Scarier's per-item
  event iteration matches `CopyNewRefs` semantics for declared-0-ref tasks, not a
  standalone fix.
- Risk: low, but **insufficient by itself**.

### C. Accept and document (current state)
Keep DIVERGE 0|396. The walkthrough is a real MAX win; the vanilla golden guards
Scarier's own determinism; xoshiro 396 is the pinned FD baseline. This is the
status quo and is defensible: the divergence is an emergent artifact of FD faithfully
reproducing a jcwild/ADRIFT-5 Runner quirk (`CopyNewRefs` over-copy), not a
Scarier semantic error.

**Recommendation:** attempt **A** only as an isolated change with the whole-suite
regression gate below; if any previously-MATCH game regresses and can't be cleanly
event-context-gated, fall back to **C**.

## Verification / re-bless (if attempting A)

1. Reproduce the mechanism first (no code change): re-confirm N-per-turn daylight
   decrement on give-completion turns via
   `A5_DUMP_VARS=DayCounter,daz6Daylightco,daz6DayCounter ./test/a5run_dump test/adrift5-games/QuestGiver_v4.blorb test/QuestGiver_walkthrough.txt 2>&1 | grep daz6Daylightco`
   and compare to the FD cache transcript's daylight trace.
2. `make -f Makefile.headless a5run`.
3. `FD_RNG=xoshiro ./test/a5_groundtruth.sh QuestGiver_v4.blorb test/QuestGiver_walkthrough.txt`
   — target is the 396 → ~0. Re-measure exactly.
4. **Whole-suite regression is mandatory** — A touches shared response-merge code:
   `./test/run_a5_walkthroughs.sh`. Require **0 FAIL** and **zero previously-MATCH
   game regressed**. Pay special attention to games with plural `%objects%`
   commands + per-turn TurnBased events (Amazon `get ammo and rifle`, Book of Jax
   `put all in bag`, AoS `You are not carrying …`) — these exercise exactly the
   `lo_multi` / `n_ref_items` leftover logic A modifies.
5. If clean: re-bless — regenerate `QuestGiver_expected.txt` from the new transcript
   and update the MAP row `QuestGiver|QuestGiver_v4.blorb|0|<new-xoshiro-count>`
   (ideally 0, promoting it MATCH).
6. `A5_SAVE_AT` round-trip byte-identical; a5 unit tests green.
7. Keep the FD oracle **pristine** — all `FD_DL_TRACE` debug patches were reverted
   and the Adrift+Headless DLLs rebuilt; do not re-introduce trace patches into a
   committed state.

## Decision note

This is currently a **deliberate DIVERGE** (option C). Do not treat the 396 hunks
as a walkthrough defect or a Scarier determinism failure — the golden self-check is
green. Only pursue A if the whole-suite gate stays clean; otherwise the emergent
`CopyNewRefs` FD quirk is not worth destabilising the response-merge layer that
125 games depend on.
