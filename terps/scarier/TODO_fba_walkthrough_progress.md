# FinnsBigAdventure — walkthrough derivation progress (IN PROGRESS)

Deriving a full-win command script for **FBA v.3c.blorb** (Larry Horsfield,
MaxScore **500**). No public walkthrough exists (author emails it on request;
the in-game `WT`/`WALKTHROUGH` is vestigial — see the wiring TODO). Derived by
blind play through `test/a5run_dump` replay scripts, guided by the game's
**`WWDD`** ("What Would Dad Do?") per-location hint tasks and the model's
scoring tasks.

## Status: **★★ MAX SCORE — 500 / 500, full deterministic win** ("scoring the maximum 500 points!" + credits), byte-verified in BOTH Scarier and FrankenDrift (both 572 turns), script = `test/FinnsBigAdventure_walkthrough.txt`

> **MAX SCORE REACHED (session 7, 2026-07-03): 450 → 500 (+50).** Full deterministic
> max-score win, verified identical in Scarier AND FrankenDrift (both "…in 572 turns,
> scoring the maximum 500 points!"). Only FD-vs-Scarier delta = the one pre-existing
> cosmetic butcher's-stall Paddy event (8× Scarier / 9× FD, a one-turn alignment shift).
> **Internal Score overshoots to 510** (the Chop Shop meal banks +20 when only +10 was
> needed) but the game's win message keys on `Score >= MaxScore` so both engines print
> "the maximum 500 points!" — a legitimate max win. The +50 came from EIGHT additions:
> 1. **Orb chain (+10)** — see the detailed writeup just below (real Orb take + burst bag).
> 2. **Magor's Chamber stand-on-stool (+5, `cl_StepUpOnSt`):** `stand on pouffe` matches
>    the higher-priority no-score `cl_StandOnSto1` (pri 4314 > StepUpOnSt 4241). Use
>    **`stand in pouffe`** — `StandOnObject`'s grammar allows `[on/in]`, but StandOnSto1
>    only allows `[on{to}/upon]`, so "in" dodges it and hits `cl_StepUpOnSt`. (Sibling
>    `cl_XFireplace` "x mantel" +5 is a DEAD task: it needs `OnStool==1 & ObjectsSee==0`
>    but every stand sets ObjectsSee=1 — unreachable, like the mis-located `cl_TieLeashTo`
>    rings whose object lives at loc101 while the task gate is `cl_MilliganSt`.)
> 3. **Read words at Rock Island 2 (+5, `cl_ReadWords`/`51`):** Location30 is S of the
>    junk-disembark room (29); added `s`,`read words`,`n` detour before `sw` to loc90.
> 4. **Mannbroom's 3 questions (+5, `cl_MbTalkScor`):** at the Academy (66, entering runs
>    `cl_InMca` which opens the convo), ask **`1`,`2`,`3`** — but Q3 (`cl_Q1Mb3`) needs the
>    business card INSIDE the pouch, so store it with **`put card in pouch`** (not rucksack).
> 5. **Telescope recon score (+5, `cl_TelescopeS`):** we rowed RI→Kong without ever using
>    the telescope at Rock Island. Added `get telescope`,`look se through telescope`
>    (`cl_LookWestTS2`, gated only on `BeWithinLocationGroup cl_RiBoat` — the boat loc31
>    is in it),`put telescope in rucksack` in the RI boat before `row se`.
> 6. **Fancy Dress Shop questions (+5, `cl_FdsScore`):** we already asked `2` (`cl_Q1Fds`)
>    at the FDS (loc70); added **`1`** (`cl_Q1FdsAsked`) so both `FdsQ1Asked`/`1` are set.
> 7. **Offer money to farmer (+5, `cl_Task1`):** at the farm (114), **`offer money to
>    farmer`** BEFORE `give powder to farmer` (the farmer leaves after taking the powder).
> 8. **The Chop Shop — a SEPARATE Sankora restaurant (+20, `cl_TurnMenuOv`+`cl_ReadMenuDe1`
>    +`cl_EatDessert`+`cl_PayTheBill1`):** entered `in` from `cl_MilliganSt` (loc121 →
>    `sit at table` → loc123), distinct from our Pirate Pie Place (loc82). Inserted mid
>    town-circuit after the antiques sale (we pass `cl_MilliganSt`, have goons, no Paddy
>    yet). Meal flow (timers: main served 4 turns after order via `cl_UnnamedEve`, dessert
>    2 turns via `cl_DessertSer`): `read menu`,`say 1` (main),**`turn menu over`** (+5 —
>    MUST be before eating: `cl_WaiterRetu2` only brings the waiter for the dessert order
>    if `MenuTurned==1`),`wait`×3,`eat meal`,`read menu` (dessert +5),`say 1` (dessert),
>    `wait`×2,`eat dessert` (+5),`pay bill` (+5),`stand up`,`out`.
> **UNREACHABLE / dead-or-exclusive tasks (confirmed, NOT chased):** `cl_XFireplace`,
> `cl_TieLeashTo` (both unreachable data bugs), `cl_BuyCollar` (mutually exclusive with the
> required paper-bag biscuits, `cl_BiscuitsBo==0`), `cl_RowNeFromR` (RI→PI NE crossing —
> the "sixth sense" forces RI→Kong (SE) then Kong→PI (N), so the NE crossing never happens),
> `cl_SankoraSee1` (no executor), and a swarm of alternate-phrasing siblings of points we
> already bank (`cl_GiveTelesc6` tinderbox = mutually-exclusive alt of the telescope gift,
> the loc-123 X/graphics-on menu variants, etc.).
>
> ### The orb chain (+10), in detail
> **450 → 460:** the real Orb of Sankora is now recovered instead of the decoy.
> Three-part fix in the script:
> 1. **Recon (loc 36, Kong trip #1):** the first `look east through telescope`
>    (`cl_LookEastTS`) only sets `cl_OutcropSee/cl_ObjectSeen`. Added a SECOND
>    `look east through telescope` → `cl_LookEastFp3` fires (gated on `cl_OutcropSee==1
>    & cl_StoneSeen==0`) and sets **`cl_StoneSeen=1`**. (Any of the repeat-look variants
>    — `look at outcrop/boulder/object through telescope` — would also set it.)
> 2. **The swap (automatic):** with `cl_StoneSeen==1` AND `cl_SankoraIdD==1` (temple Q3)
>    AND `cl_StoneTaken==0`, leaving the temple (entering **loc 98**) fires the
>    LocationTrigger `cl_SankoraRea`, which moves the REAL `cl_Stone1` onto the outcrop
>    niche (`ToSameLocationAs cl_Stone2`) and hides the decoy `cl_Stone2`. No script
>    change needed — our town circuit already re-enters 98 after answering Q3.
> 3. **Take + burst (Kong trip #2, loc 54 → 124):** `take orb` now grabs `cl_Stone1` →
>    **`cl_TakeStone5` +5** (gated: at 54, `cl_SankoraIdD==1`, `cl_StoneTaken==0`,
>    `%Player%.Held(False).Count<5`); this MOVES the silverback to boundary 124.
>    **FOOTGUN — instant death:** entering 124 while the **suit is WORN** with the leash
>    tied to the sapling fires `cl_InCostumeK` (`EndGame Lose`, "…roar of rage…killing
>    you instantly"). So **remove + stow the costume at loc 33 BEFORE stepping W into
>    124**, and pre-`get paper bag` (retrieve the inflated `cl_Bag1` to HAND — `cl_BurstBag`
>    needs `cl_Bag1 BeHeldByCharacter`, not in the rucksack). At 124: **`burst bag`** →
>    **`cl_BurstBag` +5**, silverback flees (→Hidden), and it yields the burst remnant
>    `cl_Bag3` to your HAND — **`put paper bag in rucksack`** immediately or the later
>    `push off` fails ("both hands free"). The slow death timer (`cl_Silverback1` event,
>    length 5, started by `cl_WToSapling` on entering 124 suit-OFF/leash-tied) is
>    out-run by bursting on the very next turn (the event's Stop control = `cl_BurstBag`).
> The single remaining FD-vs-Scarier hunk is cosmetic: the repeatable loc-98
> Paddy-vs-butcher's-stall flavor event fires 8× in Scarier / 9× in FD (a one-turn
> alignment shift from the +1 recon command); identical score + turn count otherwise.
>
> (This note was written at the 460 checkpoint; the remaining ≈40 were then all banked
> except the confirmed dead/exclusive tasks — see the MAX SCORE summary above.)

> **MAX-SCORE PUSH (session 6): 440 → 450 so far; roadmap to 500 below.** Used
> `A5_TRACE_RUN=1` (dump `[run task <key>]`) intersected with the game's 116 Score
> tasks to see exactly which scoring objectives fired. Added two clean +5s to the
> winning script: **`x fireplace`** (after `stand on pouffe`, `cl_XMantelpie`) and
> **`read notice`** (before `hire junk` at the Junk Office, `cl_ReadNotice1`). Both
> verified no-regression on v.3c AND v.7 (win credits intact).
>
> **Remaining ≈50 pts (distinct objectives still unscored, by cluster):**
> - **Orb chain (~+20, biggest):** `cl_SankoraSee1` (see the Orb) + `cl_TelescopeS`
>   (look at Kong through the telescope during recon — our `look east through
>   telescope` @line 169 doesn't fire it; needs the right spot/direction so
>   `cl_StoneSeen=1`) → that makes **`cl_SankoraRea`** fire on temple-exit (loc 98),
>   swapping the REAL `cl_Stone1` into the niche → then `take orb` scores
>   `cl_TakeStone5` (+5) AND spawns the silverback → **`burst bag`** `cl_BurstBag`
>   (+5). We currently take the decoy generically (no +5) and never burst.
> - **Opening Magor's Chamber:** `cl_XFireplace` "X Mantel" (needs `cl_OnStool==1`;
>   `x mantel` while on the pouffe didn't match — try other nouns) + `cl_StepUpOnSt`
>   "Stand On Stool" (our `stand on pouffe` fires a *chair* task, not this). ~+10.
> - **Magor's Library:** `cl_TakeBook` (a SECOND book beyond the thin one; `take
>   book` after `x books` didn't fire — needs `cl_BookSeen`). +5.
> - **Mannbroom (Pirate academy):** `cl_MbTalkScor` — ask all 3 Qs (`1/2/3`) with the
>   business card in the pouch (`cl_Q1MbAsked/1/2` all ==1). +5. (doc's old "skipped".)
> - **Tie leash to ring (Milligan St 1 `cl_MilliganSt`):** `cl_TieLeashTo` — a 3rd
>   tether spot (needs Paddy + a ring there). +5.
> - **Read words (Rock Island 2, `cl_Location30`):** `cl_ReadWords`. +5.
> - **Collar (`cl_BuyCollar` +5):** mutually exclusive with the paper bag (needs
>   `cl_BiscuitsBo==0`), so probably NOT co-bankable with the Kong bag.
> - Note the loc-123 "turn menu over / dessert" tasks are a SEPARATE restaurant from
>   our loc-82 Pie Place (whose meal+dessert we already scored) — likely an
>   alternative, not additive.
> **FOOTGUN:** giving the tinderbox to a baby *before* the telescope derails the
> gorilla-siesta sequence (score crashed to 365) — don't.

> **CORRECTION (session 6, later): the game is WINNABLE — my earlier "unwinnable"
> claim was a PARSER BUG on my end, not a game bug.** `cl_Location14` (Passageway to
> Dungeons) **already has an ungated `North -> cl_Location12` exit** (plus `South ->
> 25`, `In -> 15`). My connectivity analysis had truncated that Location block,
> because loc 14's LongDescription contains a *nested* `<Location>cl_Location15
> MustNot HaveBeenSeenByCharacter Player</Location>` restriction, and my naive
> `index('</Location>')` stopped at that inner tag — so I never saw loc 14's
> Movements and wrongly concluded the castle was sealed. The game's own `exits`
> command lists "north, south and in" at loc 14; typing **`north`** there climbs
> straight back to the castle. **No patch is needed** (a patched `.taf` was built and
> also wins, but it merely duplicates the exit that already exists).
>
> **The home leg (415 -> 440, appended to the script):** from the catacombs (By Metal
> Doors), `lumino` then **`w,w,w,w,w,w`** (to Bottom of Stairway) → **`up`** (A Small
> Chamber) → **`out`** (Barred Cell) → **`s`** (Near West End of Dungeons) → **`w`**
> (West End of Dungeons) → **`n`** (Passageway 14) → **`n`** (Deep Castle Corridor —
> the exit I thought was missing) → **`e,e`** → **`up`** → **`w`** (Castle Corridor 3)
> → **`remove hat`,`remove smock`** (temple disguise blocks entry) → **`in`** = enter
> Family Chambers → `cl_ToEndgame` **+25 → 440**, homecoming cut-scene ("Finny, we had
> a great day!" + paper-bag callback) + end-credits (lazzah.itch.io). Verified
> identical on **v.3c AND the newer v.7** (2023-09-21). Remaining 60 pts = optional
> island side-tasks (e.g. Pirate-Island costume return) not needed for the win.

> **UPDATE (session 6, 2026-07-03): full KONG ENDGAME + ORB RETURN solved
> (330→415), then hit a hard wall — the game cannot be *won*.** The script now
> plays the entire climax deterministically. New sequence, all confirmed in
> `test/a5run_dump` (script = 519 lines, 520 turns, 415 pts):
>
> ### The Kong endgame (330→405), in order
> 1. **Pet shop paper bag** (Secombe-north 102, `in`): the doc's `1,2,5` order is
>    WRONG — buying the collar first sets `cl_Unnamedvar=1` which blocks the biscuit
>    number entry (`cl_StateNumbe` needs `cl_Unnamedvar==0`). Correct: **`2`** (ask
>    biscuits) → **`5`** (min order → yields `cl_Bag2` strong paper bag w/ `cl_Bone`
>    biscuits inside) → optional **`1`** (collar, now via `cl_BuyCollar11` = **no
>    points**; the +5 collar `cl_BuyCollar` is forfeited once biscuits are bought).
> 2. **Empty + inflate bag**: `empty paper bag` (drops biscuits) then **`inflate
>    paper bag`** (NOT `blow paper bag` — a generic "blow" handler eats that; the
>    `[in/up]` particle is effectively required). +5 `cl_BlowPaperB` → `cl_Bag1`.
> 3. **To the pier** (route `s,s,w×7,n,w` from 102 to Stone Pier 4): passing **South
>    End of Quay 84 WITH Paddy following** fires `cl_RobbedByTh1` = Paddy fights the
>    thieves, **+5** (the death `cl_RobbedByTh` only fires if Paddy is *absent*).
> 4. **Row to Kong**: `board boat`; **`row sw`** (NOT `row to kong` — unparsed). Must
>    first `remove hat`+`remove smock` ("shouldn't wear the hat off the island"),
>    `put all in rucksack`, **`wear rucksack`** (rowing needs both hands free).
> 5. **Kong costume**: at the Boundary (124, the sapling) **`tie dog to sapling`**
>    (+5 `cl_TiePaddyTo`, frees the leash hand — you CANNOT drop the leash without a
>    tether). Then strip: `remove rucksack/jacket/war belt/boots` (stow them in the
>    carried rucksack), `get costume`, `drop rucksack`, `wear costume` (needs BOTH
>    hands empty), `get rucksack`, `get telescope`. (+5 more here — first-visit.)
> 6. **Dung**: to Small Clearing 34 = `e,n,ne` (cut-scene: gorilla craps),`ne`
>    (enter),**`roll in dung`** +5 `cl_RollInDung` (needs suit worn, at loc 34).
> 7. **Gorillas asleep**: **`wait`** (+5 `cl_WaitForSie`, sets `cl_GorillasAs=1`);
>    without it, entering the Large Clearing 37 gets you chased out (`cl_InClearing`
>    needs `GorillasAs==0` to eject; `cl_InClearing1` needs dung-smell `CostumeSme`).
> 8. **Babies**: 34 `s,e` to Large Clearing 37; **`give telescope to baby`** (+5
>    `cl_GiveTelesc`→`3`, babies flee); **`up`** to outcrop 54.
> 9. **Orb**: **`take orb`** grabs the DECOY `cl_Stone2` generically (no +5, no
>    silverback) — the real `cl_TakeStone5` never fires because the REAL orb
>    `cl_Stone1` starts Hidden and is only swapped into the niche by `cl_SankoraRea`,
>    a loc-98 trigger needing `cl_StoneSeen==1 & cl_SankoraIdD==1 & StoneTaken==0`
>    *on leaving the temple* — which our run never satisfied (StoneSeen unset). **BUT
>    the decoy WINS anyway**: `cl_GiveOrbToP` accepts Stone1 OR Stone2 (`#A(#A#O#A#)`)
>    and there is NO fake-orb loss task. `put orb in rucksack`.
> 10. **Leave Kong**: `down,w,sw,w` to 124; **`untie leash from sapling`** (silverback
>    never spawned, so ungated); re-dress (costume off→stow, boots/belt/jacket on,
>    wear rucksack); at the shore the boat is BEACHED — `put rucksack in boat`,
>    board+`disembark boat` to leave Paddy aboard, then **`push off`** (needs both
>    hands free, Paddy aboard). **`row ne`** back to Sankora. `board boat`,`get
>    rucksack`,`wear rucksack`,`disembark boat`.
> 11. **Town→temple** (route `e,s,e,e` + don smock+hat at Edge-of-Town, `e` gate,
>     then `e,e,e,e,n,n` to Bentine-mid 98): **`in`** (priest reveals tether post) →
>     **`tie dog to post`** (+5 `cl_TetherPadd`) → **`s,buy flowers,n`** (temple
>     needs a floral tribute) → **`remove boots`,`in`** (drops boots),**`in`** (enter
>     104) → **`give orb to priest`** = *"It's the Orb of Sankora!"*, **500-goon
>     reward**, `cl_GetOrbInTe` **+25 → 400**.
>
> ### The home-journey (405→415) and the WALL
> After the orb: boat hidden, sailor moved to Pier 2 (88), player at 98.
> - Route out to Stone Pier 4 (`s,s,w×7,n,w`) fires **`cl_BoatStolen`** (`StoneRepla==1`
>   trigger): boat nicked → unlocks the **junk-hire office** (`cl_EnterShop1` needs
>   `BoatNicked==1`).
> - Quayside 83 `in` → office 125: **`hire junk`,`1`,`1`,`1`,`pay 650 goons`** (+5
>   `cl_PayGirl650`, yields the `cl_Ticket`; we had 731 goons; **500 is a refundable
>   deposit** — net 150). Collect Paddy (moved to 84): `s,n,n,w` to Pier 2 88.
> - **`give ticket to sailor`** → board junk; **`sail w`** to Rock Island; **`2`**
>   (dismiss junk, 500-goon deposit refunded) → disembark on Rock Island 29.
> - Metal doors (90): `sw`,**`press 180452`** (napkin keypad code),`open doors`,`in`
>   → catacombs 52. **+10 here → 415.**
>
> **⛔ (RETRACTED — see the CORRECTION at the top; this was my parser bug, the game
> IS winnable via `north` at loc 14. Original erroneous note kept below for context.)**
> **~~WALL — `FBA v.3c.blorb` is PROVABLY UNWINNABLE from here (shipped bug).~~**
> The one and only win trigger is **`cl_ToEndgame`**, which fires *solely* on entering
> the **Family Chambers `cl_Location9`**. A full connectivity analysis of the game
> data (movement exits + every task's `Player Must BeAtLocation`/`LocationTrigger` →
> `ToLocation` edge + all 32 events, **ignoring every gate**) shows the castle cluster
> {`9,3,2,10,80,12,13,5,7,4,8`, Magor's} is a **disconnected component** — only
> **64/140** rooms are reachable from the catacombs, and **none** of them is a castle
> room. Every Castle→dungeon link is **one-way downward** (`12→14`, `3→2`, `7→13`,
> `12→5`), each also sealed by a `StoneRepla==0` gate post-orb, and there is **no
> return edge of any kind** (movement, task, or event) back up. So once you descend
> to fetch the orb you can never return home to trigger the ending. Confirmed the
> parser is sound: from the game START the Family Chambers IS reachable; from the
> catacombs it is NOT. Same class as the repo's `DwarfOfDirewoodForest` (shipped
> unwinnable). **Effective max ≈ 415 + a few island side-points (e.g. costume
> return on Pirate Island `cl_InFds11`); the +25 `cl_ToEndgame` win and full 500 are
> unobtainable.** Independent of engine (pure `.blorb` data) — would fail identically
> in FrankenDrift. *Recommend: confirm with the author / treat 415 as the ceiling.*

## (superseded) Status: **330 / 500** (byte-verified in BOTH Scarier and FrankenDrift — identical 385 turns), script = `test/FinnsBigAdventure_walkthrough.txt`

> **UPDATE (session 5, 2026-07-03): full SANKORA TOWN side-quest circuit solved,
> 285→330 (+45), byte-identical in BOTH engines.** All of `cl_MeetDog`'s gates are
> now satisfied, Paddy the dog is adopted, and the Kong paper bag is in hand. Nine
> sub-quests banked, in this order (see the script for exact moves):
> 1. **Give-horse — this is a TOWN quest, NOT Kong** (the old doc's "cl_GiveHorseS
>    (Kong!)" annotation was WRONG). At **Secombe Avenue mid** (`cl_Location95`,
>    the flower-bed room) a mother holds a **crying baby**: `get wooden horse` →
>    `give horse to crying baby` (+5 `cl_GiveHorseS`, yields the unshatterable
>    **glass ornament**). Temple→here = `out,n,n,w,w,s,s`.
> 2. **Listen** (+5 `cl_ListenToSo1`): at **Seagoon Street** (musicians room, S of
>    the herbalist) `listen to musicians`.
> 3+4. **Herbalist** (Dr.Wu's, In from the herbalist room = south-end Secombe):
>    `give bat and scorpion to herbalist` (+5 `cl_GiveIngred`) → `pay 50 goons to
>    herbalist` (+5 `cl_PayHerbali`, yields the **powder packet**).
> 5. **Farm** (In Farmyard `cl_Location114`, EAST out the SE gate along a dirt
>    road; from the herbalist street = `s` to Seagoon then `e e e e e`):
>    `give powder to farmer` (+5 `cl_GivePacket3`, yields the **goddess figurine**).
> 6. **Antiques** (Milligan St, In): entry is gated on **BOTH the gauntlet AND the
>    figurine being VISIBLE** (`cl_InToAntiqu`; in-rucksack counts as visible, so
>    the figurine from step 5 is the real unlock — do farm BEFORE antiques). Inside:
>    `get gauntlet` (only allowed at loc120 — `cl_Rucksack` blocks it elsewhere),
>    `1`, `2` (sell figurine +100 goons), `sell gauntlet to antiques dealer`
>    (+100 goons and +5 `cl_AntiquesTa3`).
> 7. **Shrine** (+5 `cl_OfferFlowe`): OUTSIDE the SW gate — `buy flowers` at a
>    Bentine stall first, then SW-gate `w` (Edge of Town, guards — disguise passes)
>    `w` (Dirt Road To Town = the shrine), `offer flowers to goddess`. **Arriving
>    here also fires the thieves-rob-a-man-with-a-dog cut-scene (`cl_MeetDog1`).**
>    **DO NOT go further West (the quayside `cl_Location84` = instant thief death).**
> 8. **Dog**: make the leash first — Outside NW Gate has a **length of rope**
>    (`get rope`; this rope alone satisfies `cl_MeetDog`'s "`cl_Rope2` visible"
>    gate — the leash need NOT be tied yet). `n` to **Amongst the Paddy Fields**
>    (`cl_Location117`): `cl_MeetDog` fires (all gates met), the stray dog trots up
>    → `feed stray dog with steak` (+5 `cl_FeedDogSco`, names him **Paddy**, who now
>    follows). Then `make a leash` (+5 `cl_MakeLeash` — the command is only
>    recognised WITH Paddy present).
> 9. **Pet shop** (Petra's, In from Secombe-north — Paddy must be with you to be
>    admitted): `1` (buy leather collar, 5 goons) → `2` (dog biscuits) → `5` (buy 5,
>    min order) → yields the **strong paper bag** (the Kong silverback tool).
>
> **✅ Scarier double-score BUG (FIXED 2026-07-03):** after `make a leash` (+5), typing
> `tie rope to dog` re-fired the score in Scarier (→335) where FrankenDrift stays 330.
> Root cause was NOT the pair I first guessed: the 2nd command resolves to `cl_TieDogWith`
> (Specific override of `tie %object% to %character%`, rope→Paddy), a DIFFERENT
> non-repeatable task that also `Execute`s the shared Repeatable=1 System scorer
> `cl_MakeLeash`. FD gates every `Score` change behind a per-task `clsTask.Scored` flag
> (a task scores at most once, ever; vb:2144) — confirmed by instrumenting FD. Ported to
> Scarier (`st->task_scored[]` + `run->cur_score_ti` set around `run_task`'s action loops,
> gate in `run_action`); leash now 380 like FD, whole corpus still at baseline. Full
> write-up in `TODO_a5_walkthrough_bugs.md` (DONE entry).
>
> **Navigation note:** the dump's numeric `cl_LocationNN` exit indices do NOT match
> the in-game compass labels for the town — navigate the town by PROSE. Real town
> layout (by prose): W→E N-S streets = **Milligan** (antiques; NW gate top, SW gate
> bottom), **Secombe** (pets/flowers-baby/herbalist), **Bentine** (temple, flower
> stalls; SE gate bottom). E-W streets = **Sellars** (north wall), **Seagoon** (south
> wall, musicians at the Secombe junction). The farm is East out the SE gate; the
> shrine is West out the SW gate.
>
> **REMAINING to the win (next session):** the town circuit is DONE — only the
> **Kong endgame** is left. Now standing at Secombe-north with Paddy on a leash,
> the strong paper bag (biscuits), rope, glass ornament, gorilla costume (in
> rucksack). Plan: **empty + inflate the paper bag** (`cl_BurstBag` tool) →
> **Kong trip #2** (`row to kong`): wear gorilla costume, `roll in dung`
> (`cl_RollInDung`, smell), pass the gorillas to the outcrop (54), `take orb`
> = the REAL Stone1 (`cl_TakeStone5`, since `cl_SankoraIdD` was set by temple Q3;
> this SPAWNS the silverback at the boundary), `burst bag` to scare it off
> (`cl_BurstBag`) → `row to buddha` back to Sankora → Temple `give orb to priest`
> (`cl_GetOrbInTe` +25 → `cl_ToEndgame` +25 → **`YouHaveWon`**). Junk-hire to ferry
> Paddy to Kong for tether points is optional (not needed for the win).

> **⚠ WORKTREE:** this work lives on the **`fba-walkthrough`** branch, checked out
> in a SEPARATE git worktree at **`/Users/administrator/spatterlight-fba`** (NOT the
> main `/Users/administrator/spatterlight` tree, which is on `scarier` and stuck at
> the old 160). Always `cd /Users/administrator/spatterlight-fba/terps/scarier`.
> (The shell cwd resets to the main tree between commands — cd every time.)

> **UPDATE (session 4, 2026-07-02):** Sankora town entry + temple solved (250→285;
> the committed script was actually already 250, doc said 245). Now standing INSIDE
> the **Temple of Sankora** (`cl_Location104`), temple questions answered.
> - **Disguise** (guards at Edge of Town gate `cl_Location86` E check smock+hat worn):
>   at Pier 4 arrival `look for a stone` (NOT `get stone` — the noun collides with
>   the Your-Room slab; +5 `cl_FindStone`→`cl_SearchFlow3`) then `fire stone at hat`
>   (slingshot must be OUT of the rucksack; knocks the rice hat off a junk roof, +5).
>   Pier 2 (`cl_Location88`, W of North-End-Quay 87) `get smock` off the washing line
>   (+5). At the gate: `remove rucksack`→`wear smock`→`wear hat`→`e` (order matters:
>   smock needs rucksack OFF + hat NOT yet worn; carry the rucksack in hand after).
> - **Town** = walled Yingtong Town, 3×3 street grid, 4 gates (SW 89 / NW 92 / NE 94 /
>   SE 100). Shops via In: 120 Antiques (In from Milligan-N 101), 122 Pet Supplies
>   (In from Secombe-top 102), 119 Herbalist (In from Secombe 96), 104 Temple (In from
>   Bentine-mid 98). Money changer at **110** (Sellars St, `EXCHANGE n GOLD`, rate 3
>   goons/gold; a sleeping dog at his feet). Flower stalls at **99 & 103** (Bentine,
>   `BUY FLOWERS` 10/12 goons). Milligan St has 3 segments 89→(chop-shop `cl_MilliganSt`)
>   →91→101→92; so SW-gate→money-changer = `n n n n e e e`.
> - **Temple**: needs (a) boots off — `remove boots` then `in` TWICE (first `in` drops
>   them), (b) a floral tribute — `buy flowers` first. Inside 104: `x statue` (+5 niche
>   `cl_XBuddha`), `offer flowers` (+5 `cl_PutFlowers`), `talk to priest`→`1`/`2`/`3`/`4`
>   (+5 `cl_TempleQues`; **Q3 sets `cl_SankoraIdD`** = learn to ID the real Orb of
>   Sankora so Kong gives Stone1 not the decoy Stone2). Leaving temple auto-retrieves
>   boots. Gold now 3 (exchanged 40→120 goons, spent 10 on flowers → ~110 goons).
>
> **THE WIN (confirmed minimal):** `give orb to priest` at 104 with EITHER Stone1 OR
> Stone2 in the rucksack (bracket `#A(#A#O#A#)`) → `cl_GiveOrbToP`→`cl_GetOrbInTe` (+25,
> Goons→500) → `cl_ToEndgame` (+25 → **`YouHaveWon`**). Marker text: *"the Empire..."*
> no — the win narration ends *"...retrieve your boots and Paddy"*; grep `YouHaveWon`.
>
> **THE ENDGAME IS A CONVERGENCE GATE — the dog is adopted LAST.** The paper bag
> (only way past the Kong silverback, `cl_BurstBag`) comes from the **pet shop (122)**,
> which won't admit you without a pet. The pet = the stray dog **Paddy**, fed the
> **dusty pork steak** (`cl_Food`) at the **Paddy Fields (117)**. But `feed stray dog`
> needs `cl_StrayDog` AT 117, placed there ONLY by **`cl_MeetDog`** (@dump 74613),
> whose restrictions require ALL of: `cl_SeeDog` (visit 117 once pre-thieves to see it
> scared off — needs `cl_MeetDog1` NOT complete), `cl_AntiquesTa3`, `cl_ExchangeGo`✓,
> `cl_GivePacket3`, `cl_GiveHorseS` (Kong!), `cl_ListenToSo1`, `cl_OfferFlowe` (shrine),
> `cl_TempleQues`✓, and `cl_Rope2` visible (the leash). `cl_MeetDog1` (thieves rob a
> man+dog at 85, gated on having SEEN gate-89) must be done, but do NOT then step onto
> **South-End-Quay 84** — `cl_RobbedByTh1` there = instant DEATH (thieves knife you).
> Verified in BOTH engines that the dog is NOT feedable until `cl_MeetDog` fires.
>
> **=> Required order to a win (next session):** Kong trip #1 (costume+dung, give
> wooden horse to mother = `cl_GiveHorseS`, do NOT take the orb yet or the silverback
> spawns) → town circuit: shrine `offer flowers` (`cl_OfferFlowe`), antiques `sell
> gauntlet`+2 Qs (`cl_AntiquesTa3`, +100 goons), herbalist give bat+scorpion+pay 50
> (`cl_GiveIngred`/`cl_PayHerbali`→cure), farm give cure to farmer (`cl_GivePacket3`→
> figurine), `listen` for `cl_ListenToSo1`, make leash from the rope at 105
> (`cl_MakeLeash`, `cl_Rope2`) → see dog scared at 117 once (`cl_SeeDog`) → `cl_MeetDog`
> fires → feed dog at 117 → pet shop 122 buy collar+biscuits (paper bag) → empty+inflate
> bag → Kong trip #2: take real orb (Stone1, `cl_TakeStone5`), `burst bag` past
> silverback → `row to buddha` back → temple `give orb to priest` = WIN. Navigation is
> easy: `row to kong` / `row to buddha` (Sankora) / `row to pirate` / `row to rock`.
> Junk-hire (650 goons, office girl at 125) transports Paddy to Kong for the tether
> points but is NOT required for the win.

> **UPDATE (session 3, 2026-07-02):** Pirate Island town fully solved (160→245),
> on the isolated **`fba-walkthrough`** worktree (`/Users/administrator/spatterlight-fba`;
> commits `796b1a47`, `3c4bd579`). Now **On Stone Pier 4, Sankora Island**.
> - **Valuables**: warehouse `look under workbench`→screwdriver; Pier 3 (61)
>   `put screwdriver in rucksack`,`pull rope`,`untie rope`,`get screwdriver`,
>   `force case open with screwdriver`,`open case`,`get bag`,`look in bag`,
>   `put bag in rucksack` (bag must be stowed to walk).
> - **Pie Place** (81→82, kid-friendly; the Davy Jones tavern is adults-only):
>   `sit at table`,`read menu`,`call waitress`,`1`,`2` (main+dessert as convo
>   numbers),`wait`×3,`eat meal`,`eat dessert`,`pay bill` (4 gold; opens the
>   bookshop),`stand up`,`out`,`out`.
> - **Bookshop** (79, opens only after the bill): `read newspaper` (see headline)
>   then **`read article`** (+5, learns valuables' owner); `search shelves` (find
>   Magor's book +5),`buy book` (+5),`read book` (+5, the oven-message clue).
> - **Ring the bell** at Grand Portico (71): +5 and **+150 gold** for returning
>   the valuables. **Hire costume**: Fancy Dress Shop (70) `2`,`search racks`,
>   `hire suit` (120 gold, +10). **Learn to fight**: Academy (66)
>   `get soldiers from rucksack` — Mannbroom trades a lesson+certificate for the
>   toy soldiers/warriors (+5; the cert survives a scripted mugger-fight).
> - **Kitchen** (77→In 76→N 78): `get key` (quick, unseen), `get napkin` (cook
>   ejects you and leaves), `unlock door with key`,`in`,`n`,`put napkin in oven`
>   (+5) → **`read napkin` = code 180452** (the Rock Island keypad, `press 180452`
>   +5), `put key on hook` (+5).
> - **Leave**: fight the mugger, `go to sleep on sails` in the warehouse (+5,
>   clears the forced "exhaustion" event), board at Pier 2, stow loose items,
>   **`row se`** → Sankora (the game's "sixth sense" redirects `row s`/Kong to
>   "Buddha Island" first). Kong (orb) + Sankora temple orb-swap endgame still TODO.
> - **Optional Pirate +5 skipped**: `cl_MbTalkScor` (ask Mannbroom all 3 Qs with
>   the business card in the pouch — one Q gates on `cl_Card` in `cl_Pouch`).

## Status (historical): **160 / 500**

> **UPDATE (session 2, 2026-07-02):** catacombs interior + Rock Island + Kong
> recon now fully derived and FD-byte-clean (only residual = one pre-existing
> intro blank line, `6a7`, unrelated to any move). Reached **160/500**, currently
> **On Pier 2 at Pirate Island** (`cl_Location55`). Sections 9–12 below are NEW.
> **Surfaced + FIXED a 2nd real Scarier engine bug** in the process (see
> `TODO_a5_walkthrough_bugs.md`): the `MoveCharacter … InsideObject/OntoObject`
> and `ToParentLocation` action forms were unhandled no-ops in
> `a5run_action.cpp`, so FBA's `hide in niche` (custodian stealth) never put the
> player inside `cl_Niche1` → the custodian "caught" you where FD plays
> "custodian goes past" (+5). Added the 3 cases (in/on set `char_onobj`+`char_in`;
> ToParentLocation clears them). **Corpus-clean** (all 25 games at baseline,
> vanilla). Rebuild: `make -f Makefile.headless a5run`.

> **Now derived against FrankenDrift (the reference runner).** Surfaced + FIXED a
> real Scarier engine bug in the process: the handfire "winks out when you walk
> into light" mechanic (a repeating TurnBased event → System task) stopped
> working after the first extinguish, because a plural `get X and Y` command's
> event-task path fired completion controls even on restriction failure, killing
> the handfire event (full write-up in `TODO_a5_walkthrough_bugs.md`, top entry).
> With that fixed, both engines behave identically. The script carries an extra
> `lumino` at the West End of Dungeons — needed by BOTH engines now, since the
> handfire correctly winks out during the lit-corridor sneak-out and the dungeon
> is entered dark. Drive FD with: `dotnet <fd-headless.dll> "<game>" <script>`.

The script is a WORK IN PROGRESS — it does NOT win yet, so it is **not** wired
into `run_a5_walkthroughs.sh` MAP. Re-run to confirm the checkpoint:

```
./test/a5run_dump "test/adrift5-games/FBA v.3c.blorb" test/FinnsBigAdventure_walkthrough.txt | grep scored
# -> You have scored 100 points out of a maximum of 500
```

### Sections SOLVED (all in the verified script)

1. **Handshake**: `o` / `no` / `b` (O to continue, NO to the visual-impairment
   graphics prompt, B to begin).
2. **Magor's Chamber** (opening, timed): stand up → `move pouffe by the fireplace`
   → `stand on pouffe` → `get tinderbox and telescope` (one turn) → `step down
   from pouffe` (NOT "get off" — only `cl_StepDown` clears `cl_OnStool`) → `move
   pouffe` (moves it back, +5; the pouffe bursts into flames ≈turn 7 near the
   fire = instant LOSS if you dawdle). `read parchment` (learns the Lumino/Snuffio
   handfire spell + sets `cl_NotesRead`, +5). 10 pts.
3. **Magor's Library** (E, dark): `conjure handfire` (or `lumino`), `look under
   desk`+`get paper`, `read notepad` (reveals "THE KEY!!!!" dungeon combo, +10),
   `x books`+`get thinnest book`, `write the key on the paper` (+5 — copies the
   combo onto the scrap so you have it in the dungeon), `read thin book` (+5,
   REQUIRED to be allowed to leave — points you to the dungeons). 25 pts.
4. **Your Room** (navigate W,out,E,in → dinner cutscene drops you here, dark):
   `lumino`; wear clothes from chest (`wear shirt/trousers/jacket` — shirt before
   jacket), `get boots`+`wear boots`; `remove slab` (+5) → `get gold coins` (from
   the hole) → `replace slab`; collect from table + under bed into the rucksack:
   toy hecatean soldiers + toy Xixon warriors (BOTH sets needed), telescope,
   tinderbox, parchment, paper, wooden horse, slingshot, dusty (pork) steak, ball
   of fluff → `put all in rucksack` between grabs (small hands). Coins do NOT go
   in the rucksack (theft) — they go in the **warbelt pouch**.
5. **Warbelt** (E → "Near Door of Your Room", `cl_Location80`): `get warbelt`
   (hangs on the door hook), `wear warbelt`, `put coins in pouch`. Wicker basket
   here has your discarded bedclothes (tunic/britches/blouse) — not taken; revisit
   if a later disguise needs them.
6. **Sneak-out**: `wear rucksack` (REQUIRED — leaving without it wearing is
   gated: "you might need something to carry things in"). Then E, out (now night,
   no dinner replay), out, w, w (spiral stairway), d (bottom), e, s (into the
   dungeon level). **NB: Your Room's "down" exit is a dead `<Dummy Location>`
   bug-trap (cl_Location6) — do NOT use it.**
7. **Dungeon combination lock** (the big one — fully reverse-engineered, 90 pts):
   The notepad KEY:
   ```
   1: Left to the right   2: Right to the left   3: Right to the right
   4: Leave alone         5: Left to the right, right to the left
   Barred: count 20 up 7 across - push   East: Pull   West: Leave alone
   ```
   Guard Room (In from the Passageway): `search detritus` (rusty key, +5), `get
   mug`. Fill mug at a slimy wall (`fill mug with slime`), `dip key in mug`,
   `unlock south door with rusty key` (cell 5, +5). Enter cell 5, `smear left
   ringbolt with slime` (does ALL cells at once, mug shatters, +5, and auto-kicks
   you back out to Near West End). Then re-enter each cell and turn ringbolts per
   the KEY — the 6 correct moves (cells 1,2,3 = 1 each, cell 5 = 2, + `pull sconce`
   at East End = 6) accumulate `cl_SecretDoor`→6 → door mechanism unlocks (+5+5).
   Cell 4 + West sconce = leave alone. Barred Cell: `count twenty bricks up and
   seven across` (+5) → `press brick` (+5, needs the ringbolts done) → `crawl into
   gap` (+5) → Small Chamber → `d` → Bottom of Stairway → `e` → **The Catacombs**.
   Dungeon map (extracted): cl_Location25 West End(N=passage,E=16);
   16 Near West End(N=18 Barred,E=22,S=20 Cell5,W=25); 22 Near East End(N=19 Cell4,
   E=17,S=21 Cell3,W=16); 17 East End(N=23 Cell2,S=24 Cell1,W=22); 15 Guard Room
   (Out=14 passage); 18 Barred(S=16,In=Small Chamber).
8. **Catacombs — spider web** (blocks south of the central aisle): `get fluff`,
   `get tinderbox`, `burn fluff with tinderbox` (+5), `throw fluff at web` (+5 —
   do it the SAME/next turn, the fluff burns out in ~2 turns; approaching the web
   with the tinderbox directly fails — the spider drives you off). 100 pts.

9. **Catacombs interior — custodian stealth + coffin scorpion** (SOLVED, 100→150).
   Map (aisle `<N>` markers): 49`<0>`(W=27 stairway,E=39); 39`<2>` central aisle
   (N=41,E=43,S=40 niche,W=49); 40`<3 Niche>`(N=39; **red-cross hiding niche**);
   42`<4 Coffin>`(S=43); 43`<5>`(N=42,E=46,S=45,W=39); 46`<8>`(N=44,E=48,S=47,W=43);
   47`<9 Bat>`(N=46); 48`<11>`(N=50,E=52,S=51,W=46); 50`<10>`(N=53,S=48);
   52 By Metal Doors(W=48,Out=90 Rock Is); 53 Custodian's Chamber(S=50).
   - **Custodian**: enter chamber 53 (`e e e n n` from 39) → he sees you
     (`cl_CustodianC=1`) and chases. **Flee with `run` cmds** (a scripted chase:
     `run south`×2, `run west`×3, `run south`) back to niche room 40, then
     `hide in niche` → **`wait`×2** (custodian must shuffle up and pass;
     `cl_CustodianG` +5, `cl_CoastIsCle=1`) → `out`. Fewer than 2 waits = the
     `cl_CaughtByCu` fake-ending (mother sends you to your room = loss).
   - **Chamber loot** (custodian now gone; re-enter `n e e e n n`): table has
     napkin+meal+ale — `get napkin`,`wear napkin`,`eat meal`(+5),`drink ale`(+5);
     `stand on chair`(+5, reveals gauntlet on shelf),`read ledgers`(business card
     falls),`get gauntlet`(while ON chair),`get off chair`,`get card`,
     `wear gauntlet`. `x diagram`(+5 map, red cross = niche 3). **`get napkin` +
     `put napkin in rucksack` BEFORE leaving** — the S exit is gated on the napkin
     (needed for the Pirate oven puzzle) else "something on the table you'll need".
   - **Coffin scorpion** (room 42, `s s w w n` from 53): `search behind coffin`
     (scorpion appears on coffin) → `knock scorpion off coffin`(+5, gauntlet
     needed) → `stamp on scorpion`(+5) → `get scorpion`(auto-rucksacks) →
     `search behind coffin` again (+5 lever) → `pull lever`(+5, `cl_LeverMoved=1`
     unlocks the metal doors).
   - **Bat** (room 47, `s e s`): entering auto-kicks something in the mist →
     `search mist`(dead bat appears) → `get bat`(auto-rucksacks).
   - **Leave**: `n e e` to 52 → `pull handle`(needs LeverMoved=1; doors open) →
     `out` → **Rock Island** (Location90). The door-exit REQUIRES `cl_Bat1` +
     `cl_Scorpion1` in the rucksack + gauntlet visible ("something left behind"
     otherwise) — bat+scorpion are later given to the **herbalist** on Sankora
     (`cl_GiveBatSco`, ingredients for a cure/powder).
10. **Rock Island** (SOLVED, 150→155). Hub. `ne` to the boat, `get in boat`,
    then **stow everything** (put tinderbox/card/gauntlet in rucksack; remove
    gauntlet — worn gauntlet blocks rowing "both hands free"), `get telescope`,
    `look SE through telescope`(+5 first-use `cl_TelescopeS`; the 4 dirs scout:
    SE=Kong/gorillas, NE=Pirate/wharf, E=Sankora/houses, W blocked). Boat is
    **beached**: on shore `push off` to float, then `row <dir>`. The E-side
    **keypad device** (buttons 0-9) is the *return* door mechanism (code TBD).
    Hint `cl_Island1` "Row SE first" → `row se` beaches on **Kong**.
11. **Kong/Gorilla Island — recon** (SOLVED, 155→160; full puzzle DEFERRED, needs
    the costume). Map: OnShore(E=124,In=boat); 124 Boundary(E=33,W=shore; has a
    **sapling** = Sankora Paddy-tether later); 33 Forest`<1>`(N=32,W=124,NE=36);
    32 Forest`<2>`(S=33,NE=34); 34 Small Clearing (**dung** appears after the
    gorilla defecates; `cl_RollInDung`); 36 Forest`<3>`(N=34,E=37,SW=33);
    37 Large Clearing (~12 gorillas + **rocky outcrop**, Up=54); 54 On Top of
    Outcrop (the **glowing orange orb** `cl_Stone2` in a boulder-niche; Down=37).
    - **Departure gate**: can't leave Kong until `cl_ObjectSeen=1` — at **room 36**
      `look east through telescope` (see the outcrop/boulder/orb) then `x outcrop`
      (+5 `cl_SeeStoneSc`). THEN back to shore, `push off`, `row n` → **Pirate**.
    - REMAINING Kong (needs items from Pirate): wear **gorilla costume** +
      `roll in dung`(+5 smell) to pass the gorillas; reach outcrop 54;
      `take orb`(+5 `cl_TakeStone` — this **spawns the silverback at boundary 124**);
      at 124 `burst bag`(+5, inflated paper bag scares it off — the "loud noise",
      `cl_Gorillaisl10-12`; mirrors how Finn scared sister Polgara pre-dinner);
      give the **wooden horse** to a mother gorilla → **ornament** (`cl_GiveHorseS`
      +5). Then row on to Sankora with the orb.
12. **Pirate Island — town** (IN PROGRESS from here, 160). Arrival = Pier 2 (55).
    Town map: 55 Pier2(N=56); 56 Blubber Wharf(E=57,W=58); 57 East End Wharf
    (N=59,S=61 Pier3); 58 West End Wharf(In=62 Warehouse); 59 Fisherman's Lane
    (N=63); 61 Pier3 (**pull rope→box/metal case**, `cl_PullRope`+5,
    `cl_OpenBoxSco`); 62 Warehouse (**newspaper**, `cl_ReadNewspa`+5); 63 Yardarm
    St(E=67,W=64,In=81 Pie Place); 64 Yardarm St2(N=65,W=68,In=79 Bookshop);
    65 Mansion House Rd(N=69); 66 Mannbroom's Academy (**learn to fight**,
    `cl_LearnToFig1`; In from 68); 67 East End Yardarm(In=70 **Fancy Dress Shop**);
    68(W=72,In=66); 69 By Large Mansion(E=74); 70 Fancy Dress Shop (**hire gorilla
    costume, 120 gold**, `cl_HireCostum`+5); 72 Outside Davy Jones' Locker(In=73);
    73 **Tavern** (menu/book, food ordering, **key** `cl_PutKeyOnHo`,
    `cl_Attable*`); 74 Grand Drive(E=71 Portico,SE=75); 75 Path Around Mansion
    (E=77); 76/78 **The Kitchen** (napkin-in-oven `cl_PutNapkinI`, `cl_Kitchen21`
    "get the key"); 77 Back of Mansion(In=76); 79 **Bookshop** (`cl_BuyBookSco`);
    81 Pie Place. **Economy**: gold starts 18 (from Your Room); need **120** for
    the costume; **ring a bell** `cl_PullBellpu1` gives **+150 gold** (location
    TBD, likely mansion); Sankora uses **goons** (money-change gold→goons,
    `cl_ExchangeGo*` / `cl_MoneyChang` "don't exchange ALL your goons").

### Sections REMAINING to script

- **Pirate Island** (12 above): buy/hire costume (need 120 gold → ring the bell
  for +150), paper bag (from tavern food?), pull rope on Pier 3 → box, read
  newspaper, tavern food+book+key, kitchen napkin-in-oven, learn to fight (?).
- **Kong return** (11 above): costume+dung → outcrop → orb → burst-bag escape +
  give-horse ornament.
- **Sankora Island + endgame** (`cl_BlankHint*`): disguise past guards, temple
  offering (flowers `cl_PutFlowers`), money-change, feed the dog (leash/collar,
  `cl_FeedDogSco`/`cl_MakeLeash`), rice-hat via slingshot+stone (`cl_ThrowObjAt1`),
  farm/goat/dead-kid, herbalist (give dead **bat**+**scorpion** → cure powder),
  **temple orb swap** `cl_GetOrbInTe` (+25, `cl_StoneRepla=1`, Goons=500) →
  `cl_ToEndgame` (+25 → **`YouHaveWon`**). Win marker TBD (grep `YouHaveWon`).

## Method / tooling (reuse for continuation)

- Full model dump: `./test/a5dump "test/adrift5-games/FBA v.3c.blorb" > /tmp/fba_dump.txt`
  (123k lines). Extracted helpers already saved:
  - `/tmp/fba_wwdd.txt` — all 90 `[wwdd]` hint texts (the puzzle skeleton).
  - `/tmp/fba_score.txt` — 116 scoring tasks with their exact `<Command>` syntax
    and point values (the action backbone; grep for the verb you need).
  - `/tmp/fba_locs.txt` — 130+ locations (key → short name).
- Replay loop: keep the growing master at `/tmp/fba_wt.txt`; append candidate
  commands, run `a5run_dump`, and isolate the NEW output by `awk` from the unique
  final master command (currently `/^> throw fluff at web$/{go=1} go`). Filter
  `grep -vE "handfire|^$"` to drop the repeated handfire status line.
- To decode a puzzle: find its scoring task in `/tmp/fba_score.txt`, then
  `awk '/<Key>KEY<\/Key>/,/<\/Task>/'` on the dump for its exact command +
  restrictions (the gating variables tell you the prerequisite order).
- Extract any location's exits: the per-`<Location>` `<Direction>`/`<Destination>`
  pairs (see the perl one-liner used for the dungeon map).
- `ulimit -t 40` on every run (Horsfield games are event-heavy).

## When it finally wins

Wire per the standard playbook: `test/a5_groundtruth.sh` in both RNG modes,
add a `FinnsBigAdventure|FBA v.3c.blorb|v|x` MAP line, and (if stable) a golden.
FBA is a Horsfield game with likely RAND-independent conformance — expect it to
behave like the other native-solution wins (LostLabyrinth / TheEuripidesEnigma).
