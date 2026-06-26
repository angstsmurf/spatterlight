# Alexis: Dalskee — walkthrough (WON, 23/65; win verified deterministic)

**Game:** `ALEXIS.TAF` — *"Alexis: Dalskee"* by Kingsbury. **Native ADRIFT 3.9**
(TAF sig byte8=`0x94`/byte10=`0x37`), a fantasy quest with the ADRIFT Battle
System. You are Alexis; the diary sets the goal: *journey to the castle of Uron
and destroy the Elves' enemy, **Urgorn***. Your companion **Serond** (a friendly
wizard NPC) wanders the land and fights at your side.

**Result:** **WIN** ("Congratulations, you have beaten Urgorn and saved the
Country Dalskee…"), deterministic, banked at **23/65**. The max is 65; the
banked route takes the *minimum-combat win path* (23 pts). The remaining ~42
points are **optional** (extra monster kills + Hard difficulty + a few side
puzzles) and are listed at the end as a max-score follow-up. Solution:
`harness/alexis_solution.txt` (101 commands).

## The win spine

Collect the **7 elemental stones** (Forecarn, Tonerith, Dusteron, Glaven,
Kedarn, Longmore, Nelone) → give all 7 to **Larnt** at Nelone Bridge (he's a
traitor; this opens the bridge to Uron) → cross into **Uron Castle** (say the
password, open the door) → fetch the keys → descend through the Mirror/Torture
rooms to the **Dungeon** → **kill Urgorn** (the type-6 EndGame win).

**Only one fight is mandatory: Urgorn.** Every stone simply lies in a room, and
the central hub is opened by *giving food to Tarin* (not by fighting the
Bridgekeeper). Combat is the **3.9 legacy strength-vs-defence** model and works
fine — Serond does most of the damage; a plain `small sword` (easy difficulty)
plus one drink of the gourd is enough to drop Urgorn.

## Two gotchas that cost real time

1. **Light the lantern AT HOME.** Task 1 (`light *lantern *`, +5) is **scoped to
   the cottage** — it is *not* runnable at the cave entrance. Fuel it
   (`put oil in lantern`, +3) and `light lantern` in the cottage *before* you
   leave; carry the lit lantern into the Caves of Eternal Night. (This looked
   like a parser bug — the pattern has a glued `*lantern` wildcard — but SCARE
   matches it correctly when the task is in scope. **Faithful, no engine issue.**)
2. **The compass labels in the structural dump are scrambled** (ADRIFT's
   internal direction order ≠ the dump's slot order). **Navigate by the game's
   own room prose** ("West takes you…"), not by the dumped exit slots. The room
   *connectivity* in the dump is correct; only the N/E/S/W labels are unreliable.

## Solution (annotated)

**Cottage (room 0):** loot, fuel + light the lantern.
```
easy            (difficulty: easy = +20/+20 to stats, +0 score; see max-score note)
take all        (lantern, cloak, nice food, diary; Forecarn stone already in your bag)
wear cloak
down            (cellar)
take oil        (a bottle of oil, on the shelves)
take coin        (gold coin -> money, for shops)
up
put oil in lantern   (+3)
light lantern        (+5  -- MUST be done here in the cottage)
```
**West loop — Tonerith, small sword, Dusteron, into the caves:**
```
out
east            (Tonerith Pass)
take tonerith stone
west
north           (Hills of Forecarn)
take small sword
south
west            (Dusteron Pass)
north
north           (Dusteron Desert)
take dusteron stone
east            (Dusteron plains river bank)
push tree       (+2; collapses the rotting tree across the river -> opens north)
north
north           (Entrance to the Cave)
nw              (into the dark Passage; the lit lantern lets you continue)
nw              (Main Cave -- Narfild the troll is here, Serond fights him)
e               (Large cave)
take glaven stone   (off the large stone table)
w               (Main Cave)
se
se              (back out to the Cave Entrance)
```
**Hub via Tarin, then Longmore + Kedarn stones:**
```
ne              (Long Glaven path -- Tarin the dwarf bars the way NE)
give food to tarin   (+2; he steps aside)
ne              (Glaven junction = the central hub)
se              (Longmore junction)
take longmore stone
se              (Longmore marshes -- Serond fights the marsh king)
se              (Longmore pass)
se              (Track junction)
e               (Kedarn junction)
s               (Kedarn forest opening)
se              (Kedarn forest clearing -- Forester Goblin hangs here)
take kedarn stone
```
**Back to the hub, up the Nelone mountains for the last stone, then to Larnt:**
```
nw              (forest opening)
n               (Kedarn junction)
w               (Track junction)
nw              (Longmore pass)
nw              (Longmore marshes)
nw              (Longmore junction)
nw              (Glaven junction / hub)
ne              (Nelone mountain junction, r48)
ne              (Nelone mountain pass -- a Narn here)
e               (Thin mountain path)
se              (Nelone mountain junction, r56)
take nelone stone   (all 7 stones now)
e               (Nelone bridge West)
e               (Nelone Bridge -- Larnt)
give stones to larnt   (he laughs evilly, sheds his cloak, flies to the castle;
                        the bridge east is now open)
e               (Nelone Bridge East)
```
**Up the Uron dark path to the castle:**
```
e               (Uron Dark Path)
ne              (Karurn the Narn is here)
nw
ne
n               (Uron Castle Entrance)
say the password    (+2; "the words seem to have moved the doors")
open door           (+3; the doors creak open)
n               (Uron Castle Main Hall)
```
**Fetch the keys (small key in the Long room, large key in the bedroom chest):**
```
n               (Long room -- a locked stone door on the north wall)
take all        (small key, off the long table)
s               (Main Hall)
w               (Corridor)
u               (Bedroom)
unlock chest    (with the small key)
open chest      (+1)
take all        (the large key)
d               (Corridor)
e               (Main Hall)
n               (Long room)
unlock door     (+2; the stone door, with the large key)
open door
n               (Mirror room)
```
**The Mirror-room trap, then Urgorn:**
```
drink gourd     (Serond's small gourd: +20 defence, a small combat cushion)
north           (Torture room -- the SAFE exit. In the Mirror room, `east` and
                 `west` are death traps that fling you back across the map;
                 only `north` is correct.)
north           (+3; Dungeon -- Urgorn seals the door behind you)
attack urgorn   (repeat; Serond also attacks. Urgorn falls in ~a dozen rounds)
... (attack urgorn x15 in the banked file -- extra swings after he dies are
     harmless) ...
```
→ *"Receiving one more hit, Urgorn falls backwards… Congratulations, you have
beaten Urgorn and saved the Country Dalskee."* **WIN.**

## Score breakdown (banked 23/65)

put oil +3, light lantern +5, push tree +2, give food to Tarin +2, say password
+2, open castle door +3, open bedroom chest +1, unlock stone door +2, enter the
Dungeon (north) +3 = **23**. The win task (`urgorn dies186457`) is itself unscored.

## Max-score follow-up (the other 42 pts — optional, NOT yet banked)

All faithful, all reachable in principle, but each adds combat/backtracking and
some interact with the shared RNG / your survivability:
- **Hard difficulty +10** instead of Easy (but Hard applies **−20/−20** to your
  stats — viable only with the power-up items below; Easy is safer for a clean
  win, like *light_up*'s `hard` tradeoff).
- **Optional monster kills** (Serond helps; legacy str-vs-def): Narfild +5
  (Main Cave), Bridgekeeper +5 (Bridge of Tonerith), Wolf +2, Swamp monster +2,
  Forester Goblin +2 (drops the +20-strength juice), Eagle +3, Narn +1, and
  possibly Larnt +5 in the castle.
- **Side puzzles:** turn ring +3 (caves), water-in-pan +1 and fill-pot +1
  (elven water point), unlock the marshy-cave chest +1, dig +2 (needs the metal
  spade), touch ball +1 (**warning: r66's "ball" is a teleport trap**).
- **Power-up items for a Hard-mode/clean fight:** Haron's phial of elven potion
  (**+200 stamina**), the juice (+20 str), elven health leaves (+30), the elven
  armour/dagger (cupboard, behind the boiling-water puzzle), and the magic-cube
  / `dard dard larna dard` buff spell.

## Reproduce
```
sh harness/play.sh "games/ALEXIS.TAF" harness/alexis_solution.txt
```
Native 3.9, deterministic; no SCARE change and no combat-assist needed (legacy
strength-vs-defence combat works, and Serond fights alongside you).
