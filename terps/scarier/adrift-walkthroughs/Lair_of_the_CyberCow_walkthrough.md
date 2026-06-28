# Lair of the CyberCow — walkthrough

- **Author:** Conrad Cook, *Lair of the CyberCow*, copyright 2008, written in
  ADRIFT 3.9 for **IF Comp 2008**. No published walkthrough on Key & Compass,
  IF Archive, or CASA.
- **Engine:** ADRIFT 3.90 data. The Battle System is **not** used for any
  reachable point (no combat-assist involved).
- **Result: the game is fully winnable in SCARE.** It reaches its proper
  `EndGame` ending — **task 175, the "fairy kiss"** (`*** The End ***`). The
  game's own in-game `walkthrough` command now plays through to that ending; an
  earlier draft of this file wrongly concluded the win was unreachable, but that
  was only because of the three interpreter bugs fixed below.
- **Two routes are documented:**
  1. **The win** — the in-game `walkthrough` route (`put fairy into robot →
     uncle → ... → give berry to fairy`). Solution:
     `harness/cybercow_win_solution.txt`.
  2. **Max display score (10/10)** — a deterministic route that banks every one
     of the game's display points and reaches the climactic **Robot-vs-CyberCow
     confrontation** set-piece (but stops short of the fairy-kiss ending).
     Solution: `harness/cybercow_solution.txt`.
- Both solution files begin with the name/gender start-up prompts: `Hero`,
  `male`.
- **Caveat:** Vluurinik (the fairy) moves on a partly random walk, so the exact
  number of `z` waits needed to lure and catch her can vary from run to run. The
  headless harness is deterministic (fixed RNG seed), so the checked-in command
  lists are reproducible there; in normal play, wait a few extra turns if she
  hasn't taken the bait yet.

## The map

```
                Clinging to the Steeple (0)
                        | out/in
                Top of Bell Tower (6)
                        | u/d
   Meadow (10) — Chapel Yard (5) — Living Room (2)
        w          |  n              | d  (after PUSH COUCH)
                Bus Stop (23)     Cellar (12)  [robot table]
                   |  s
                In the Chapel (8) — e — Bottom of Bell Tower (7) — in — In the Bell (11)
                   |  s
                The Altar (9)

   Chapel Yard —d→ Bottom of the Well (1)  [CyberCow — blocks "west" to the lair]
```

The cottage's **Living Room** hides a trapdoor to the **Cellar** (push the
couch). The **Bell Tower** holds the bell (and the robot **plans** taped inside
it). The **Well** (reached after you rig the rope) is home to the **CyberCow**.
The fairy **Vluurinik** flies a fixed loop through the **Steeple → Meadow →
Chapel Yard**.

## Route 1 — the win (fairy-kiss ending)

This is the game's own in-game `walkthrough` route (type `walkthrough` in the
game to see it). It builds the robot, powers it with the **live fairy**,
surrenders to it (`uncle`), is led down to the lair, and ends in the floating
bell. See `harness/cybercow_win_solution.txt` for the exact command list.

Phases:

1. **Cottage + bell tower (same as the 10/10 route, steps 1–3 below):** open the
   cupboard, take the **bowl** and **flashlight**, `push couch`, fetch the
   **snail** in the Meadow, rig the **well rope**, and get the **robot plans**
   from inside the bell (`flip bell with cross`, `take packet`, `unfold packet`).
2. **Build the robot and milk the cow:** in the Cellar, `complete robot`
   (the in-game text calls it `fix robot`). Down the well, `give snail to cow`
   then `milk cow` to fill the bowl.
3. **Bait and catch the fairy:** carry the milk to a room on Vluurinik's loop
   (the steeple or the **chapel yard** both work — see the walk-meet engine fix),
   `drop` the bowl, wait until she snatches it, then `catch Vluurinik`
   (`catch fairy`).
4. **Power the robot with the live fairy and surrender:** back in the Cellar,
   `put fairy into robot`. The robot wakes up, grabs you, and interrogates you;
   answer `uncle`. After a few turns it gives up — **"The Robot releases you
   with a mean little shove"** — and the cellar `up` exit, which `put fairy in
   robot` had sealed, **reopens.**
5. **Lead the robot to the lair:** go `up`, `w`, `d`. The robot **follows you**
   ("The Robot marches in"); at the **Bottom of the Well** it finally meets the
   CyberCow ("The Robot and the CyberCow lock eyes") and the way `w` into the
   lair opens.
6. **Lair → floating bell → the kiss:** `take steak` triggers the flood; ride it
   up, `get in bell`, and when the fairy reappears, `take berry` and
   `give berry to fairy`. *"…in the only place she considered safe, she kissed
   you."* — **`*** The End ***`.**

## Route 2 — the deterministic 10/10 max-score command list

This route banks all ten display points and ends at the Robot-vs-CyberCow
confrontation set-piece (it does **not** reach the fairy-kiss ending — it powers
the robot with **bones**, not the live fairy).

```
Hero
male
s              (Chapel Yard)
e              (Living Room)
open cupboard
take bowl
take flashlight
push couch     +1  reveals the cellar trapdoor
w
w              (Meadow)
take snail
e
s              (In the Chapel)
e              (Bottom of Bell Tower)
u              (Top of Bell Tower)
untie rope
out            (Clinging to the Steeple)
push cross     +1  the cross tumbles down to the Chapel Yard
in
d              (Bottom of Bell Tower)
take rope
w
n              (Chapel Yard)
tie rope to well
throw rope down well   +1
take cross
s
e              (Bottom of Bell Tower)
flip bell with cross   +1  slides the cross under the bell, opening it
hit bell with cross
in             (In the Bell)
take packet
out
unfold packet  -> robot construction plans
w
n
e              (Living Room)
d              (Cellar)
complete robot +1  (uses the plans)
u
w
d              (Bottom of the Well — the CyberCow is here)
feed snail to cybercow
milk cybercow  +1  fills the bowl with milk
u
s
e
u
out            (Clinging to the Steeple — the fairy's home)
drop milk
z
z
z
z              wait: Vluurinik flies in, snatches the milk bowl (+1 "baited"),
                     and makes off toward the meadow
in
d
w
n
w              (Meadow)
catch Vluurinik       the baited fairy can now be caught
eat fairy             -> bones (+ wings)
e
e
d              (Cellar)
put bones in robot     +2  the bones power the robot; it comes alive: "-DEATH-"
up
w
d              +1  fleeing into the Chapel Yard triggers the Robot-vs-CyberCow
                   confrontation; the berserk Robot turns on the CyberCow
score          -> 10 out of 10
```

### Phase notes (route 2)

1. **Opening / cottage (push couch +1).** From the Bus Stop go `s` to the Chapel
   Yard hub. `e` into the Living Room: open the cupboard and take the **bowl**
   (needed to milk the cow) and the **flashlight**. `push couch` reveals the
   cellar trapdoor (+1).
2. **Meadow.** `w`,`w` to the Meadow; `take snail` (the CyberCow's favourite
   food).
3. **Bell tower (push cross +1, well rope +1, flip bell +1).** `s`,`e`,`u` to the
   top of the tower. `untie rope`, then `out` onto the steeple and `push cross` —
   the heavy cross tumbles down into the Chapel Yard (+1). Back `in`,`d`,
   `take rope`, then to the Chapel Yard (`w`,`n`) and `tie rope to well` +
   `throw rope down well` (+1) to make the well climbable. `take cross`, then
   `s`,`e` and **`flip bell with cross`** (+1) — wedging the cross under the bell
   rolls it open. `hit bell with cross`, `in`, `take packet`, `out`,
   `unfold packet` → the **invincible-robot plans**.
4. **Cellar robot (complete robot +1).** From the bell tower, `w`,`n`,`e`,`d`
   into the Cellar and `complete robot` (+1). (The flashlight is a red herring —
   it "won't work as a power source.")
5. **CyberCow (milk +1).** `u`,`w`,`d` down the well to the CyberCow.
   `feed snail to cybercow`, then `milk cybercow` (+1) fills the bowl.
6. **Catch the fairy (baited +1).** Carry the milk to a room on Vluurinik's loop
   (here the **steeple**: `u`,`s`,`e`,`u`,`out`). `drop milk` and wait: Vluurinik
   flies in, **snatches the bowl (+1)** and bolts for the meadow. Chase it down
   (`in`,`d`,`w`,`n`,`w`) and `catch Vluurinik`.
7. **Bones robot (+2) and the confrontation (+1).** `eat fairy` turns the caught
   fairy into **bones**. Back to the Cellar (`e`,`e`,`d`) and `put bones in
   robot` (+2) — the bones power it and it wakes up berserk ("-DEATH-").
   **Flee immediately** (`up`,`w`): the berserk Robot pursues you up into the
   Chapel Yard. Going `d` there (+1) pits the Robot against the CyberCow; the
   Robot fries the CyberCow, which **explodes in a flood of milk** that knocks
   you both flat. **Score 10/10.**

   *Do **not** linger near the berserk Robot and do **not** `push` it — it kills
   you with its ray ("a blinding, searing ray of light — You have died").*

## Engine fixes (all confirmed against the ADRIFT 3.9 Runner P-code)

Three interpreter bugs blocked this game; all three are engine-level (they ship
in Spatterlight, not just the harness) and leave the bundled-walkthrough corpus
output byte-identical.

### 1. Walk `MeetObject` dynamic→global index (the fairy was uncatchable)

Catching Vluurinik depends on the fairy's **walk ObjectTask**: when her fixed
walk steps into a room that holds the milk bowl, it runs a `#lured` task. A
walk's `MeetObject` is stored in the TAF as a **1-based dynamic-object index**,
but the runtime needs a **global** object index. The parser performs this
conversion (the `|V380_WALK:_MeetObject_|` fixup) **only for version 3.8 games**;
the 3.9/4.0 WALK schemas read it raw. `npc_tick_npc_walk()` then used that raw
dynamic index as a global one — so `MeetObject` (the milk bowl) pointed at the
wrong object, the lure never fired, and the fairy could never be caught.

**Fix (`scnpcs.c`):** convert `MeetObject` from dynamic to global at run time for
any game newer than 3.8 (`npc_walk_meetobject_needs_fixup()` +
`obj_dynamic_object()`), matching the 3.8 parser.

### 2. Walk-met tasks are dispatched by command, honouring "Where"

When a walking character meets an object/character, the Runner does **not** run
the single task stored in the walk; it copies that task's **command text** and
runs it through the normal task matcher, so every task sharing that command is a
candidate and the one whose "Where" room list matches the **player's** room
(restrictions permitting) is what fires. CyberCow has **two `#lured` tasks** —
one for the steeple, one for the chapel yard — so the fairy steals the milk bowl
in **either** room. Running only the literal walk task fired the steeple variant
only; dropping the milk in the chapel yard did nothing.

A brief earlier attempt instead made walk-met tasks *skip* the "Where" check.
That is wrong (and broke the robot's confrontation, firing it the instant the
robot was built): `Form1.characters` runs the met task through `Form1.tasks()`,
which **does** check "Where". **Fix:** `run_npc_walk_task()` in `scrunner.c`
dispatches the met task by command across same-command siblings, gated by "Where"
+ restrictions; the three walk-meet sites in `scnpcs.c` call it.

### 3. An event that marks a task incomplete clears the flag (doesn't reverse)

After `uncle`, the **"De-Uncle 2 Freedom"** event clears the `put fairy in
robot` task so the cellar's `up` exit (gated on that task being *not* done)
reopens. SCARE implemented an event's "set task incomplete" as a task **reverse**,
which it gates on the task's `Reversible` flag — and `put fairy in robot` is not
reversible, so the event silently did nothing and the player stayed **trapped in
the cellar**. The Runner's `checkevent` just **clears the task's completed flag
directly** (no reverse, no `Reversible`/`Where` check). **Fix (`scevents.c`):**
when an event marks a task finished/incomplete, call `gs_set_task_done(…, FALSE)`
instead of running the task backwards.

### Bonus: `take all` ignored open containers / surfaces

A separate bug surfaced here: after `open cupboard` (holding the bowl and
flashlight), the Runner answers `take all` with *"You take the bowl and the
flashlight from the cupboard."* but SCARE said *"There is nothing to pick up
here."* The "take all" frontends used `lib_take_not_associated_filter`, which
unconditionally excludes any object inside/on another object. **Fix
(`sclibrar.c`):** both frontends now use `lib_take_filter`
(`obj_indirectly_in_room`, which recurses through *open* containers/surfaces).

## How the win was unblocked

The win is **task 175 `#end game`** (the fairy kiss): in the floating bell the
fairy reappears, you `take berry` and `give berry to fairy`. The flood that
floats the bell starts from `take steak`/`turn on television` deep in the
**lair**, reached by `d` from the Bottom of the Well (**task 98**, which needs
`_permaUncle`). `_permaUncle` is set by saying `uncle` to the fairy-powered
robot — but `put fairy in robot` seals the cellar, so the only way `uncle` is
ever useful is if the cellar then **reopens**. The "De-Uncle 2 Freedom" event
does exactly that, and fix #3 above is what makes it work; fixes #1 and #2 let
you catch the fairy and bait it in the natural rooms. With the cellar reopening,
you lead the robot to the well, the lair opens, and the fairy-kiss ending is
reached — exactly as the game's own `walkthrough` command intends.
