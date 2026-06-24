# Wes Garden's Halting Nightmare — walkthrough / analysis

*Wes Garden's Halting Nightmare* by **Jubell** (ADRIFT 4, written for the ADRIFT
Spring Thing, 2010; "made with all of the unregistered Adrift limitations in
place"). A graphics‑heavy file (3.5 MB of embedded illustrations) but a tiny
game: **10 rooms, 25 tasks, 3 NPCs**.

**Result: UNWINNABLE — max reachable 30/100, deterministic.** The intended win
exists and the whole back half of the game is fully built and works, but it is
sealed behind a single **orphaned key object**: the *gold ring* required to
solve the very first gate (the Lovers' Fountain) sits on a *severed hand* that
**no task, event, or character‑walk in the game ever brings into play**, so the
fountain can never be solved and everything downstream of it is unreachable.

Solution file: `harness/wes_ghn_solution.txt` (deterministic, seed 1234).

## The story / setup

Wes Garden, grieving his grandfather and (years earlier) his vanished parents,
is talked into his Uncle Copeland's meditation class and wakes inside **Mercy
Hospital**, a surreal afterlife‑ish nightmare. He can *summon the Soul Scythe*
at will (the game's combat verb), and must work his way from the Emergency Room
Foyer, through a "Core of Beauty", "Halting Chapel", "Radiology", a "Medicine
Cabinet" and a "Maternaless Ward", to box up the candy‑striper **Hope
Endlessly**'s spirit.

## Map (dump‑derived; in‑game compass labels are reliable here)

```
 Foyer(0) ─N→ Walkways(1) ─N→ Waiting(2) ─N→ Grand Corridor(3)
                                                 │  ├─E→ Halting Chapel(5)
                                                 │  └─W→ Core of Beauty(4) ─N→ Radiology(6)
                                                 └─N(locked, optical scanner)→ Medicine Cabinet(7) ─N(locked)→ Maternaless Ward(8) → … → Outside(9)
```

The Foyer→Walkways step is one‑way (a "roaring" mob makes the Foyer
inescapable; closely examining the balcony figure destroys the path back).

## The reachable route (banked, 30/100)

```
talk to charity        (Foyer)        +10   then flee North (the patients turn hostile)
closely examine figure (Walkways)     +5    opens the North door; a bazooka man destroys the way back
ring bell               (Waiting)      0    flavour ("nothing appears to happen")
                         → North to the Grand Corridor
summon scythe                          0    "a black mist… forms the Soul Scythe" (your weapon)
                         → West to the Core of Beauty
drink water             (Core)         +5
                         → East, East to the Halting Chapel
talk to hope                           0    enables taking the candle
take candle                            0    Hope turns hostile and attacks
attack hope ×6                         +10  the Soul Scythe kills her (acc 50 > her agi 30); "#Hopedies"
```

That is every reachable point. **Score 30/100.** (`ring bell`, `summon scythe`
and `examine portraits` score nothing.)

### Combat note (the Battle System here is correctly configured)

Unlike Azra / Villains & Kings / To‑Hell / Mr‑Smith (whose authors left every
accuracy/agility at 0, silently disabling combat), this game configures combat
properly. With the **Soul Scythe summoned** the player rolls accuracy **50**
vs Hope's agility **30**, so blows land and she dies in ~6 hits; the player has
40 max stamina and Hope deals ~5/hit, so the kill is comfortably survivable. No
`SC_ASSUME_COMBAT` assist is needed. (Footgun discovered while deriving this:
the attack verb is plain `attack hope` — *not* `attack hope with scythe`, which
the grammar does not take; and the scythe must be summoned first or the player's
bare‑handed accuracy loses to Hope's agility.)

## Why the other 70 points (and the win) are unreachable

The win is **`Put Hope's spirit into box`** (task 24, EndGame/win) in the
Chapel, reached through a long but fully‑built chain. Walking the data backwards:

* **Win (task 24)** needs *Hope's Spirit* held, which is produced by
  **`Give Sighing Prudence to Hope` (task 23, +5)**.
* The *Sighing Prudence* and the keys for that step come from the **Maternaless
  Ward** (`Knock on door` task 19 +5 → reveals the *La Virgencita key*;
  `Walk through Magna Mater door` task 20 +5).
* The Ward is behind the **Medicine Cabinet door**, whose key (per the door's
  `Key` field) is the **vial** — and the vial is dropped into the Core of Beauty
  by solving the fountain.
* The **Medicine Cabinet** (and its `Talk to Charity` task 15 +10) is behind the
  **Grand Corridor door**, an *optical scanner* unlocked with an **eyeball** —
  obtained by **`slash painting` (task 14, +5)** of "The Great Lover", Dr.
  Micheals' portrait.
* `slash painting` requires **`Talk to Micheals` (task 12)** first, and Micheals
  lives in **Radiology**, which is opened only by **`#OpenRadiology` (task 16)**.
* Task 16 is executed by exactly two tasks: the fountain (task 11) and the
  Medicine‑Cabinet Charity (task 15). Task 15 is itself behind the scanner door
  (eyeball → portrait → Micheals → Radiology), so the **only non‑circular way to
  open Radiology is the Lovers' Fountain (task 11).**

### The single fatal orphan: the gold ring

The fountain task — **`Give ring and candle to fountain` (task 11, +20)** — has
three restrictions, two of which require the player to **hold the gold ring**
and **hold the candle**. The candle is takeable in the Chapel. The **gold ring
is not obtainable at all**:

* At game start the gold ring (dynamic object 2) is positioned **`on` the severed
  hand** (its authored `InitialPosition = 3`/on‑surface, `Parent = 0` = the
  hand), and the severed hand (object 3) has authored **`InitialPosition = 0`
  (Hidden)**, `Parent = -1`. The `-1` is just "no parent" — the normal companion
  to a Hidden start, *not* a corrupt/out‑of‑bounds room (that would trip SCARE's
  "out of bounds room" error path instead). Tellingly, **every** other
  reveal‑later object — candle, eyeball, vial, Hope's Spirit, both keys, the
  Sighing Prudence — has the *identical* `InitialPosition = 0`/`Parent = -1`
  start and is pulled into play by a task action; the severed hand is the one
  whose reveal action was never written.
* Enumerating **every** task action, both timed events, and all NPC
  character‑walks: the severed hand (object‑ref `var1=6`) is referenced by only
  **one** action — `Walk through Magna Mater door` (task 20), which *hides* it —
  and the gold ring (`var1=5`) by only two — the fountain's own *hold* check and
  the fountain's "consume the ring" hide. **Nothing ever un‑hides the hand or
  the ring.**

So the ring can never be held → the fountain can never fire → Radiology never
opens → the eyeball, the Medicine Cabinet, the Ward, the *Sighing Prudence*, the
spirit and the boxing‑the‑spirit win are all permanently sealed.

This was confirmed constructively: force‑injecting the gold ring into the
player's hands makes the **entire** remaining chain work end‑to‑end (fountain
+20 → vial → Radiology → `slash painting` +5 → unlock scanner door with eyeball →
Medicine Cabinet `talk to charity` +10 → unlock the next door with the vial →
the Ward), i.e. the gold ring is the **sole** structural break. The other 70
points all sit downstream of it.

### Faithful to the original Runner

SCARE reads the orphaned hand/ring straight from the `.taf`; the reference
ADRIFT Runner has the same data and no reveal action either, so the game is
unwinnable in the real Runner too. This is a plain authoring omission (the
author forgot to give the player a way to obtain the wedding ring of "The Great
Lover"), not a SCARE divergence — the same class of bug as Spirit's Flight's
orphaned Ice Totem.

**Not a 3.9→4.0 conversion casualty either.** The TAF signature is native
**4.0** (header byte 8 = `0x93`, byte 10 = `0x3e`, matching Sun Empire; a 3.9
game such as Spirit's Flight reads `0x94`/`0x37`). The conversion bug that the
opt‑in *move‑assist* repairs is a move action whose "To:" combo was left at VB's
default `-1` (destination stranded in `Var3`, silently ignored by the Runner) —
but **no move action in this game has an unset (`-1`) destination**, and running
with `SC_ASSUME_MOVES=1` yields the identical 30/100. The break is not a
fires‑but‑goes‑nowhere move; it is the *complete absence* of any action that
un‑hides the severed hand, so move‑assist cannot touch it.

## Reproduce

```
sh harness/build.sh
sh harness/play.sh ~/adrift-battle/games/WesGHN.taf harness/wes_ghn_solution.txt
```

Deterministic: 30/100 on every run; the game continues (no win marker).
