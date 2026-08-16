# The Town of Azra — Walkthrough

ADRIFT game by Stan Tencza. An explicitly **unfinished** RPG-sandbox ("This
game is still under development… The game is nowhere near completion."). Derived
by playing under SCARE (deterministic seed) and cross-checked against the game's
internal task, exit, and **Battle System** data.

There is **no score and no winning ending** in either build (`score` reports a
maximum of **0/0**; the file contains no `ACT type=6` at all). "Completing"
Azra means working through the six goals the author lists in the intro:

1. Kill a bandit, and get his money
2. Kill a deer, take the carcass and sell it to Drako
3. Make a few purchases from Harthro's, Calado's, Golapho's
4. Stay at Gralle's Inn
5. Purchase a house
6. Learn Stealth Tactics

You are **Pindarus**, a man in the Roman-ish town of Azra, starting with **$500**
and the clothes on your back.

---

## ⚠️ Azra ships as two files, and they are not the same game to play

| corpus file | signature | sha256 (first 16) | goals reachable |
|---|---|---|---|
| `The_Town_Of_Azra.taf` (IF Archive) | **4.00** (`…c2 cf 93 45 3e 61`) | `e40ec150e1531f49` | **3 and 4 only** |
| `The Town Of Azra.taf` (adrift.co) | **3.90** (`…c2 cf 94 45 37 61`) | `14684c1d2cd8b929` | **all six** |

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
| `goldens/the_town_of_azra_v390_solution.txt` | `The Town Of Azra.taf` (3.90) | **all six goals**, 505 turns |

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

## The 3.90 build: all six goals

`goldens/the_town_of_azra_v390_solution.txt`, 414 commands / **505 turns**,
run with `SCR_SKIP_WAITKEY=1` (the city wall north of Northern Trail prints
"Press any key to continue!" every time you cross it). The route is the block
above, then:

### 1. Armour up so the grind is free (the key move)

```
wear the bronze helmet     (the 4.00 route buys it and never puts it on)
n / w
buy copper armor           ($180)
wear the copper armor
e / n / n                  (-> Bandit Trail)
```

Damage is `strength − defence`, applied only when positive. The bandit's
strength is **5**; rawhide armour (2) + bronze helmet (1) + copper armour (4)
puts defence at **7**. From that point **bandits cannot land a single point of
damage** and the entire rest of the game can be played without ever healing.
Without the copper armour you take 3 a turn, and roughly 26 stamina per bandit,
against 100 — the grind is survivable but needs constant trips back to the inn.

### 2. Goal 1 — bandits, until stealth tuition is affordable

```
attack bandit  ... x8-10    ("You kill the vile bandit!")
search for money            ("You now have $...")
z                           (wait for the next bandit to walk back in)
```

Two kills is enough here. The purse is **random per bandit** — observed $193,
$217 and $432 — so the route is derived adaptively and then frozen. The
respawn is the Bandit NPC's walk step re-asserting its room, so a wait of a few
turns brings a fresh, full-stamina bandit back; `search for money` must be typed
while the dead body is still there.

### 3. Goal 6 — Stealth Tactics

```
learn stealth from ormulus  ($800; the task is where=anywhere, so Ormulus's
                             own wandering does not matter)
go stealth                  (lapses after 20 turns, event 5)
n                           (-> Northern Forest)
```

Worth knowing: **stealth is not actually needed to hunt.** Task 33
`#deerrunsoff` is the Deer's walk-completion task and only fires when the walk
step ends, which never comes round while you keep swinging — an unstealthed
player kills deer just as well (measured: 7 kills in 40 attacks). It is on the
route because it is goal 6, not because the hunt requires it.

### 4. Goals 2 and 5 — the deer economy, x15

```
attack deer  ... x4          ("You strike the deer one last and final time,
                              and it falls dead!"  ...  "Deer comes in.")
take the deer carcass
s / s / s / s / w / s        (Forest -> Bandit Trail -> ... -> Butcher Shop)
show the deer carcass to Drako
sell the deer carcass to Drako for 500 dollars
n / e / n / n / n / n        (back to the Forest)
```

The deer dies in **four** chops with the hunting sword and a replacement walks
in **on the same turn**, so unlike the bandit there is no respawn gap. Drako
pays a flat **$500** whatever number you type in the `sell` command (task 39's
action is a hard `dollars += 500`), and both `show` and `sell` are repeatable.
That is $500 per 20-turn round trip — about $28 a turn, against roughly $16 a
turn for bandits. Fifteen carcasses take $103 to $7,603.

### 5. Goal 5 — the house

```
n / e / e                   (Butcher -> Outside Stores -> Plaza -> Residential)
buy house                   ($7,500; "The house IS YOURS!")
unlock the front door        (the key comes with the sale)
in
```

Rooms 13/14/15 — Sitting Room, Bedroom, Dining Room — exist only behind this
purchase, and they carry the game's best rest tasks: `relax on the large sofa`
(+4 stamina), `nap on the small sofa` (+5), against `doze on the chair` (+1) at
the inn. The route finishes by touring all three and calling `stats`.

---

## Mechanics worth keeping

- **A rejected command costs no turn.** `attack bandit` with no bandit present
  answers "That is not an option or command." and the clock does not advance —
  it is a parser rejection, not a failed action. This matters twice over: an
  adaptive driver can probe freely, and the 49 such probes in the derived route
  could simply be **deleted** afterwards, leaving the turn count and every
  subsequent event untouched (505 turns before and after). Substituting `z` for
  them instead desynchronises everything, because `z` *is* a turn.
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
author's renewable-farm design showing through — and on the 3.90 file it works
exactly as intended.

### Summary

| Goal | 4.00 build | 3.90 build |
|------|-----------|-----------|
| 3. Shop at Harthro's / Calado's / Golapho's | ✅ | ✅ |
| 4. Stay at Gralle's Inn | ✅ | ✅ |
| 1. Kill a bandit & take his money | ❌ acc 0 vs agi 0 ⇒ no hit lands | ✅ |
| 2. Kill a deer & sell the carcass | ❌ same stalemate | ✅ $500 a head |
| 6. Learn Stealth Tactics | ❌ no income ⇒ never $800 | ✅ |
| 5. Purchase a house | ❌ no income ⇒ never $7,500 | ✅ + rooms 13/14/15 |

The 3.90 file completes every goal its author set. Only the Town Council Hall
and the Law Building stay out of reach, and those are unreachable by
construction in both builds.
