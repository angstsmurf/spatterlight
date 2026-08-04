# A Fine Day For Reaping — walkthrough (**WIN, all five souls, 73 moves**)

- **Game:** *A Fine Day for Reaping* by James Webb (aka revgiblet), IFComp 2007,
  `Ver 1.0 2007`. You are Death. Five names are on today's list and none of them
  wants to go quietly.
- **Engine:** **ADRIFT 4.00** — the file's 14-byte version header is
  `3c 42 3f c9 6a 87 c2 cf 93 45 3e 61 39 fa`, which is `V400_SIGNATURE`
  verbatim (`sctaffil.cpp:55`). 54 rooms, 247 objects, **278 tasks**, 16
  variables, 17 events.
- **Result:** **WIN** — all five souls reaped, the five per-soul epilogues
  print, then *"Life is good for Death. Today has, all things considered, been
  a fine day for reaping."* / *"Congratulations!"*
- **Score: there is none.** `score` answers *"Your score is 0 out of a maximum
  of 0. (0%)"* at every point in the game, so the corpus measure here is the win
  itself.
- **Harness row:**
  `afdfr_solution.txt|AFDFR.taf|Life is good for Death.|SCR_SKIP_WAITKEY=1`,
  PASSing golden.
- **Sources:** `downloaded/AFineDayForReaping_walkthrough.txt` (the author's own
  prose walkthrough, which lists *every* branch rather than one route) and
  `downloaded/AFineDayForReaping_clubfloyd.html` (the ClubFloyd session of
  2010-04-17 — a winning oracle, but 713 commands of group flailing with
  `save`/`restore`/`undo` mixed in, so unusable as a replay).

## There is no canonical route — the author wrote 2–3 solutions per soul

This is the unusual thing about the game and the reason the route below had to
be *chosen* rather than transcribed. The author's walkthrough does not give a
walkthrough at all; it gives a menu:

| Soul | Where | Solutions the author documents |
| --- | --- | --- |
| Lord Nigel McWorthington | snowdrift, Nepal | **(1) dig him out with the repaired shovel**; (2) repair the solar heater in 5,500 AD with the Manchester magnet and melt the drift |
| Ernest Busset | lost in time | **(1) ride the time machine to 10,097 BC**; (2) shovel up the skeleton key in Fada, blow the Follina cave open with Area 51 explosives |
| Jimiyu Wangai | Wajir, Kenya | **(1) read the chess guide and beat him**; (2) go to 4,002 BC and rig the rules so Black starts; (3) marry off his granddaughter — gown from the costume shop *plus* a pizza voucher from Nantes |
| Agathe Laurent | Paris hotel, room 247 | **(1) set off the smoke alarm with the lighter**; (2) sharpen the lucky-dip saw and cut through the ceiling of room 147; (3) get a Magnifipizza uniform and lure her off the bed |
| Splong5b | autopsy room, Area 51 | (1) meshomatic from 5,500 AD → crawl the air ducts; **(2) alien mask from the costume shop**; (3) dry-cleaning mix-up + "Blue Sparrow" false papers |

The ending prints a *different* epilogue paragraph per soul depending on which
branch you took, so no single run can see all of them. The route below takes
the bold entries: four "way 1"s plus the mask for Splong5b (the mask is cheaper
than the meshomatic, which would need a second round trip in the time machine —
and Ernest's shoe, the price of the mask, is picked up in Manchester anyway).

Between them the four gadget puzzles the route *does* exercise cover the
engine's interesting bits: a two-part assembly consumed by a third implicit
object (the shovel), a repair gated on a carried item plus a read-once
document (the time machine), a numeric-command task (`10097`), and a
state-flipping button (AD/BC).

## The engine facts, from `SCR_DUMP_TASKS`

* **Win** = TASK 6, fired by EVENT 2 when the variable `soulsreaped` reaches 5.
  Each `reap soul` task does `ACT type=3 v1=1 v2=1 v3=1` — increment
  `soulsreaped` (index 1) by 1.
* **Loss** = TASK 5, fired by EVENT 1 when `timea` reaches 47. EVENT 0 bumps
  `timea` once every 15 turns, so the "twelve hours" the game keeps mentioning
  are a **~705-turn budget**. This route spends 73 of them: probing
  `x hourglass` on the turn before the last reap still answers *"The grains of
  sand tell you that you have ten hours left to complete your contract."*
  The timer is real but it is not a design constraint on the route.
* **The horse only listens in hub rooms.** Every travel task
  (`say <place> to horse`) carries
  `WHERE_ROOMS=[5 6 7 13 17 25 34 39 40 41 42 46 51]`. From anywhere else you
  get *"No-one pays any attention to you."* — which is why the route walks back
  out of the Area 51 corridor to the Storage Cupboard (25) before every
  departure, and out of the Laboratory after the 10,097 BC trip.
* **Arrival auto-moves are `rep=0`.** Landing in Kenya the first time runs
  TASK 70, which walks you straight into Jimiyu's hut. The *second* Kenya
  arrival leaves you in the Village and needs an explicit `n`. Same shape for
  the other destinations.
* **`10097` is not gated on the clue.** TASK 195 (`cmd=[[10097/10,097]]`) has
  exactly two restrictions: TASK 192 done (the coil is in the panel) and
  variable 10 (`ad`) == 1, i.e. the red button has been pressed to select BC.
  Reading the clue is optional flavour, so the route types the year directly.

## Three things that cost derivation time

### 1. `take tape` is refused on purpose

In the Manchester cellar:

> If you ever need it then you know where to find it.

The masking tape (TASK 95) is deliberately un-takeable. It is consumed
implicitly by TASK 182, `repair shovel` — which in turn requires TASK 94,
`x workbench`, to have run first. So the sequence is `x workbench` →
`repair shovel`, with the tape never entering the inventory.

### 2. The cellar clue is `x wreck`, not `search wreckage`

The author's walkthrough says *"search the wreckage of the time machine in the
cellar"*. Neither word works: `wreckage` is not a noun the game knows (`x
wreckage` → *"That didn't make any sense to me."*), and SEARCH on the machine
answers *"I don't understand what you want me to do with the time machine."*
The alias is `wreck` (`OBJNAME obj=36 [time machine] … alias=[wreck]`) and the
verb is EXAMINE:

> You rummage through the destroyed time machine. The only thing of note that
> turns up is the digital readout, which remains - surprisingly - undamaged.
> It is blinking 10,097 BC at you.

Cosmetic here, since the year can be typed without it, but it is the only way
to learn 10,097 in-game.

### 3. `SCR_SKIP_WAITKEY=1` is mandatory

The game opens on an interactive title screen (*"1 : Read Me First / 2 :
Credits and Thanks / 3 : Begin Reaping / Please Choose an Option."*) preceded
by a wait-for-key. Without the env var the harness stalls after
*"Loading game..."*. The first line of the solution is the menu answer `3`.

## The route

73 commands. `harness/afdfr_solution.txt`.

| # | Command(s) | What it does |
| --- | --- | --- |
| 1 | `3` | title menu → Begin Reaping |
| 2–3 | `e`, `read mail` | Hallway; the letter and **The List**. Do not drop it — it is the travel gate |
| 4–6 | `s`, `s`, `take scythe` | Stable; **no reaping without the scythe** (every `reap soul` task has `RESTR type=0 … obj1=[Scythe]`) |
| 7–11 | `say kenya to horse`, `s`, `e`, `take branch`, `w` | Wajir; the **sturdy branch** by the lake (shovel handle). The arrival auto-move drops you in the hut, hence the `s` first |
| 12–14 | `say nevada to horse`, `w`, `take blade` | Area 51 Storage Cupboard; the **shovel blade** |
| 15–19 | `e`, `n`, `n`, `push button`, `take lighter` | Security Room: the button **unlocks the Laboratory**, and the **cigarette lighter** is Agathe's whole puzzle |
| 20–22 | `s`, `s`, `w` | back to the Storage Cupboard — the horse won't travel from the corridor |
| 23–26 | `say manchester to horse`, `take shoe`, `x workbench`, `repair shovel` | Cellar; **Ernest's shoe** (currency for the mask), then workbench → tape → **shovel** |
| 27–31 | `u`, `s`, `x papers`, `n`, `d` | the Manchester living room and the **time machine plans** |
| 32–36 | `say san francisco to horse`, `e`, `n`, `x counter`, `take wire` | Hardware store; `x counter` reveals the **coil of wire**, then take it |
| 37–39 | `s`, `e`, `read chess guide` | Book shop — reading the guide is *the entire* Jimiyu solution |
| 40–45 | `w`, `w`, `s`, `give shoe to man`, `mask`, `n` | Costume shop; the shoe buys either the mask or the gown — take the **mask** |
| 46–47 | `say nevada to horse`, `wear mask` | the mask only works in Area 51; wearing it auto-lures **Private Kline** into the Storage Cupboard |
| 48–50 | `e`, `s`, `reap soul` | Autopsy Room — **soul 1/5: Splong5b** |
| 51–58 | `n`, `n`, `w`, `enter machine`, `repair machine`, `push red button`, `10097`, `reap soul` | Laboratory → inside the machine; the coil + plans repair it, the red button flips AD→**BC**, the year takes you to the jungle — **soul 2/5: Ernest Busset** |
| 59–62 | `enter machine`, `e`, `s`, `w` | home to 2007, out of the lab, back to the Storage Cupboard |
| 63–65 | `say nepal to horse`, `dig snowdrift`, `reap soul` | mountain pass — **soul 3/5: Lord Nigel McWorthington** |
| 66–69 | `say kenya to horse`, `n`, `play chess`, `reap soul` | second Kenya visit, so the auto-move is spent → explicit `n` into the hut — **soul 4/5: Jimiyu Wangai** |
| 70–73 | `s`, `say france to horse`, `use lighter`, `reap soul` | Paris, room 247; the lighter trips the smoke alarm, the sprinklers wash the chalk circle away — **soul 5/5: Agathe Laurent** → WIN |

## Reproducing

```sh
cd terps/scarier/adrift-walkthroughs
sh harness/run_v4_walkthroughs.sh afdfr     # PASS against the committed golden
```
