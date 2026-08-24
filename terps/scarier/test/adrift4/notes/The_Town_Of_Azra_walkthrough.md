# The Town of Azra — Walkthrough

ADRIFT game by Stan Tencza. An explicitly **unfinished** RPG-sandbox ("This
game is still under development… The game is nowhere near completion."). Derived
by playing under SCARE (deterministic seed) and cross-checked against the game's
internal task, exit, and **Battle System** data.

There is **no score and no winning ending** in either build (`score` reports a
maximum of **0/0**; the file contains no `ACT type=6` at all). "Completing"
Azra means working through the goals the author lists in the intro — and
**one of the six is not actually reachable**, on either build (see
"Goal 5 was never possible" below):

1. Kill a bandit, and get his money
2. Kill a deer, take the carcass and sell it to Drako
3. Make a few purchases from Harthro's, Calado's, Golapho's
4. Stay at Gralle's Inn
5. Purchase a house ← **not reachable; see below**
6. Learn Stealth Tactics

You are **Pindarus**, a man in the Roman-ish town of Azra, starting with **$500**
and the clothes on your back.

---

## ⚠️ Azra ships as two files, and they are not the same game to play

| corpus file | signature | sha256 (first 16) | goals reachable |
|---|---|---|---|
| `The_Town_Of_Azra.taf` (IF Archive) | **4.00** (`…c2 cf 93 45 3e 61`) | `e40ec150e1531f49` | **3 and 4 only** |
| `The Town Of Azra.taf` (adrift.co) | **3.90** (`…c2 cf 94 45 37 61`) | `14684c1d2cd8b929` | **1, 2, 3, 4, 6** |

The underscored file is a **4.00 upconversion of the author's 3.90 original**,
made by opening it in the ADRIFT 4 editor. The upgrade left every battle
attribute range degenerate (`Lo == Hi`) with Accuracy and Agility at 0, and the
4.0 Battle System resolves a blow with the strict test `accuracy > agility`.
`0 > 0` is false, so **no attack by anyone can ever land** — you and the bandit
"manage to avoid" each other forever. Combat is Azra's only income, so that
build is walled off from goals 1, 2, 5 and 6. This is faithful, not a SCARE
divergence: the same stalemate was verified live in `run400.exe` under Wine
(see the `adrift-combat-zero-accuracy-stalemate` memo).

The spaced adrift.co file is the **untouched 3.90 original**, and
`battle_is_legacy_version()` recognises it, so SCARE uses `battle_legacy`: the
pre-4.0 model has no Accuracy/Agility gate at all, every blow connects, and
damage is simply `strength − defence`. That is what the author designed against
and it makes the whole game playable. **The ADRIFT 4 editor's upgrade silently
destroyed Stan Tencza's game**, and we have both files to prove it.

Two committed rows follow the split:

| row | game | reaches |
|---|---|---|
| `goldens/the_town_of_azra_solution.txt` | `The_Town_Of_Azra.taf` (4.00) | goals 3 + 4, 27 turns |
| `goldens/the_town_of_azra_v390_solution.txt` | `The Town Of Azra.taf` (3.90) | goals 1, 2, 3, 4, 6 — everything but the house, 58 turns |

`SCR_ASSUME_COMBAT=1` — SCARE's opt-in assist for games whose author left
Accuracy at 0 — is no longer needed for anything here. It was the old way to
see the combat goals on the 4.00 file; the 3.90 file gets the author's intended
combat with no flag at all.

---

## Town map

```
        Northern Forest (9)          [deer]
              |  S
        Bandit Trail (3)             [bandit]   <- <waitkey> on the city wall
              |  S
        Northern Trail (6)
              |  S
  Golapho(2)-W- Busy Street (1) -E- Harthro's Tavern (0)
              |  S                  [Ormulus, Kiris in the street]
        Poel's Plaza (7) -SE- Gralle's Inn (4) -in- rented room (5)
         /  |  \
       W    E   S
   Outside  Residential(12)   Calado's
   Stores(10)  |  in           Metal Works(8)
      |S       Your House(13) -u- Bedroom(14) / -in- Dining(15)   [needs $7500]
   Butcher(11)
   [Drako]
```

Rooms 16/17/18 (In Front of the Town Council Hall, its Main Door, and the Law
Building with Pahlidro) have **no inbound exit from rooms 0–15 and no task that
moves the player there** — task 58 `enter *** town council hall` is itself
`where = room 17`, i.e. only typeable once you are already inside. They are
orphaned content, consistent with the author's own disclaimer. Neither build
can reach them.

---

## The 4.00 build: goals 3 and 4, and that is the ceiling

```
                           (press a key past the 3 intro screens)
e                          (Busy Street -> Harthro's Golden Tavern)
change a dollar into cents (the tavern sells in cents; you start with $0 cents)
buy a coffee               (Goal 3a: $1.50)
take the mug of coffee     (the mug stays on the bar)
gulp the coffee
sip the coffee
buy a steak                (Goal 3a: $8)
take a bite out of the steak
w                          (-> Busy Street)
w                          (-> Golapho's Armor)
buy a bronze helmet        (Goal 3b: $24)
buy rawhide armor          (Goal 3b: $90)
wear the rawhide armor
e                          (-> Busy Street)
s                          (-> Poel's Plaza)
s                          (-> Calado's Metal Works)
buy a hunting sword        (Goal 3c: $80)
wield the hunting sword
n                          (-> Poel's Plaza)
se                         (-> Gralle's Inn)
rent a room                (Goal 4: $50)
in                         (-> your rented room)
lie on the bed
nap on the bed             (you take a nap and "feel a little better")
out                        (-> Gralle's Inn)
nw                         (-> Poel's Plaza)
stats
wealth                     (ends at $246 and 50 cents)
```

All three shops (**Goal 3**) and a night at Gralle's Inn (**Goal 4**),
deterministically, leaving $246.50 and nowhere left to spend it.

---

## ⚠️ Goal 5 was never possible: battle death is permanent

Azra is designed as a renewable hunting sandbox. Task 19 `#banditkristdies`
and task 37 `#deerdies` — the Bandit's and the Deer's **KilledTask**s — each
drop a corpse object into the forest, move their NPC to *hidden*, and hand it a
full stamina restore (+30 / +20). That is unmistakably "and now it walks back
in, alive again": both NPCs have a looping walk whose first step is `dest=0`
(hidden → the trail). Sell fifteen carcasses at $500 apiece and the $7,500
house is yours; the author even says so in the intro ("I'm not saying that you
can't continue to kill more bandits and sell more carcasses to gain more money,
of course. :)").

**The real ADRIFT Runner never did that.** `killchar` (run390 `@42D410`,
run400 `@44B13C`) runs the KilledTask and then, unconditionally and *after* it,
stamps `&HFB` — that is **−5**, a `LitI2_Byte` sign-extended, not 251 — into
the NPC's room field. Exactly two places read it:

* the **walk ticker** breaks out of the walk loop for any NPC at −5
  (run390 `loc_45A4BC → loc_45ABD0`; run400 `loc_4685B6 → loc_468D61`), so the
  KilledTask's move-to-hidden is the last thing that NPC's walk ever does; and
* **`where <name>`** answers "<Name> is dead!" (run390 `loc_459D60`).

Campbell Wild's own ADRIFT 4 manual says the same thing in prose (l. 2659):

> The default behaviour for when a character is killed (i.e. its stamina
> reaches zero) is for the character to disappear, and any objects it was
> holding are moved to the current room. Typically you would want to create a
> dead body and have some message notifying the player of the recently
> deceased.

So there is **one bandit and one deer, once**, and the author's remark is
untested. Azra's economy is a *game* bug. Scarier used to let a corpse keep
walking, which is why the pre-2026-08-24 golden here ran 505 turns and sold
fifteen carcasses; that route was bought with an engine divergence and has been
re-derived.

### The money ceiling

| step | wealth |
|---|---|
| start | $500.00 |
| after the three shops and the inn (goal 3 + 4) | $246.50 |
| `search for money` on the bandit's body (goal 1) | $459.68 |
| carcass sold to Drako (goal 2) | $959.68 |
| `learn stealth from ormulus` −$800 (goal 6) | $159.68 |

The purse is random per bandit ($193, $213 and $432 observed across seeds), so
even the luckiest single kill lands near $1,200 — against a **$7,500** house.
**Goal 5 is unreachable.**

---

## The 3.90 build: goals 1, 2, 3, 4 and 6

`goldens/the_town_of_azra_v390_solution.txt`, **58 turns**, `SCR_SEED=26
SCR_SKIP_WAITKEY=1` (the city wall north of Northern Trail prints "Press any
key to continue!" every time you cross it). The route is the 4.00 block above —
minus the copper armour, which is now pure spend — then:

### 1. Goal 1 — the bandit on the Bandit Trail

```
wear the bronze helmet     (the 4.00 route buys it and never puts it on)
n / n / n                  (Busy Street -> Northern Trail -> Bandit Trail)
attack bandit  ... x10     ("You kill the vile bandit!")
search for money           ("You now have $459 and 68 cents.")
```

Damage is `strength − defence`, applied only when positive. The bandit's
strength is **5**; rawhide armour (2) + bronze helmet (1) puts defence at
**3**, so he takes 2 a turn off you — 16 stamina over the fight, against a
ceiling of 102. The old route's $180 copper armour pushed defence to 7 and made
the grind free, which mattered when the grind was fifteen kills long; with one
kill it is not worth the money.

Ten attacks is the measured minimum at this seed. **Overshooting is free**: an
`attack` aimed at an absent or dead NPC is a *parser rejection*, not a failed
action, so it costs no turn and nothing downstream shifts (11–15 attacks give a
byte-identical transcript). That makes the attack blocks self-syncing and safe
to re-derive.

### 2. Goal 2 — the deer, and the one carcass

```
n                          (-> Northern Forest; the deer is already there)
attack deer  ... x4        ("You strike the deer one last and final time,
                            and it falls dead!")
take the deer carcass
s / s / s / s / w / s      (Forest -> Bandit Trail -> ... -> Butcher Shop)
show the deer carcass to Drako
sell the deer carcass to Drako for 500 dollars
```

Four chops with the hunting sword. The Deer's patrol puts it in the forest for
roughly 5–8 turns in every ~16, and at `SCR_SEED=26` the route arrives on a
turn when it is present — hence the fixed seed on the row. Drako pays a flat
**$500** whatever number you type in the `sell` command (task 39's action is a
hard `dollars += 500`).

Stealth is **not** needed to hunt. Task 33 `#deerrunsoff` is the Deer's
walk-completion task and only fires when the walk step ends, which never comes
round while you keep swinging.

### 3. Goal 6 — Stealth Tactics

```
learn stealth from ormulus  ($800; the task is where=anywhere, so neither
                             Ormulus's wandering nor your own room matters —
                             the route buys it standing in the butcher's shop)
go stealth                  (lapses after 20 turns, event 5)
status / stats / wealth
```

Ends at **$159.68**, stamina 86/102, 58 turns.

---

## Mechanics worth keeping

- **A rejected command costs no turn.** `attack bandit` with no bandit present
  answers "That is not an option or command." and the clock does not advance —
  it is a parser rejection, not a failed action. This matters twice over: an
  adaptive driver can probe freely, and surplus probes can simply be **deleted**
  from the derived route afterwards, leaving the turn count and every subsequent
  event untouched. Substituting `z` for them instead desynchronises everything,
  because `z` *is* a turn.
- **`nap on the bed` does not heal.** Task 51's action is `type=7 v1=2` —
  MaxStamina **+2** — so the reassuring "You feel a little better" raises your
  ceiling by 2 and leaves current stamina exactly where it was. Repeatable
  indefinitely, and useless when you are hurt. Current stamina comes back at
  +1 a turn from `doze on the chair`, or +2 once from `relax on the chair`.
- **Food heals only where you bought it.** Event 0 `#plateremoval` (30 turns)
  takes the steak and the mug away, so `take a bite out of the steak` on the
  trail just gets "Where's the beef?". Coffee is worse: `sip`/`gulp`/`chug` are
  all `where = room 0`, the tavern.
- **The cents counter is never normalised.** `wealth` happily reports
  "$464 and 108 cents"; `change cents into a dollar` is the manual fix.
- **The intro's own hint chain is accurate** — "after killing a bandit, use the
  command 'search for money'", and Drako's "You can sell that to me for 500
  dollars!" tells you the exact `sell` phrasing. The route follows it.

## Battle System data (both builds)

| Character | Str | **Acc** | Def | **Agi** | Stamina | KilledTask |
|-----------|-----|---------|-----|---------|---------|------------|
| **Player (Pindarus)** | 1 | **0** | 0 | **0** | 100 | — |
| Bandit (npc 2) | 5 | **0** | 3 | **0** | 30 | 19 |
| Ormulus (npc 6) | 3 | **0** | 2 | **0** | 30 | — |
| Deer (npc 5) | 0 | **0** | 0 | **0** | 20 | 37 |
| (the other nine NPCs) | 0 | **0** | 0 | **0** | 0 | — |

Every weapon is `acc=0` too (`SCR_DUMP_OBJLOC=1` prints `acc=` beside `hit=`);
the eight objects with any battle value are gold armor (hit 8 / prot 8), heavy
sword 8, Azranian Soldier's Sword 8, broad sword 6, hunting sword 5, Calado
Special Edition Knife 5, hunting dagger 4, steel switchblade 3. The 22 type-7
"Change Battle Attribute" actions in the file only ever address `Var1` 0
(Attitude), 1 (Stamina) and 2 (MaxStamina) — nothing can move Accuracy off 0.

Under 4.0 rules that is fatal; under 3.9 rules the hit test is skipped entirely
and none of it matters. Task 19 `#banditkristdies` and task 37 `#deerdies` both
hand their NPC a full stamina restore (+30 / +20) on the way out, which is the
author's renewable-farm design showing through — but the Runner stamps the dead
NPC's room to −5 *after* the KilledTask runs and its walk never ticks again, so
the restore is dead code and the farm never turned. See "Goal 5 was never
possible" above.

### Summary

| Goal | 4.00 build | 3.90 build |
|------|-----------|-----------|
| 3. Shop at Harthro's / Calado's / Golapho's | ✅ | ✅ |
| 4. Stay at Gralle's Inn | ✅ | ✅ |
| 1. Kill a bandit & take his money | ❌ acc 0 vs agi 0 ⇒ no hit lands | ✅ |
| 2. Kill a deer & sell the carcass | ❌ same stalemate | ✅ $500 a head |
| 6. Learn Stealth Tactics | ❌ no income ⇒ never $800 | ✅ |
| 5. Purchase a house | ❌ no income ⇒ never $7,500 | ❌ one bandit + one deer ⇒ $959.68 max |

The 3.90 file completes five of the six goals its author set. **Goal 5 is out
of reach in both builds** — on the 4.00 file because no blow can land, on the
3.90 file because battle death is permanent in the real Runner and the game's
renewable-hunting economy therefore never existed. Rooms 13/14/15 (the house
interior) join the Town Council Hall and the Law Building as content no player
could ever have seen.
