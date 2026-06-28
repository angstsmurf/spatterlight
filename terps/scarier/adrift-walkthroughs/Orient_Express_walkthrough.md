# The Oriental Express — Walkthrough

ADRIFT game ("The Oriental Express"). Derived by playing to completion under
SCARE (deterministic seed) and cross-checked against the game's internal
scoring table (17 scoring tasks).

**Result: 300 / 300 points** and the winning ending ("You successfully complete
your assignment").

You are **Haveno Cash**, a private detective hired by Anita Fortune to recover
a precious family jewel stolen from the Fortune estate. The trail leads aboard
the Oriental Express, through a comic cast of misfits, off at HopeImakeit
Station, and into a back-alley booze warehouse where the jewel turns up in the
last place you'd want to reach into.

---

## Full command list (300 points, start to finish)

```
examine mustach            (+2, examine Oddly Istink's mustache)
take coin                  (from the soda machine's change return)
put coin in slot           (+3, the coin buys a newspaper from the stand slot)
take newspaper             (the headline: a jewel was swiped from the Fortunes)
open suitcase
take pistol                (needed later to deal with Igotta Bigbottom)
board train                (+5)
give ticket to oddly istink (+3, the conductor takes your ticket)
s                          (Inside train -> Compartment A)
s                          (Compartment A -> Compartment B)
take bag
give bag to ivanna         (+3; her note: "Meet me in the lounge. The Jewel is here.")
s                          (Compartment B -> Compartment C)
s                          (Compartment C -> Lavatory Car)
e                          (Lavatory Car -> Compartment A bathrooms)
use toilet                 (+2)
wash hands                 (+2)
in                         (-> shower)
take shower                (+25)
out
w                          (-> Lavatory Car)
s                          (Lavatory Car -> Dining Car)
read menu                  (+5)
take spoon                 (the spoon engraved "OE"; unlocks the door to the Lounge)
order tea                  (+5, "Order *")
give coin to waiter        (+20)
give waiter tip            (+10)
s                          (Dining Car -> Lounge Car)
z                          (wait ~4 turns for Ivanna to walk in from the north)
z
z
z
buy ivanna a drink         (+5)
kill igotta bigbottom      (+0; with Ivanna present as witness, you shoot Igotta)
take card                  (+50, the business card drops from the dead Igotta)
n                          (Lounge -> Dining)
n                          (Dining -> Lavatory)
n                          (Lavatory -> Compartment C)
n                          (Compartment C -> Compartment B)
n                          (Compartment B -> Compartment A)
out                        (leave the train -> HopeImakeit Station)
use phone                  (+0; the kidnappers call - get the cash from the card's address)
w                          (HopeImakeit Station -> Up the Creek Road)
n                          (Up the Creek Road -> Booze Barn)
give card to habibo        (+60, Habibo hands you the brown paper sack)
examine ivanna             (+0; unlocks the door west to the Warehouse)
w                          (Booze Barn -> Warehouse)
examine anita              (+50, untie Anita Fortune; "go retrieve the jewel")
shoot thug                 (+0, drop the Thug with your pistol)
examine ivill getyou       (+0; the boss falls - the jewel is in his belly button)
use spoon to get jewel     (+50, dig it out with the spoon -> WIN)
```

---

## Walkthrough, explained

### Phase 1 — At the Station
1. `examine mustach` (**+2**) — Oddly Istink's straggly mustache.
2. `take coin` from the soda machine's change return, then `put coin in slot`
   (**+3**) — the coin actually goes into the *newspaper* slot and buys a paper.
   (The single coin is later also "given to the waiter"; see the note below —
   both score.)
3. `take newspaper` and read it: a jewel was stolen from the Fortune estate.
4. `open suitcase`, `take pistol` — you'll need it for the lounge thug.
5. `board train` (**+5**). The conductor (Oddly Istink) follows you aboard;
   `give ticket to oddly istink` (**+3**). This unlocks travel down the train.

### Phase 2 — Down the train
Each car's far door is locked until you've done the right thing, so the order
matters:
6. `s, s` to **Compartment B**. `take bag`, then `give bag to ivanna` (**+3**).
   Her note: *"Meet me in the lounge for a drink. The Jewel is here."* Giving
   her the bag opens the way south.
7. `s` to **Compartment C**, `s` to the **Lavatory Car**.
8. Detour `e` into the **Compartment A bathrooms**: `use toilet` (**+2**),
   `wash hands` (**+2**), `in` to the shower, `take shower` (**+25**), then
   `out, w` back to the Lavatory Car.
9. `s` to the **Dining Car**. `read menu` (**+5**), `take spoon` (the "OE"
   spoon — it both unlocks the door to the lounge and is your jewel-extraction
   tool), `order tea` (**+5**). The waiter Gimme Atip wanders in;
   `give coin to waiter` (**+20**) and `give waiter tip` (**+10**).

### Phase 3 — The Lounge
10. `s` to the **Lounge Car**. Igotta Bigbottom is here and keeps swinging at
    you (his hits can't land — the Battle System rolls in your favor). **Wait**
    (`z` ~4 turns) until **Ivanna walks in from the north** — you need a
    witness. Then `buy ivanna a drink` (**+5**) and `kill igotta bigbottom`:
    you pull the pistol and shoot him. `take card` (**+50**) — the business
    card he was carrying drops to the floor and the staff haul his body off.

### Phase 4 — Off the train, into the alley
11. Head back up the train: `n, n, n, n, n` to **Compartment A**, then `out`
    to **HopeImakeit Station** (taking the card unlocked the exit).
12. `use phone` — the kidnappers call: they've grabbed Anita; collect the cash
    from the address on the card. This unlocks the road `w`.
13. `w` to **Up the Creek Road**, `n` to the **Booze Barn**. `give card to
    habibo` (**+60**) — he hands over the brown paper sack of cash.
14. `examine ivanna` — needed to unlock the door `w`. `w` into the **Warehouse**.

### Phase 5 — The jewel
15. `examine anita` (**+50**) — you untie Anita Fortune; the game promises you
    can now "retrieve the jewel." The Thug and "BIG" Ivill Getyou attack.
16. `shoot thug` drops the Thug. `examine ivill getyou` — the enormous boss
    crashes down and you spot the **J.O.B.** (the jewel) lodged in his belly
    button.
17. `use spoon to get jewel` (**+50**) — dig it out of the bellybutton with the
    engraved spoon. You hand it to Anita and escort her home. **You win.** 🎉

---

## Note on the coin (flavor text vs. scoring)

The game has exactly **one** coin, taken from the soda machine's change return.
Two scoring tasks reference it: `put coin in slot` (**+3**, at the station) and
`give coin to waiter` (**+20**, in the dining car). Putting the coin in the slot
*consumes* it (you get a newspaper and the change return is then empty), so it
looks as though you must choose one or the other.

In fact **both score**. The "give coin to waiter" task's restriction does not
require the coin to be in your inventory — it only checks that you're with the
waiter — and its line *"You toss the coin to the waiter"* is pure flavor. So the
optimal route does `put coin in slot` first (banking the +3) and still scores
`give coin to waiter` (+20) later for the full **300 / 300**.

(This is the same "flavor text can lie" pattern seen in other ADRIFT games:
trust the `score`, not the prose.)
