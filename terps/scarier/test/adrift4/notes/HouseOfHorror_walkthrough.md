# House Of Horror — walkthrough

- **Engine:** ADRIFT 3.90 (`hhorror.taf`, 51,820 bytes, from
  `https://www.adrift.co/files/games/hhorror.taf`). **36 rooms, 7 NPCs, 125
  tasks, 46 events, 14 variables.** Max score 155.
- **Result:** ★ **WON at 145/155** — and **145 is the ceiling**. The last ten
  points are an author bug, not a missed move; see below.
- **Solution:** `goldens/hhorror_solution.txt` (144 commands, zero parser
  failures). Golden blessed, in `run_v4_walkthroughs.sh` with
  `SCR_SKIP_WAITKEY=1`. Win marker: `It has been a long and frightful night`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`,
  `SCR_DUMP_OBJLOC` and play.

## What it is

A treasure hunt in a haunted house. You arrive on the Driveway at night, open
the front door, and have thirty-six rooms to loot before driving away. Nine
treasures at ten points each, seven monsters worth five or twenty apiece, and
one shot from one gun.

## The ten points you cannot have

Sixteen `ACT type=4` in the file and that is the whole score:

| Task | Where | Points |
|---|---|---|
| T21 `give poisoned cheese to rats` | Wine Cellar 13 | +5 |
| T27 `throw molotov` | Toy Room 17 | +5 |
| T36 `pour bleach on monster` | any | +5 |
| T40 `blow whistle` | Attic 27 | +5 |
| T49 `pour weedkiller on plant` | Conservatory 11 | +5 |
| T54 `pour holy water on skeleton` | Hidden Room 28 | +20 |
| T81 `fire blunderbuss` (zombie) | any | +20 |
| T101–T109 | one treasure in room 35 | 9 × +10 |

25 + 40 + 90 = 155. Each of T101–T109 is a `RESTR type=0` on its own object
with `v2=0` ("in room") and `v3=36`, i.e. room 35, *Home Free!* — **except
T109, the bag of doubloons, whose `v3` is 0**. The `var3 == 0` arm of case
0/6 in `restr_object_in_place` (`screstrs.cpp`) does not test a room at all;
it tests `position == OBJ_HIDDEN`.

The doubloons start hidden (`OBJLOC obj=11 pos=-1`) and T81 moves them **onto**
the zombie's corpse (`ACT type=0 v1=14 v2=3 v3=1` — onto surface 0). So from
the instant they become obtainable they stop being hidden, and T109 can never
pass. Leaving them hidden — never shooting the zombie — *does* score the +10,
but forfeits T81's +20, so that trade is ten points worse. 145 is the ceiling
and the route reaches it. The same shape as the +2 stranded behind an EndGame
action in *Three Monkeys One Cage*.

## How the ending works

T110 `* drive *` in the Driveway (room 29) moves the player to room 35, moves
**all held objects** there (`ACT type=0 v1=0` = all held), and moves the car.
The treasures never have to go *into* the car; carrying them is enough.

Events 25–33 are `starter=3 startTask=111` (1-based → T110) with
`time1=time2=1`, so the nine scoring tasks run on the turn *after* `drive`,
and EVENT 43 `[Delay]` runs T120 `Finish` — the file's only `ACT type=6`,
`v1=0` = win — on that same turn. The route therefore needs one real turn
after `drive`; it uses `wait`.

`score` and `i` are meta-commands and do **not** tick events, which is why the
`score` two lines from the end still reads 65: the +80 has not happened yet,
and once it does the game is over, so 145 is never printed. WINTEXT is
non-empty, so `Congratulations!` is suppressed and the transcript ends on the
game's own text.

## Four order traps

**1. The ghost robbery, turns 5–8.** T48 `GHOST MOVING STUFF` is `rep=0`,
gated on `RESTR type=3 v1=6` (the ghost is in your room), and its action is
`ACT type=0 v1=0 v2=1 v3=1` — move ALL HELD to a random room of group 1. It
fires exactly once, the first time you share a room with the wandering ghost,
and it scatters your entire inventory across the house. The route walks
straight up to Hallway<4> (room 21) carrying **nothing**, meets the ghost
there on turn 7, and picks the torch up off the floor afterwards. Spend it
early or it happens later with a full pack. (This cost a whole exploratory
run: the torch turned up lying in a hallway and the cheese simply ceased to
exist.)

**2. The molotov has an eight-turn fuse.** `light molotov` at the Kitchen hob
(room 9) starts EVENT 1, which runs T26 `KILLED BY MOLOTOV` if you are still
holding it when the timer expires. Kitchen → Toy Room is 9 → 5 → 4 → 1 → 15
→ 17, five moves, so throwing on the sixth turn is the entire margin. Make
the cocktail first (`use vodka with cloth`), light it last.

**3. Do not waste the loaded blunderbuss.** `* fire * blunderbuss *` matches
seven tasks and T81 does not out-rank the rest by room:

- **T70 / T76 — room 11, the Conservatory — have no NPC restriction at all**,
  so firing anywhere in the Conservatory silently burns the shot;
- T77 (room 13, rats alive) and T78 (room 27, bats alive) shadow T81, so kill
  those two first — the route does;
- T74 / T79 / T80 need T85/T86 `hit door with hammer`, never done here.

The zombie wanders a 48-step path. The route meets it on the Landing (room 15)
coming down from the Bathroom. If a run diverges, fire wherever you next meet
it — anywhere except room 11.

**4. The Conservatory will not give up the hammer.** T35 blocks `take` in room
11 until T49 (`pour weedkiller on plant`) is done, and trying costs health.
The weedkiller is in the Laboratory, behind the iron-locked Wine Cellar door,
behind the rats. So the cheese chain gates the hammer, which gates
`break wall`, which gates the Hidden Room and its +20.

## Capacity is exactly ten objects

The endgame inventory has to be nine treasures plus the torch, so every tool
is dropped the moment it is spent: the hook after `open hatch`, the bible
after `bless water`, the whistle after the bats, the holy water after the
skeleton, the plunger after the toilet, the iron key and the sledge hammer in
the Bathroom, and the blunderbuss on the turn after it is fired.

## The chain

```
open door (Porch)                      seeds the house
hook (Master 24)      -> open hatch (21)         -> Attic 27
cheese (Landing 15) + poison (Closet 25)
     -> poison cheese  (which also yields THE BOTTLE)
     -> rats (+5), and unblocks 13 -> 14
bottle -> turn tap / fill bottle (Toilet 6)      -> water
bible (Games 16)      -> bless water             -> holy water
open window (En Suite 26) opens 26 <-> 34 Ledge  -> Veg Patch 33: iron key
iron key -> unlock door (Wine Cellar) -> Laboratory 14: braclet, weedkiller
weedkiller -> plant (+5) -> sledge hammer, and opens 11 <-> 31 Garden
     -> Shed 32: dish cloth
vodka (Music 2) + dish cloth -> use vodka with cloth -> cocktail
     -> light molotov (Kitchen) -> throw molotov (Toy Room) (+5)
whistle (Library 8)   -> blow whistle (Attic) (+5)
hammer -> break wall (Attic) -> Hidden Room 28: skeleton (+20), bleach
bleach -> pour bleach on monster (from Hallway<3> 19) -> Bathroom 20
     -> unblock toilet with the plunger (Guest<2> 22) -> the gold bar
say live (Lounge 3)   -> the blunderbuss appears in the fireplace
ball (Games 16)       -> put ball in blunderbuss  -> the zombie (+20)
```

## The map

`*` marks a gated exit.

```
29 Driveway -N- 0 Porch -N- 1 Entrance Hall
 1: N->4  E->3 Lounge  W->2 Music  U->15 Landing
 3 Lounge: N->7 Dinning
 4 Hallway<1>: N->5  E->7  W->6 Toilet
 5 Hallway<2>: N->11 Conservatory  E->9 Kitchen  W->8 Library
 8 Library: N->10 Study  *brass key   (EMPTY)
 9 Kitchen: N->12 Meat Locker  S->7  D->13 Wine Cellar
11 Conservatory: N->31 Garden  *T52 (until the plant dies)
13 Wine Cellar: S->14 Laboratory *iron key AND *T13 (rats)  U->9
                IN->30 Hole *T7 dig   (EMPTY)
31 Garden: E->33 Veg Patch  W->32 Shed  S->11
33 Veg Patch: U->34 Ledge      34 Ledge: S->26 *T3   D->33
15 Landing: N->19  E->17 Toy  W->16 Games  D->1
19 Hallway<3>: N->21  E->20 Bathroom *T82  W->18 Guest<1>
18 Guest<1>: N->22 Guest<2>
21 Hallway<4>: N->24 Master  E->23 Spare  W->22  U->27 Attic *T5 hatch
23 Spare: S->20 *T83
24 Master: E->26 En Suite  W->25 Walk in Closet
26 En Suite: N->34 Ledge *T3 (open window)
27 Attic: S->28 Hidden Room *T6 break wall   D->21
```

The Study (10), the Hole (30) and the Attic itself hold nothing, so the brass
key, the shovel and `dig dirt` are all optional and the route skips them.

## Notes

- The nine treasures are the diamond ring (Music 2), the silver candlestick
  (Library 8), the silver necklace (Guest<2> 22), the property deed (Kitchen
  9), the bundle of money (En Suite 26), the emerald ring and the gold bar
  (Bathroom 20 — the bar is *inside* the toilet), the gold braclet
  (Laboratory 14) and the bag of doubloons (on the zombie's corpse).
- Two objects are called `ring`; the parser resolves `take ring` in the Music
  room to the diamond one and `take emerald ring` in the Bathroom to the other.
- T111–T119 (`-1` … `-9`) look like a second scoring pass but only zero the
  nine tracking variables; they are driven by the immediate events 34–42 and
  are restricted on the negated "inside container" form (`v2=10`) against the
  car. Nothing ever reads those variables back.
- `pour bleach on monster` is `where=2` (a room list, not a single room) and
  works from Hallway<3> without entering the Bathroom, which is just as well
  since T82 keeps the Bathroom shut until the monster is dead.
