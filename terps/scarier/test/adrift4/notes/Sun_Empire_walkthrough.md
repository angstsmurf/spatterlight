# Sun Empire: Quest for the Founders (Part I) — Walkthrough

ADRIFT game by Daniel Hiebert (2003), winner of the ADRIFT Summer Comp 2003.
Derived by playing the game to completion under SCARE (deterministic seed) and
cross-checked against the game's internal scoring table.

**Result: 140 / 145 points** and the winning ending ("Congratulations!").
The single remaining point is **unobtainable in the shipped game** — a logic bug
in the game's own task data (not a SCARE bug), documented at the end.

You play Tangee Simone, chief engineer at a genetics institute. After a routine
DNA analysis, the lab is overrun by hostile Orgaan soldiers. The optimal run
collects every reachable DNA sample, rescues the security captain, secures the
research data, and escapes to the evac bus.

---

## Full command list (135 points, start to finish)

```
use print pod              (+5)
e
d
n
e
s
open door
sw
open trunk
take id pass
take clothes
ne
n
w
z                          (wait ~5 turns for Jeriah to wander into the lab)
z
z
z
z
give id pass to jeriah     (+10, Jeriah gives you his pdd)
x pdd                      (+5)
take genetic sampler
get sample from me         (+5)
get sample from limb       (+5)
perform dna analysis
e
n
e
take grenade               (+5, lifted off the distracted Malthew)
w
s
w
s
u
w
talk to mark
ask mark to follow me      (+10, triggers Code Red / the Orgaan attack)
e
d
n
z                          (wait ~10-12 turns: breach +5, Mark kills Skyrv +5 & Skynd +5)
z
z
z
z
z
z
z
z
z
z
z
take rifle
get sample from skyrv      (+5)
get sample from orgaan soldier   (+5)
get sample from guard      (+5)
get sample from mark       (+5)
s
get sample from jeriah     (+5)
get sample from orgaan soldier   (+5)
u
w
answer vidcom              (+10)
open drawer
take data chip
download data              (+10, copies research to the chip and wipes the system)
w
throw grenade              (+10, clears the dozen Orgaans blocking the hall)
n
e
get sample from nadine     (+5, scores despite the "can't sample Antonai" text)
e
talk to malthew            (+10, the wounded captain joins you for the bus)
get sample from malthew    (+5)
w
w
s
shoot windows              (rifle shatters the plasteel to the gardens)
w                          (escape -> marines evac -> WIN)
```

---

## Walkthrough, explained

### Phase 1 — Explore the institute *before* the attack
Once the attack starts, security seals all the doors, so all the side content
must be done now.

1. `use print pod` — wave your hand over the pod to open the Control Room door
   (**+5**). (`open door` does *not* work.)
2. Go `e, d, n` to the **Genetics Lab**, then `e, s` to the **PQ Intersection**.
3. `open door`, `sw` into **Jeriah's PQ**; `open trunk`, `take id pass`,
   `take clothes`. (Jeriah lost his ID pass and wants it back.)
4. Return `ne, n, w` to the **Genetics Lab**. Jeriah wanders; wait (`z`) about
   five turns until he enters, then `give id pass to jeriah` (**+10**). He
   thanks you with his **pdd** (his DNA-stimulation research).
5. `x pdd` (**+5**) — reading his research counts.
6. `take genetic sampler` from the lab table, then `get sample from me`
   (**+5** — sample your own DNA) and `get sample from limb` (**+5** — the
   founder limb in the growth vat).
7. `perform dna analysis` at the big wall computer. ⚠️ Use **perform** (or
   *initiate*) — *run* is grabbed by the movement parser.
8. Detour for the grenade: `e, n, e` to **Security**, `take grenade` (**+5** —
   Malthew is distracted and never notices), then `w, s, w` back to the lab.
9. `s, u, w` to the **Control Room**; `talk to mark`, then
   `ask mark to follow me` (**+10**). This triggers **Code Red** and the attack.

### Phase 2 — Survive the Orgaan breach
10. `e, d, n` back to the **Genetics Lab** and **wait** (`z`). The Orgaans blow
    the doors (**+5**), and Skyrv & Skynd attack **Mark**. Their hits can't hurt
    you (your Defense out-rolls their Strength), so just keep waiting — Mark
    kills Skynd (**+5**) and Skyrv (**+5**). ~10–12 `z`'s clears it.
11. `take rifle` (dropped by the dead guard), then sample the bodies:
    `get sample from skyrv`, `get sample from orgaan soldier`,
    `get sample from guard`, `get sample from mark` (**+5 each**).
12. `s` to the **Stairwell**, where Jeriah and an Orgaan now lie dead:
    `get sample from jeriah`, `get sample from orgaan soldier` (**+5 each**).

### Phase 3 — Secure the data and escape
13. `u, w` to the **Control Room**. `answer vidcom` (**+10**) — security relays
    the evac plan (and this unlocks the data download).
14. `open drawer`, `take data chip`, `download data` (**+10**) — the data is
    copied to the chip and erased from the system.
15. `w` across the fallen I-beam to the **Cafeteria**. `throw grenade` (**+10**)
    — roll it down the hall to wipe out the dozen entrenched Orgaans. (Shooting
    them or charging in just gets you killed; the grenade is the only way.)
16. `n, e` to **Reception**; `get sample from nadine` (**+5** — it scores even
    though the game says "you cannot obtain a genetic sample off of Antonai";
    that line is flavor). Then `e` to **Security**. `talk to malthew` (**+10**)
    — the wounded captain agrees to come; `get sample from malthew` (**+5**).
17. Head back `w, w, s` to the **Cafeteria**, `shoot windows` to break out to the
    gardens, and `w` to escape. The marines cut down your pursuers and load you
    onto the bus. **You win.** 🎉

---

## The one unobtainable point (a game-data bug, not a SCARE bug)

The game's internal scoring table totals exactly 145. Exactly one of the 24
scoring tasks can never fire in the shipped data:

- **Sample Skynd (+5).** The Skyrv and Skynd sample tasks are written almost
  identically. Each combines two checks — "the live NPC is in the room" (true
  only *before* death) and "the corpse object is in the room" (true only *after*
  death) — so the two checks can never both hold at once. Skyrv's task joins
  them with **OR**, so it succeeds (one or the other is always true). Skynd's
  task joins them with **AND**, making it logically unsatisfiable. This is an
  author mistake in the task's restriction expression (`OR` typed as `AND`).

  It is **not** a SCARE bug: the AND/OR operators are stored in the game's `.taf`
  data, and both SCARE and the original ADRIFT Runner evaluate the same bracket
  expression (the Runner's `evaluaterestrictions` routine; SCARE's tokenizer in
  `screstrs.c`). Skynd is therefore unreachable in the original Runner too.

So **140/145 is the maximum** obtainable in any faithful interpreter; the final
point is lost to a bug in the game itself.

> Note: "Sample Nadine" (+5) *does* score, even though the game prints
> "you cannot obtain a genetic sample off of Antonai." That line is flavor —
> the sample task still completes (its restriction is an OR with a branch that
> passes), so the point is awarded. The route above includes it.
