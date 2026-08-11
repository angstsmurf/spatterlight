# The Lost Mines — walkthrough

- **Engine:** ADRIFT 3.9 (`lostmines.taf`, 37,088 bytes). A 1920s gold-rush
  town: 16 rooms, 64 objects, 54 tasks, 2 NPCs (the Bartender and Gus), 2
  events.
- **Result:** ★ **WON, 100/100** — the sum of every `ACT type=4` in the file.
  There are no negative ones.
- **Solution:** `goldens/lostmines_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `Congratulations, you have found the lost gold.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS` +
  `SCR_DUMP_OBJLOC`.

## Route

```
s / read poster                        (+5)
s / w / in                             (Cabin)
open cabinet / take key / open box     (+5, the box holds cheese)
d / open fridge / open desk            (+5 axe, +5 coupon)
u / out / up c                         (+5, climb the log pile to the roof)
use axe on boards                      (+10)
open box / take dynamite
d / w / w / in                         (Home)
open closet / take hat / open pillow / take cards
wear hat                               (+5)
out / e / in / u                       (Gambling Room)
use cards on table                     (+5, swaps the decks → matchbook)
out / e / e / n / n / in               (+5, into the mine)
take ring                              (+10)
n / use cheese on rat                  (+5, opens the tunnel north)
s / s / s / s / w / w / in             (Blue Keg Pub)
give ring to bartender                 (+5)
ask bartender about boulder            (+5, he gives you a pencil)
sign coupon                            (+5)
give coupon to bartender               (+5, he gives you a beer)
out / e / e / n / n / in / n / n       (back to the blockage)
give beer to gus                       (+5)
score
use dynamite on blockage               (+10, EndGame win)
```

## Scoring — all 100

| Task | Command | Points |
|---|---|---|
| 3 | `read poster` | +5 |
| 4 | `up c` (climb the logs) | +5 |
| 7 | `open box` (Cabin) → cheese | +5 |
| 8 | `open fridge` → axe | +5 |
| 9 | `use axe on boards` | +10 |
| 26 | `open desk` → coupon | +5 |
| 21 | `wear hat` | +5 |
| 23 | `use cards on table` → matchbook | +5 |
| 22 | `in` (enter the mine) | +5 |
| 33 | `take ring` | +10 |
| 34 | `give ring to bartender` | +5 |
| 35 | `ask bartender about boulder` → pencil | +5 |
| 39 | `sign coupon` | +5 |
| 28 | `give coupon to bartender` → beer | +5 |
| 36 | `use cheese on rat` | +5 |
| 41 | `give beer to gus` | +5 |
| 40 | `use dynamite on blockage` (EndGame win) | +10 |
| | **total** | **100** |

## The chain of favours

Old Gus is standing at the blockage with a pick and no intention of leaving,
and the whole middle of the game is the single-file sequence that gets rid of
him. Each link is written as a restriction on the next, so nothing here can
be reordered:

```
take ring (in the mine)
  -> give ring to bartender          T34 needs the ring held
  -> ask bartender about boulder     T35 needs T34 done   -> pencil
  -> sign coupon                     T39 needs the pencil -> a valid free-beer voucher
  -> give coupon to bartender        T28 needs T39 done   -> beer
  -> give beer to gus                T41
  -> use dynamite on blockage        T40 needs T41 done + the matchbook
```

The bartender's own hint (T51) spells out the joke: *"He says he's strong, do
you have anything he could lift?"* — you ask him to shift the boulder at the
mine entrance, and the pencil is what he happens to be holding when he agrees.

## Gated exits

| Exit | Gate |
|---|---|
| `room=0 IN -> 13` (into the mine) | T22, restricted on **T21 `wear`** — the hard hat |
| `room=14 N -> 15` (to the blockage) | `gateTask=36 wantDone=1` — **`use cheese on rat`** |
| `room=10 U -> 11` (gambling room) | `gateTask=23 wantDone=0` — closes once you swap the decks |

The hard hat is in the closet at **Home**, at the far western end of the map;
the mine is at the far east. That single restriction is what makes the route a
there-and-back-and-there-again, and it is worth doing the whole western half
(cabin, cellar, roof, home, pub) in one pass before ever walking into the mine.

## Two containers that refuse to be carried

- **The metal box on the cabin roof.** `take box` → *"It's too heavy to lift,
  it'd be easier to just look in it here."* The stick of dynamite is inside:
  `open box`, then `take dynamite`.
- **The lumpy pillow at home.** `take pillow` → *"You have no use for a lumpy
  pillow."* The deck of cards is inside: `open pillow`, then `take cards`.

Both are the same authoring pattern — an `unmoved` static with the real object
as a child — and both give a plausible refusal rather than a parser error, so
it is easy to conclude the object is scenery and walk away.

## Notes

- **The dynamite is decorative.** T40's restrictions are `RESTR type=0 v1=9`
  (matchbook held) and `RESTR type=2 v1=42` (T41 done) — it never checks for
  the stick at all. `use dynamite on blockage` scores the win with an empty
  pocket. Taking it is what the fiction wants, not what the engine needs.
- **The cards are decorative too.** T23 has `restr=0`; the deck swap works
  whether or not you found the pillow. The author's own hint (T44) is the
  puzzle as intended: *"The players are too busy trying to catch each other
  cheating for you to steal the matchbook… Switch the deck of cards you find
  with the deck on the table."*
- **The two events are pure atmosphere** — `EVENT 0 [Bird]` and
  `EVENT 1 [Miner]`, both `affTask=0`, `texts=---`, restarting every 20 and 10
  turns. Nothing in this game is timed.
- **Five hint tasks ship in the file** (T42–T44, T51, T52), each with
  `HINTQ`/`HINT1`/`HINT2`. Two are gated on progress (T51 on giving back the
  ring, T52 on reading the poster) so they only offer an answer once the
  question can have occurred to you.
- **`take shovel`** (T0) at the mine entrance is restricted on T35, i.e. you
  cannot pick it up until after the bartender has been asked about the
  boulder. It scores nothing and is never needed — a loose end.
- **The ending is honest about the con.** You blow a hole *beside* the
  blockage and load out the gold while Gus keeps digging: *"old Gus, well he
  finally cleared the blockage in the tunnel and found a rusty tin can on the
  other side."*
- **No `<waitkey>` anywhere** (`SCR_MARK_WAITKEY=1`).
