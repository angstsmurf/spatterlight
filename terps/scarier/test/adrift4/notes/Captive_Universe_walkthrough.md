# Captive Universe — walkthrough

**File** `Captive.taf`, 74,568 bytes, ADRIFT **3.90** (XOR-obfuscated; the
version comes from the 14-byte header signature, not the size).
**Author** not recorded anywhere in the file; `games.manifest.tsv` carries only
the title, and the source is `https://www.adrift.co/files/games/captive.zip`.
The endgame text signs off *"Based on Captive Universe by Harry Harrison"* —
the 1969 novel whose premise this is: an Aztec village sealed in a valley by a
boulder the gods dropped in the only exit, five hundred years ago.

**Result: WON, 100 / 100** — the file's declared maximum — in **57 commands**.
The game confirms it: *"You scored 100 out of the maximum 100! That is 100% of
the game! Well done - you scored maximum points!"*

**Artefacts**

| What | Where |
|---|---|
| Route | `goldens/captive_solution.txt` |
| Transcript | `goldens/captive_solution.expected.txt` |
| Suite row | `harness/run_v4_walkthroughs.sh`, `captive_solution.txt\|Captive.taf\|You scored 100 out of the maximum 100!\|` |

No published walkthrough exists (checked Key & Compass, IF Archive, CASA), the
adrift.co zip ships no solution, and — unlike *Veteran Knowledge* and *The Lost
Tomb* — the author built no hint menu, so `SCR_DUMP_TASKS` prints not one
`HINT2=`. It was derived from the dump.

## Structure

62 rooms, 61 tasks, 49 objects, 2 NPCs, 19 events, **no variables**. The map is
three layers: the temple (cells, courtyard, inner temple), the valley outside
it (village, fields, trees, swamp, waterfall), and — inside the cliff — the
decks of a colony ship, which is what the valley actually is.

## 100/100 is provable

Nine `ACT type=4` awards and no other scoring action in the file:

| Task | What | Points |
|---|---|---|
| T1 | `hit lock with stone` | 10 |
| T11 | `north` out of the courtyard gate | 10 |
| T17 | via EVENT 2 or EVENT 3 — entering the cliff, either way | 10 |
| T19 | via EVENT 4 or EVENT 5 — getting through, either way | 10 |
| T24 | `use crowbar` on the throne | 10 |
| T33 | mother's tortilla (EVENT 15) | 10 |
| T36 | `eat tortilla` | 10 |
| T48 | `pull plug` | 10 |
| T50 | `put diamond on pedestal` | **20** |

8 × 10 + 20 = 100, the declared maximum, and the route fires all nine. There is
one tenth award and it goes the other way: **T3 `* hint *` is `ACT type=4
v1=-356`**, the author's joke penalty for asking for a hint, and it is the only
thing in the game that can move the score off the ceiling.

Ten `ACT type=6` actions: nine deaths (`v1=2`) and exactly one win, T50's
`v1=0`.

## The game is a clock, and the clock is one-shot

There are no variables; every gate in the game is either a task-done test or an
event. Breaking the cell lock (T1) starts **EVENT 0**, which runs T10 ten turns
later and kills you anywhere in the courtyard (rooms 5–10). Walking out of the
courtyard gate (T11) starts **four events simultaneously**:

| Event | Turn | Kills in |
|---|---|---|
| EVENT 13 [Closetoo arrest] → T29 | 8 | rooms 11–18 |
| EVENT 10 [Tree arrest] → T26 | 18 | rooms 17, 30, 31 |
| EVENT 11 [Field Arrest] → T27 | 18 | rooms 12–16, 18, 20, 21, 25–28, 34, 39 |
| EVENT 9 [Evening] → T13 `-nighttime` | 20 | — nightfall |

**All four are `restart=0`.** They fire once, at exactly that turn, and never
again — so the only thing that matters is where you are standing on turns 8, 18
and 20, and a route that survives those three instants can wander freely in
between. That is the whole difficulty of the first half: rooms **19, 40, 41 and
44** (up trees) and the swamp appear in no arrest task's `WHERE_ROOMS`, so the
answer is to climb something and sit still.

Two arrests are the opposite shape — `starter=1 restart=1`, i.e. they run every
single turn until their task's restriction stops passing:

- **EVENT 14 → T31** kills in the village (rooms 20, 21) while T13 `-nighttime`
  is *not* done.
- **EVENT 6 → T21** does the same in the inner temple (room 43).

So the village and the temple are lethal until turn 20 and safe after it, and
everything the second half of the game needs — the Smith, the grainhouse, your
mother, the throne — is in one of those two places. The first twenty turns are
not a puzzle with a solution, they are a wait.

After nightfall one more clock starts: **EVENT 12 [Serpent]**, `restart=1`,
`time1=3 time2=5` — a random 3-to-5-turn cycle that runs T28 and kills in the
stream (rooms 25, 26, 27). The route never goes there, which is why the seed
does not matter to this golden.

## Route

Escape (turns 1–8, against EVENT 0's ten):

```
get stone / hit lock with stone (+10) / n / n / open door / n / ne / n (+10)
```

`get stone` (T0) is the loose flagstone in the cell floor; T1 smashes the lock
with it and opens room 0's north exit (`EXIT gateTask=1`). Note the author's
door handling: there are **four separate `open * door` tasks**, T4–T7, each with
its own two-room `WHERE_ROOMS` and distinguished only by how many `*`s the
pattern carries (`open * door`, `open ** door`, `open *** door`,
`open **** door`). Only one of them can match in any given room, so the player
types `open door` and never notices.

Hide (turns 9–20):

```
e / e / u / u / x nest / get key / x string / x coin / z / z / z / z
```

Room 44 [Far too high up a tree] holds the magpie's nest, and the nest holds the
**small golden key** — the one the room 15 description says was "allegedly
stolen years ago by a magpie, probably for decorating its nest", which is the
game's one genuinely fair piece of foreshadowing. The blue string and the coin
are scenery: no task in the file refers to either.

**`Globals.WaitTurns` is 3 in this game**, so the four `z`s are *twelve* turns,
not four — turns 9 to 20, with nightfall landing on the last one. Measured the
usual way:

```
printf 'z\nquit\ny\n' | SCR_TRACE_EVENTS=1 ./scare Captive.taf 2>&1 >/dev/null \
  | grep -ac '^Event: ticking event 6:'      # -> 3;  `look` gives 1
```

Night (the village, the temple):

```
d / d / w / w / n / in / out / use crowbar / u / get corn / d / in
out / w / in (+10) / eat tortilla (+10)
out / s / s / s / e / unlock door / in / use crowbar (+10)
```

Both NPCs are pure event plumbing — neither is ever spoken to. EVENT 16 and
EVENT 15 are `starter=1 restart=1`, so T34 and T33 fire the instant you walk
into the Smith's hut and into your mother's house respectively: he hands over
the crowbar and asks you to rob the grainhouse for him, she hands over the
tortilla. EVENT 17 is the third of them and it is the payoff — walk back in
holding the grain and T37 swaps it for the rope.

The crowbar is the game's universal tool and it is used four times, in four
different rooms, under the same command: T32 takes the grainhouse door apart
(opening room 20's `up` exit, `EXIT gateTask=32`), T24 prises the diamond out of
the throne, T38 rips the grating out of the swamp, T39 levers the ledge's steel
door open.

The ledge, and into the ship:

```
out / w / n / tie rope to ledge / u (+10) / use crowbar (+10) / w
n / e / e / pull plug (+10) / n / put diamond on pedestal (+20)
```

Room 13 is "the gap between the temple and the cliff, the part of the cliff
where the vultures land… on a ledge about four storeys up. Always the exact same
piece of cliff." T14 lassoes it, T15 climbs, and EVENT 2 pays T17 the +10 for
arriving.

Inside the ship, one `n` from the Meat Room covers what the map draws as four
rooms: T41 chains **three `ACT type=1`s** in a single task (rooms 59, 58, 50), so
the passageway rooms exist on the map but are never seen. T42/T43/T44 do the
same in the other three directions.

Then `pull plug` (T48, +10) folds up the security field and opens room 54's
north exit (`EXIT gateTask=48`), and the diamond goes back into the hole in the
pedestal it was cut for — which is, of course, the ship's navigation key.

**Do not `push button` in either loading bay.** T46 covers rooms 52 and 53 and is
an `ACT type=6 v1=2` death: the Emergency Release opens the cargo door onto the
valley you are trying to leave.

## The finding worth carrying: an event that *un*-finishes a task

**EVENT 18 [Timedoor]** is `starter=3 startTask=40 affTask=40(fin=1) time1=1` —
started by T39 and pointed back at T39 with **`fin=1`, which un-completes it**
one turn later. In play:

```
> use crowbar
You attack the steel door with your crowbar. Twenty minutes of near exhaustion
later you manage to lever it open. The opening is to the west.

> look
[...]

> w
As long as the steel door is closed, you won't be able to get through.
```

T40 `west` is restricted on T39 being done, so **`w` must be the very next
command** — spend a turn on anything at all and the door "beeps, flashes a little
green light, and slides shut again". It is not a soft-lock: T39 is `rep=1`, the
+10 has already been paid by EVENT 4 and is kept, and `use crowbar` works again.
But a route that pauses to take stock on the ledge reads as a wrong solution.

The idiom is not rare — a `SCR_DUMP_TASKS` sweep of the whole `games/` corpus
finds `affTask …(fin=1)` in **nine files**: `Captive`, `Mangiasaur`,
`To_Hell_And_Beyond`, `Vendetta`, `humbug` (2), `losttombv2`, `the_pk_girl`,
`tra` (6) and `wrecked` (17), and in almost all of them the event points back at
its own `startTask`. So the shape is worth recognising on sight: **an `affTask`
pointing at its own `startTask` with `fin=1` is a task that undoes itself after
`time1` turns** — a one-turn door here, a re-arming trap in *Mangiasaur*, a
weather cycle in *tra*. It is never an engine bug, and the symptom is always the
same: a step that visibly succeeded stops counting as done a turn or two later.

## The shortcut this route deliberately does not take

The ship has **two entrances**, and the point pairs are wired so that either one
scores the same 20:

| Entrance | Scores T17 via | Scores T19 via |
|---|---|---|
| the ledge — `tie rope to ledge`, `u` (T15), `use crowbar` (T39), `w` (T40) | EVENT 2 | EVENT 4 |
| the swamp — `use crowbar` (T38) then `swim` (T18) in room 35 | EVENT 3 | EVENT 5 |

The swamp entrance needs **only the crowbar**. It skips the grainhouse, the
grain, the rope, the tie and the climb — the entire Smith sub-plot — for exactly
the same 100/100, and it is eight commands shorter. Verified: the run reaches
room 46 [Pump room] and finishes on the same score.

The committed route takes the author's designed path anyway, so the regression
covers both NPCs, all three chained Smith events and the timed door rather than
routing around them. Same call as *Salutations*, for the same reason.

## Nothing is left unreachable

Every award fires. The unfired tasks with endings on them are the nine deaths
(courtyard, the five arrests, the loading-bay button), and the only unfired
scoring task is T3's −356 hint penalty, which is not a point you would want.
