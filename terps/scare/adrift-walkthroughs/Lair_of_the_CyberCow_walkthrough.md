# Lair of the CyberCow — walkthrough

- **Author:** Conrad Cook, *Lair of the CyberCow*, copyright 2008, written in
  ADRIFT 3.9 for **IF Comp 2008**. No published walkthrough on Key & Compass,
  IF Archive, or CASA. (The game ships an in-game `walkthrough` command, but its
  text is an inaccurate draft — it uses verbs the game does not implement, e.g.
  `fix robot` for `complete robot`, `catch fairy` for `catch Vluurinik`,
  `milk cow` etc. — and its routing does not actually win.)
- **Engine:** ADRIFT 3.90 data. The Battle System is **not** used for any
  reachable point (no combat-assist involved).
- **Result:** **Max score reached: 10/10 (100%), deterministic.** The play-through
  below banks every one of the game's display points and reaches the climactic
  **Robot-vs-CyberCow confrontation**, where the CyberCow is destroyed.
- **The separate "fairy kiss" END-GAME ending (`#end game`, task 175) does NOT
  appear to be reachable in SCARE through any path found — see the closing
  analysis.** The 10/10 route is the furthest reachable, fully-scored state.
- Solution file: `harness/cybercow_solution.txt` (first two lines are the
  name/gender start-up prompts: `Hero`, `male`).
- **Required engine fix:** the fairy *Vluurinik* was **uncatchable** in SCARE
  until a real interpreter bug was fixed (walk `MeetObject` dynamic-index
  conversion, missing for 3.9/4.0 games). Without it the game caps at 6/10. See
  the "Engine fix" note below.

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

## The deterministic 10/10 command list

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

## Phase notes

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
6. **Catch the fairy (baited +1).** Carry the milk up to the **steeple** (`u`,`s`,
   `e`,`u`,`out`) — this is where Vluurinik's flight loop passes a room you can
   stand in. `drop milk` and wait: Vluurinik flies in, **snatches the bowl
   (+1)** and bolts for the meadow. Chase it down (`in`,`d`,`w`,`n`,`w`) and
   `catch Vluurinik`.
7. **Bones robot (+2) and the confrontation (+1).** `eat fairy` turns the caught
   fairy into **bones**. Back to the Cellar (`e`,`e`,`d`) and `put bones in
   robot` (+2) — the bones power it and it wakes up berserk ("-DEATH-").
   **Flee immediately** (`up`,`w`): the berserk Robot pursues you up into the
   Chapel Yard. Going `d` there (+1) pits the Robot against the CyberCow; the
   Robot fries the CyberCow, which **explodes in a flood of milk** that knocks
   you both flat. **Score 10/10.**

   *Do **not** linger near the berserk Robot and do **not** `push` it — it kills
   you with its ray ("a blinding, searing ray of light — You have died").*

## Engine fix required (the fairy was uncatchable)

Catching Vluurinik depends on the fairy's **walk ObjectTask**: when the fairy's
fixed walk steps into a room that holds the milk bowl, it runs task 76
(`#lured`). A walk's `MeetObject` is stored in the TAF as a **1-based dynamic-
object index**, and the runtime needs a **global** object index. The parser
performs this dynamic→global conversion (the `|V380_WALK:_MeetObject_|` fixup)
**only for version 3.8 games**; the 3.9 and 4.0 WALK schemas read it raw.
`npc_tick_npc_walk()` then used that raw dynamic index as if it were a global
index — so for this 3.9 game `MeetObject` (the milk bowl, dynamic index 17,
global object 28) was read as global object 17 (`_sleeping`), which is never in
any room, so the lure **never fired** and the fairy could never be caught.
This caps the game at 6/10.

**Fix (`scnpcs.c`):** convert `MeetObject` from a dynamic to a global index at
run time for any game newer than 3.8 (`npc_walk_meetobject_needs_fixup()` +
`obj_dynamic_object()`), matching what the 3.8 parser already does. Engine-level
(applies to Spatterlight, not just the harness); the existing winnable corpus
games are unaffected (they have no object-meet walks).

## Second engine fix: `take all` ignored open containers / surfaces

A separate SCARE bug surfaced in this room: after `open cupboard` (which holds
the bowl and flashlight), the ADRIFT runner answers `take all` with *"You take
the bowl and the flashlight from the cupboard."* — but SCARE said *"There is
nothing to pick up here."* `lib_cmd_take_all` / `lib_cmd_take_except_multiple`
used `lib_take_not_associated_filter`, which **unconditionally excludes** any
object that is inside or on another object — so contents of an open container
(or items on a surface) present in the room were never picked up. The base
`lib_take_filter` already does the right thing: it uses `obj_indirectly_in_room`,
which recurses only through *open* containers/surfaces, so it includes
open-container/surface contents while still excluding the contents of *closed*
containers. **Fix (`sclibrar.c`):** both "take all" frontends now use
`lib_take_filter` (the over-restrictive `lib_take_not_associated_filter` is
retired). Verified the two banked corpus wins that use `take all`
(SecretOfLostWorld, X-Files) still complete.

## Why the "fairy kiss" ending is not reachable (analysis)

The game's only `EndGame`/win action is **task 175 `#end game`** (the "fairy
kiss"), reached by a flood finale: in the floating bell (room 32) the fairy
appears, you `take berry` and `give berry to fairy`, and a kiss ends the game.
That flood only starts from **`#fairy appears` (task 169)**, which fires from
**`take steak` (task 96)** or **`turn on television` (task 106)** — both deep
inside the **lair** (rooms 28/30). Reaching the lair requires entering the
"Bottom of the Well" confrontation cluster (rooms 25–30), whose three entrances
are all blocked in SCARE:

- **`d` from the well — task 98 — needs `_permaUncle`.** `_permaUncle` is set by
  saying `uncle` (task 37) to the **psycho (fairy-powered) Robot**. But powering
  the robot with the *live fairy* (`put fairy in robot`, task 82) **seals the
  cellar**: the cellar's only exit (`up`) is gated on task 82 being *not* done,
  so once the fairy is installed you are trapped in the cellar with the
  interrogating robot. `_permaUncle` can therefore only ever be obtained while
  sealed in the cellar, with no way to reach the well to use it.
- **`d` from the Chapel Yard — task 117 — needs `put bones in robot` done.** This
  is the route the 10/10 list uses, but its move action relocates the **CyberCow**
  into the arena (for the confrontation set-piece), not the player; the player
  stays in the Chapel Yard and is then washed back out after the milk explosion.
  It never delivers the player into the arena.
- **`d` from the Chapel Yard — task 112 — needs the "dead fairy" (object 41)
  held.** Object 41 is recovered (task 5) only from the Robot's **hulk** — i.e.
  after the Robot has been *killed* with the cross (task 147). But the Robot can
  never be brought to that state: the fairy-powered robot is locked in the
  sealed cellar, and the bones-powered robot is berserk and lethal — `push robot`
  (the move that begins the bell-tower "fall" that makes it vulnerable) is an
  instant death.

The CyberCow that blocks the lair entrance (`west` from the Bottom of the Well)
is never removed — it still blocks even after the confrontation destroys "a"
CyberCow in the set-piece. With the lair unreachable, `#fairy appears` never
fires, the flood never starts, and **task 175 (the win) is unreachable**.

This is consistent with the in-game `walkthrough` text being a non-working
draft (it routes through `put fairy into robot → say uncle → up`, but the
cellar is sealed the instant the fairy goes in). Whether the deadlock is
faithful to the original ADRIFT Runner (an as-shipped broken comp game) or a
further SCARE divergence on top of the `MeetObject` bug was not resolved; the
`MeetObject` fix is independently correct and is what raises the reachable
score from 6 to the full 10.
