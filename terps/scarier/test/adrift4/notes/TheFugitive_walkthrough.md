# The Fugitive — walkthrough (**WIN, 656/666 — the reachable maximum**)

- **Author:** Renata Burianova (`arctica0@hotmail.com`), "The Garden of Life"
  (`geocities.com/shenanda976`), 2001–2006. A midnight phone call tells you
  they are coming for you; you have no idea who "they" are, and the whole game
  is spent running from them and assembling the proof that you didn't do it.
- **Engine:** **ADRIFT 3.90**
  (`xxd -p -l 12 games/Fugitive.taf | cut -c17-22` → `944537`).
- **Result:** **WON, 656 of 666**, which is **every point the game can actually
  award** — see "The ten points nobody can score" below.
- **Source:** `downloaded/TheFugitive_walkthrough.html` is the author's own
  page, but it is prose-only, gives no command list, and stops caring about the
  route the moment you reach the city ("go to the motel", "then you go to the
  factory"). The wired route is **derived from scratch** against the task dump:
  156 rooms, 130 tasks, 36 of them scorers.
- Row: `fugitive_solution.txt|Fugitive.taf|This is the proof of innocence|SCR_SKIP_WAITKEY=1`.
  The waitkey flag is mandatory — the intro, the train, the concert, the
  nightclub scene and the ending all paginate.

## The ten points nobody can score

There are 36 scoring tasks. 34 of them award 10, one awards 16, one awards 310;
that is 666 exactly. The route collects 35 of the 36. The one it cannot collect
is not a routing failure — it is unreachable in the shipped game:

```
TASK 73  where=2 room=-1 restr=0 rep=1  cmd=[look * mirror]   ACT type=4 v1=10
```

The rear-view mirror in a stolen car. The game also declares an input synonym
`look` → `l` (the author's way of making the one-letter form work everywhere),
and the synonym filter runs *before* task matching, so the pattern's literal
`look` can never be present when the matcher looks at it:

```
$ SCR_TRACE_FLAGS=512 ./scare ../games/Fugitive.taf   # 'look in mirror'
Printfilter: synonym "l in mirror"
```

`l in mirror` doesn't match `look * mirror`, and there is no other way in —
TASK 73 has no alternative command and no other task references it. The real
run400 rewrites the same way. So the ceiling is **656**, and this route hits it.

The last command before the win is a deliberate `score`, so the golden pins the
running total (346) as well as the ending: 346 + 310 for `break seal` = 656.

## Route decisions worth the digging

**Take the taxi, not your car.** The author's page lists five vehicles and
treats them as equivalent flavour; they are not. The car and the taxi both dump
you in the same street maze, but only the taxi lets you `fight` the driver
(TASK 27, +10) and keep his pistol — and only *with a pistol in hand* does the
punker ambush in the streets resolve as TASK 35 instead of TASK 33. TASK 35
leaves a dead punker carrying the can of beer that TASK 37 `[drink * beer]`
wants (+10). The car's only exclusive scorer is the dead mirror task, so the
taxi wins 20–0.

**Drink the beer before boarding the train.** `jump out` of the moving train is
TASK 29, and its first action is `ACT type=0 v1=0 v2=0 v3=0` — drop everything
held. The can goes with it. Open and drink it in the maze, one turn after you
take it.

**The train is on a three-turn cycle.** Board on the turn *after* "The train is
coming to station from the west." Two `wait`s put you in phase from where the
maze exit drops you; any change to the route length upstream re-phases it and
you get "The train is gone."

**In the woods, boots first, `undress soldier` last.** EVENT 26 ("death in
woods2") arms six turns after the undress, and the walk east-east to the jeep
plus the wait for the guard to be dealt with is five. Reversing the last two
commands of the soldier sequence kills you on the way out.

**`dive`, not `get coins`.** The fountains are the game's seed money and the
authored pattern is `[get * coin*]` — but the author also declares the synonym
`coins` → `money`, so `get coins` arrives at the matcher as `get money` and is
answered by the money-report library instead ("Take what?"). The ALTCMD `dive`
reaches TASK 81/82 untouched. Same class of collision as TASK 73, but this one
has an escape hatch.

**The casino comes after the gunshop.** `play` does not *add* winnings — it
`ACT type=3` **sets** your money variable to a random amount. On this seed 2181
becomes 56. It is still +10, so it stays in the route, but it has to run after
`buy bomb` (500) or the bomb is unaffordable.

**`sleep` needs you horizontal.** `lie down` alone puts you on the floor and
does not satisfy TASK 88; `lie on bed` does. Sleeping flips the clock to 23:00,
which is the switch for the entire night half of the city — bar concert,
nightclub, casino, Thel, the shadow at the church — and turns the day half
(gunshop's daytime trade, restaurant, bookstore, morgue) off. Everything you
mean to buy or sell has to be done before you sleep.

**The Thel scene teleports you.** `talk to thel` in the library drops you on
Riverside Road <2>, not where you were. Walk back to Outside the library for
`x thel` (+10) before moving on.

**Do the church interior before `unlock`.** `unlock` at Outside the church is
+10 and arms EVENT 46, which kills you in the cemetery five turns later.
Shovel + dig + score + seal is four. Go inside first for the priest scene
(TASK 121, the +16), come out, *then* unlock.

## Where the points are

| # | command | room | task | pts |
| --- | --- | --- | --- | --- |
| 12 | `call 777-777` | In the phone booth | 13 | +10 |
| 13 | `call random` | In the phone booth | 14 | +10 |
| 14 | `call police` | In the phone booth | 15 | +10 |
| 15 | `call mother` | In the phone booth | 16 | +10 |
| 16 | `call lawyer` | In the phone booth | 18 | +10 |
| 19 | `fight` | In the taxi | 27 | +10 |
| 27 | `drink beer` | the street maze | 37 | +10 |
| 48 | `get boots` | the woods | 68 | +10 |
| 49 | `undress soldier` | the woods | 67 | +10 |
| 55 | `wear uniform` | — | 70 | +10 |
| 56 | `wear boots` | — | 71 | +10 |
| 64 | `dive` | By the fountain <1> | 81 | +10 |
| 67 | `sell rifle` | Inside the gunshop | 95 | +10 |
| 68 | `sell knife` | Inside the gunshop | 93 | +10 |
| 69 | `sell pistol` | Inside the gunshop | 94 | +10 |
| 78 | `buy food` | the restaurant | 92 | +10 |
| 84 | `buy map` | Inside the bookstore | 83 | +10 |
| 91 | `dive` | By the fountain <2> | 82 | +10 |
| 106 | `call mother` | the second booth | 17 | +10 |
| 107 | `call lawyer` | the second booth | 19 | +10 |
| 127 | `pay` | Inside the motel | 97 | +10 |
| 129 | `unlock 13` | Upstairs | 98 | +10 |
| 131 | `watch tv` | In your room | 99 | +10 |
| 133 | `sleep` | In your room | 88 | +10 |
| 152 | `accept` | Inside the gardens | 112 | +10 |
| 158–159 | (the concert fires by itself) | Inside the bar <1> | 96 | +10 |
| 169 | `show amulet` | Inside the nightclub | 116 | +10 |
| 180 | `x thel` | Outside the library | 119 | +10 |
| 188 | `show amulet` | Inside the gunshop | 108 | +10 |
| 189 | `buy bomb` | Inside the gunshop | 107 | +10 |
| 200 | `play` | Inside the casino | 85 | +10 |
| 213–214 | (Eddie kills the priest) | Inside the church | 121 | **+16** |
| 216 | `unlock` | Outside the church | 123 | +10 |
| 219 | `dig grave` | In the cemetery | 122 | +10 |
| 221 | `break seal` | the grave | 124 | **+310** |
| — | `look in mirror` | any drivable car | 73 | *+10, unreachable* |

## Shape of it

`goldens/fugitive_solution.txt`, 221 lines:

```
stand up / get clothes / wear clothes / out          the apartment, on a timer
enter stairway / down ×3 / west / out                down to the street
in / call 777-777 / call random / call police
     / call mother / call lawyer / out               the booth, five calls
get taxi / fight / get pistol / out                  the driver's pistol
south ×2 / east / get beer / open can / drink beer   the punker ambush
west / south / west / north / west ×2 / south
     / east / south                                  out of the maze
wait / wait / get in train / jump out                the three-turn cycle
x ground / get branch / se                           the woods
x soldier / get pistol / get knife / get rifle
     / get boots / undress soldier                   boots BEFORE undress
east / east / get in jeep / wait / out               the jeep, past the guard
wear uniform / wear boots                            +20, and it disguises you
--- DAY ---------------------------------------------------------------
(fountain <1>) dive                                  the seed money
(gunshop) sell rifle / sell knife / sell pistol      2250
(restaurant) buy food / talk to bruce                Bruce opens the morgue
(bookstore) buy map
(fountain <2>) dive
(morgue) x dead body / get ad / read ad
(booth) call mother / call lawyer
(motel) pay / up / unlock 13 / in / watch tv
     / lie on bed / sleep / answer                   the clock goes to 23:00
--- NIGHT -------------------------------------------------------------
(gardens) talk to bruce / accept                     the job
(bar) in / out                                       the concert fires itself
(nightclub) show amulet / talk to thel
(library) in / talk to thel                          teleports you to Riverside
north / west / x thel                                walk back for the +10
(gunshop) show amulet / buy bomb                     500, so this comes first
(casino) play                                        `play` SETS your money
(factory) in                                         the bomb goes off by itself
--- ENDGAME -----------------------------------------------------------
(church) in / wait / out                             the priest, +16
unlock                                               arms the 5-turn death
south / get shovel / dig grave / score / break seal  — WIN, 656/666
```

The factory is the only place in the game where doing nothing is the solution:
walk in with the bomb and the "nuke 'em" event runs on its own. Likewise the
bar — the concert is a room event, and standing in the room for one turn is the
whole of it.
