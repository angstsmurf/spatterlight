# TODO — Alien Diver residual divergences

Alien Diver is WON and its core mechanics work. It is still kept OUT of the
strict a5 walkthrough suite. This file tracks its FrankenDrift-vs-Scarier diff.

Repro harness (no dotnet needed for the Scarier side):
```
./test/a5run_dump test/adrift5-games/AlienDiver.blorb test/AlienDiver_walkthrough.txt
FD_RNG=xoshiro FD_SEED=1234 sh test/a5_groundtruth.sh \
    test/adrift5-games/AlienDiver.blorb test/AlienDiver_walkthrough.txt
```
The blorb is gitignored (copyright); it lives at `test/adrift5-games/AlienDiver.blorb`.

The vanilla column stays a Scarier self-golden (FD's System.Random lands the ship
in a different room, so vanilla FD never matches — the game is only diff-able
under `FD_RNG=xoshiro`). **xoshiro hunk-line count history: 826 (original) →
600 (after BUG 1) → 436 (after the `<#if#>` fix) → 32 (after BUG 3) →
15 (after the win-banner `<cls>` fix) → 0 (after BUG 4 + the v5 empty-room
listing grammar).**

## ✅ DONE — full MATCH 0|0, wired into the strict suite

Alien Diver is now byte-exact against FrankenDrift under `FD_RNG=xoshiro` and is
wired into `test/run_a5_walkthroughs.sh` (`AlienDiver|AlienDiver.blorb|0|0`) with
a blessed `test/AlienDiver_expected.txt` vanilla golden. The whole a5 golden
suite stays green in both RNG modes + save/restore. BUG 4 and the room-listing
grammar (below, formerly "STILL OPEN") are fixed.

---

## FIXED

### BUG 1 — `pc` (play crafted card) over-counted colored fragments  ✔ FIXED

Playing a crafted card awarded **N** colored fragments (N = number of crafted
cards held) instead of 1. Root cause: **`ShowCardsP3` is `<Repeatable>0</Repeatable>`
and is `Execute`'d once per crafted card to stamp each a unique `CardId`
(`Assignidco` counter). Scarier re-applied the Completed/Repeatable gate PER
group-member, so `ShowCardsP3` ran only for the FIRST card; the rest kept a stale
duplicate `CardId`. Then `pc`'s `CraftACard4` restriction `CardId … EqualTo
%number%` matched EVERY held card, so `CraftACard4` (and its `+fragment`) fired
once per card.**

Fix (`a5run_action.cpp`, SetTasks-Execute handler): the Completed/Repeatable
gate is FD's `AttemptToExecuteTask` ENTRY test, evaluated ONCE per Execute — a
non-repeatable task run over a plural group reference must still fire its actions
for every member (FD's `ExecuteSubTasks` iterates items inside one execution;
`Completed` flips only afterwards). Snapshot the flag at entry (`tt_done_at_entry`)
and gate on that, instead of the live `task_done[tti]` which a task that marks
itself done on member 0 would trip for members 1..n. After the fix each crafted
card gets a unique `CardId` (1,2,3,…) and `CraftACard4` fires for exactly the
selected one. Verified via `A5_TRACE_RESTR`.

### BUG 2 — save/restore did not round-trip runtime state  ✔ FIXED

A mid-script `A5_SAVE_AT=N` round-trip was not byte-identical (~126 hunks).
TWO distinct gaps:

1. **`gm` live group-member list not reconstructed on restore.** Our save
   serialises runtime group membership only as the `@InGroup:<grp>` property
   overrides (`PropOv`); the ordered `arlMembers` list that `RandomKey` /
   `EverywhereInGroup` enumerate was never rebuilt, so after restore they saw
   stale (static-only) membership. Alien Diver drives cube/room state through
   `AddLocationToGroup` (`Cubeextrac`, `CubeUnlock1`, `OceanEvery`, `CubeInacti`)
   and picks random rooms with `RandomKey`. Fix: new
   `a5state_rebuild_live_groups()` (a5state.cpp) rebuilds `gm` from the restored
   effective membership (still-present statics in model order, then runtime adds
   in `st->ov` creation order); called at the end of `restore_scarier_body`.

2. **FD-schema `<Group>` (de)serialiser only handled `Objects` groups.** Save
   wrote the static model list for Locations/Characters groups and restore
   ignored them. Fix (`a5run.cpp`): `group_candidate_count/_key` iterate the
   right entity space per group type; save writes live membership for EVERY
   group type, restore reconciles the listed `<Member>`s across the whole
   candidate space (so runtime removals restore too).

Verified: `A5_SAVE_AT={20,30,32,50}` all give diff 0; the whole a5 golden suite
(`XOSHIRO=0 SAVERESTORE=1`) stays green (no game regressed).

### BONUS — `<#…#>` multi-word OO value blanked the status line  ✔ FIXED

The `Crashed Ship Location:` status field
(`<#if(%ShipFound%=1,""&CrashedShi.Location.Name&"","Unknown")#>`) rendered EMPTY
(~40 status-line hunks). Root cause: `replace_expressions` (a5text.cpp) resolved
a `<#…#>` body's bare OO chains with `a5expr_replace` (DISPLAY mode, no quoting).
A multi-word value like `CrashedShi.Location.Name` → `Ocean M053` was emitted
UNQUOTED, so the sexpr tokeniser split it at the space and dropped the tail — and
as an argument to `if(..)`'s `&`-concat it reduced to nothing (short arg list →
`if` gets <3 args → empty). Fix: use `a5expr_replace_expr` (EXPRESSION mode,
`bExpression=True`) so a non-integer OO value is emitted as a quoted string
literal (FD's `ReplaceOO(bExpression:=True)`). Verified byte-exact on the
expression, and no golden regressed (this pattern is AlienDiver-only in the
corpus).

### BUG 3 — first play never awards the cube-power crafting bonus (SCORE 1 vs 2)  ✔ FIXED

At the FIRST `pc`, FD awarded a `+3` crafting-fragment bonus TWICE (`Playerfrag`
0→6) and Score 0→1; Scarier awarded 0. The reward (`RollDieP2P` / `RollDieP2P1`,
`IncVariable Playerfrag = 3`) is gated on `Playersrol Must BeEqualTo Targetnumb1`
— the PLAYED card's power (`%object%.CraftedCar`, set in `CraftACard4`) vs the
cube's target number (`SetVariabl`). The played card's `CraftedCar` was **0**, so
it never matched; no bonus, no score. This cascaded to every later
`Crafting Fragments:` / `Game Score:` / `Crafted Cards:` status echo AND to the
win (FD reaches the take-off, scoring 2; Scarier reached the generic ending,
scoring 1).

**Root cause — TWO compounding bind bugs, both confirmed by tracing `CraftedCar`
SetProperty on BOTH engines (Scarier `[SETPROP]` / a patched FD `[FD-SETPROP]`;
FD binds `ReferencedObject2` to the CARD — `ARedBlankC2`/`BlankCard41`/
`ARedBlankC1` — for every craft):**

1. **Single-pass arg bind.** `CraftACard2` runs `Execute CraftACard8
   (%Player%.Location.Objects.ObjectIsAT | %object%)`. `run_one` bound arg0
   `object1 := cube` (which also aliases the bare `ReferencedObject`, see
   `bind_reference`) BEFORE resolving arg1 `%object%`, so
   `eval_arg_to_key("%object%")` read the just-clobbered cube instead of the
   parent's card.
2. **Sibling Execute pollution.** `CraftACard2` runs `Execute CraftACard5`, `…8`,
   `…6`, `…7` in sequence, each `(…ObjectIsAT | %object%)`. Even with a two-pass
   bind, the FIRST sibling (`CraftACard5`, the blue branch) left `object1`/bare
   `ReferencedObject` = cube, so the NEXT sibling's `%object%` read that cube.
   Only blue (first in the list) resolved correctly; green/yellow/red got the
   cube. This is why the earlier naive two-pass made the diff WORSE (436 → 640):
   it needed the sibling isolation too.

**Fix (`a5run_action.cpp`, SetTasks-Execute `run_one`):** mirror FD's
`clsUserSession.vb:2169-2310` exactly — (a) resolve ALL non-passthrough args
against the parent state, THEN apply the binds (deferred two-pass), and (b) wrap
each Execute in FD's `oExistingRefs = NewReferences … NewReferences =
oExistingRefs` by snapshotting the reference table (`ref_snap_take`) before the
dispatch, restoring it before each group member, and restoring once after — so a
sub-task's bindings are LOCAL to that Execute and don't leak into the parent's
`%object%`. After the fix each craft imprints `CraftedCar` on the CARD, byte-exact
with FD. **436 → 32; whole a5 golden suite still green (0 regressions, both RNG
modes + save/restore).**

### BONUS — win banner + take-off narrative swallowed by the EndGameText `<cls>`  ✔ FIXED

At the winning `take off`, FD showed the take-off narrative, `*** You have won
***`, then the `Well Done…` EndGameText; Scarier showed ONLY the EndGameText.
Root cause: AlienDiver's `<EndGameText>` opens with `<cls>`, and Scarier's
end-of-turn `sb_resolve_cls(out, 0)` wiped the WHOLE turn back to that `<cls>` —
including the banner and narrative. FD's `CheckEndOfGame` Displays the banner with
`bCommit=True` (clsUserSession.vb:515) BEFORE the WinningText (vb:528), and the
headless renderer clears only within a commit (`Program.cs` EmitHtml's per-call
StringBuilder), so the EndGameText `<cls>` can't reach the already-committed
banner. Fix (`a5run.cpp` `emit_endgame`): resolve the EndGameText's `<cls>`
against a floor pinned right after the banner, so it wipes only its own region.
**32 → 15; golden suite still green.**

---

## FIXED (the last 15 residual — the seen/listing family)

Both were in the "room-render / seen-timing" family (`obj_seen` / the listing
grammar feed the WHOLE corpus), so each landed with a full golden-suite pass.

### BUG 4 — `HaveBeenSeenByCharacter` timing (dominated the 15 residual)  ✔ FIXED

The remaining 15 lines are essentially ONE root cause. FD sets `HasSeenObject`
early (init `PrepareForNextTurn` before the first render, and per-turn BEFORE the
turn's Look override runs); Scarier's `update_seen` runs at END of turn, so a
same-turn `Must HaveBeenSeenByCharacter %Player%` restriction sees the object as
not-yet-seen and fires one turn late. Two symptoms:

* **Opening cube status** (`41c41` + `42a43,47`): the opening render's `Look`
  AfterTextAndActions override `ShowCubeSt1 → Execute ShowCubeSt` fails
  `ShowCubeSt`'s `ReferencedObject Must HaveBeenSeenByCharacter %Player%`, so
  Scarier prints `Sorry, I'm not sure which object you are trying to #.` instead
  of the `Cube Status: … GREEN light pulsates..Cube Power Number <6>` block.
  (Note FD renders the opening TWICE — render 1, via a Look-task path, carries
  the Cube Status; render 2, the raw `ShowFirstRoom` `ViewLocation`, does not —
  so matching this needs FD's exact init render ordering, not just the seen flag.)
* **Ship-location lag** (`3180c3185`, `3220c3225`, `3260c3265`): the status field
  `Crashed Ship Location: <#if(%ShipFound%=1,…CrashedShi.Location.Name,"Unknown")#>`
  shows `Unknown` for 3 extra echoes. Same root: `Shipfound = "1"` is gated on
  `CrashedShi Must HaveBeenSeenByCharacter %Player%` (model line ~12689), which
  Scarier satisfies one turn after FD.

**Fix — mirror FD's `clsCharacter.Move` (clsCharacter.vb:524-533).** FD marks the
arrival location, its objects and its visible characters seen *at move time* —
inside `Move`, before the room render and before the movement's
`AfterTextAndActions` overrides run — not (only) at the turn boundary. Scarier's
`update_seen` refreshed at turn boundaries, so a same-turn `HaveBeenSeen` gate on
a fresh arrival (the ship-repair `ResetRollC → CheckIfPla1 → Shipfound`, and the
opening `ShowCubeSt1 → ShowCubeSt`) fired late. New `mark_player_arrival_seen`
(a5run_action.cpp) marks loc + visible objects + visible characters seen for the
player the moment the MoveCharacter action lands them AtLocation (gated on
`char_onobj == NULL`, matching FD's `ToWhere.ExistWhere = AtLocation`). This alone
fixed BOTH the ship-location lag AND the opening cube-status block (arriving at the
start room via Setup's MoveCharacter marks the cube seen before the opening Look
override) — no separate init-render-ordering hack needed. Player-centric like the
rest of `obj_seen`/`char_seen`; whole golden suite green in both RNG modes +
save/restore. **15 → 2.**

### v5 empty-room listing grammar (`41c41`, `80c85`)  ✔ FIXED

`Also here is a Cube 6 .` vs FD `There is a Cube 6  here.`. FD's `ViewLocation`
lists general objects as `There is <list> here.` (no leading pSpace) when the
room *body* — long description + special-listed objects, NOT the room name — is
empty on a v5 game, else the trailing `Also here is <list>.`
(clsLocation.vb:132-139). Scarier always used `Also here is`. Fix
(view_location_impl, a5text.cpp): capture whether the body render (`proc`) is
empty and pick the wrapper accordingly. **2 → 0.**

---

## ✅ After BUG 4 — DONE

Wired into `test/run_a5_walkthroughs.sh` as `AlienDiver|AlienDiver.blorb|0|0`,
blessed `test/AlienDiver_expected.txt` with `-b`, and updated the write-up in
`test/A5_WALKTHROUGH_FINDINGS.md`. AlienDiver reports `MATCH 0|0` (vanilla golden
+ xoshiro FD-diff) with save/restore OK.
