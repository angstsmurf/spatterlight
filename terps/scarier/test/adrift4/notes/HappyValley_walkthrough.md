# Happy Valley — walkthrough (**WIN**, 72 commands)

- **Author:** Jacqueline H. ("Lumin"), 2 July 2008; the game credits the idea
  to a 2006 NickyDude writing challenge that demanded a room containing
  "trees, bushes, grass, cottage, boulders, cave entrance, and a wooden sign".
  You have just been handed a job as curator of a fantasy adventure theme
  park, and the mines are on strike, the dragon has quit, and the elf is
  miserable.
- **Engine:** **ADRIFT 4.00**
  (`xxd -p -l 12 games/valley.taf | cut -c17-22` → `93453e`).
- **Result:** **WON.** No score system (`score` reports 0 of 0) — the ending
  room is literally called "You is a Winner!".
- **Source:** `downloaded/HappyValley_hints.txt`, from the game's own
  `valley.zip` (`README_if_stuck.txt`). Unlike every other file in
  `downloaded/`, this one *is* a bare command list — and it does not run.
- Row: `valley_solution.txt|valley.taf|and live happily ever after.|SCR_SKIP_WAITKEY=1`.

## Where the shipped hints file goes wrong

It reads as if it were written against a later revision of the game. Five
distinct breakages, all confirmed against the task dump:

1. `x path` / `x patch` / `x weeds` / `get weeds` are listed at **Outside the
   Mine**, but the small patch and the weeds are objects 96 and 97 and both
   live in room 0, **Happy Valley**. In room 2 they answer `You see no such
   thing.`
2. It then goes `n` … `s` from Outside the Mine, which only has `EXIT room=2 E`
   and `EXIT room=2 W`. Both are refused and the whole tail slides.
3. `enter 3436` cannot fire TASK 56, whose pattern is `enter 3436 *` — the
   trailing wildcard needs at least one more word. `enter 3436 on lock` works.
4. `turn on water` and `water plant` are listed before the cup has been filled,
   and `turn on water` is listed in the Curator's Office rather than the
   Bathroom next door.
5. `get cup` is attempted twice, once a room too early.

The route here is the same solution re-derived from the dump, and it keeps the
hints file's shape (and its two optional `ask` conversations) wherever the
original works.

## The three restrictions that actually constrain the route

**The potion counts to exactly five.** `give cup to granny` is TASK 46 and its
first restriction is

```
RESTR type=4 v1=2 v2=2 v3=5      # variable 0 == 5, and op 2 is '=='
```

Each of the five correct ingredients (purple spotted leaf, red globe berry,
wild demonflower, boring bark, white starflower) bumps variable 0 by one when
Goospaduggle takes it. The **pink** spotted leaf on the vine outside the mine
is a decoy: TASK 40 accepts it with the same "Aha! Perfect!" line but does
*not* increment. Give her that as well and the count is still five, so it is
harmless — but there is no sixth real ingredient, and no way to get to five
without all five of the right ones.

**The gloves must be worn, not carried.** TASK 36 (`get wildflowers` in the
Thicket) restricts the gardening gloves with `v2=2`, which is *worn*, not
`v2=1` (held). The demonflower bites. `wear gloves` is a separate command and
the hints file does have it — after two failed `get wildflowers` attempts,
which is presumably the joke.

**`x tools` is what puts the crowbar in the world.** The crowbar starts
nowhere (`room=-1`); TASK 15, `x tools` in Silverbeard's Smithy, is the action
that places it there. And you get exactly one visit to the smithy, because
`n` out of it is TASK 2, which teleports you to the Tunnel and throws the
sword on the floor. So `x tools` / `get crowbar` have to happen on that visit,
before `ask silverbeard about sword`.

## The two chains

**Dragon → smithy → sword.** The dragon blocks the tunnel south of the mines.
Drug her with the sleeping potion (TASK 50), which unlocks `EXIT room=7 S`
(`gateTask=50 wantDone=1`) into the smithy. Ask Silverbeard about the sword and
he hands over the Dragonbane. Walking out (TASK 2) drops it; pick it up again
(TASK 3) and `defeat dragon` (TASK 4) — which is a bluff, not a killing, and is
the flag the elf is waiting on.

**Elf → goblet → dragon.** With TASK 4 done, `ask elf about goblet` (TASK 59)
makes Armolas produce his iron box. `enter 3436 on lock` opens the combination
— the number is scrawled on a scrap of paper in the bird's nest at the top of
the Great Oak — but the lid has rusted shut, so `pry box open` with the crowbar
is a second step. Inside is a CD player; turning it off is what actually yields
the goblet. Give the goblet to the dragon and she goes back to her lair, which
is where the game ends.

## Shape of it

`goldens/valley_solution.txt`, 72 lines:

```
w / ask foreman about strike / ask foreman about demands
get gloves                                    Outside the Mine
e / get weeds                                 purple spotted leaf (Happy Valley)
n / u / x nest / x scrap / get pen / get bark the oak: 3436, the pen, the bark
d / s / e / get berry                         red globe berry
e / open file cabinet / get order forms / x list
get cup                                       out of the waste basket
e / turn on sink / fill cup / w
water plant / get flower                      white starflower
fill out forms / w / mail form / get crate
w / s / wear gloves / get wildflowers         demonflower (gloves WORN)
n / w / give crate to foreman                 hard hats end the strike
ask granny about potion / x scroll
give purple leaf / berry / wildflower / bark / starflower to granny
give cup to granny                            var0 == 5 -> sleeping potion
w / s / give potion to dragon
s / x tools / get crowbar                     the ONE smithy visit
ask silverbeard about sword / n / get sword / defeat dragon
ask dragon about treasure
n / e / e / s                                 back to the Thicket
ask elf about goblet / enter 3436 on lock / open box / pry box open
turn off cd player                            the goblet, at last
n / w / w / s / give goblet to dragon
s                                             -- WIN
```
