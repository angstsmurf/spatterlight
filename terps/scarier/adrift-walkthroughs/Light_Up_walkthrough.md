# Light Up — walkthrough

*Light Up: An Interactive Horror* by **TDS** (ADRIFT 4, file
`light_up_4summer_comp.taf`). A bleak five‑chapter horror: a grieving father
follows his missing son into another world and is forced through six "trials"
and a combat gauntlet by a hidden people who think he fulfils a prophecy.

**Result: WON — full game, deterministic. Banked score 73; maximum reachable
75.** Solution `harness/light_up_solution.txt`. Reproduce with
`sh harness/play.sh games/light_up_4summer_comp.taf harness/light_up_solution.txt`
(win marker: **"THE END / Congratulations!"**). Uses the Battle System, but
combat is properly configured here (non‑zero accuracy), so **no combat‑assist
is needed** — unlike Azra / V&K / Mr Smith / To‑Hell, this game's fights work.

The game's own `MaxScore` field is left at 0 (so it always prints "out of a
maximum of 0"); the real scoring is 16 story tasks + a difficulty modifier (see
the score table at the end).

## Map (37 rooms, 5 chapters)

- **Ch 1 – The House:** Driveway → Outside House → Den → Corridor → (Kitchen /
  Bedroom) → Steps → **Secret Room** → (climb into the machine box).
- **Ch 2 – The Field:** a 10‑tile field maze, F1→F10, with a suffocation timer.
- **Ch 3 – The Village:** six numbered Cells, each a "trial" run by a
  disembodied voice. Pass all six.
- **Ch 4 – The Death:** a free‑for‑all combat gauntlet (rooms 24–32). Kill **10**
  of the Ozgat/Riven/Higher enemies → teleport to the Waste Land (+15).
- **Ch 5 – The Waste Land:** walk north/east to meet **Arkot**; the ending fires
  automatically a few turns later.

## Chapter 1 — The House

(The intro shows two `[Press any key]` screens — send a blank/any line for each.)

1. `open trunk`, `get flashlight` — the flashlight is in the car trunk.
2. `north` to the boarded‑up house. `get rock` (one lies on the grass).
3. `break window` (**+2**) — needs the rock; the door is boarded, the window is
   the way in. You drop into pitch darkness — `turn light on` (the flashlight).
4. In the **Den**: `look under chair` (**+2**, finds a **saw**); `take saw`.
5. `ne` to the **Kitchen**: `open refrigerator`, `take vial` (a blue vial — fuel
   for the machine later). `sw` back, `north` to the **Corridor**.
6. `saw door` (**+2**) — saws the boarded north door open.
7. `west` to the **Bedroom**: `dig` (**+2**, finds a **laptop**); `take laptop`.
8. `east`, `open door`, `north` to the **Steps**, `down` to the **Secret Room**.
9. **Laptop +2 (optional but free):** the car's licence plate reads **910‑CCC**
   (`x plate` back in the driveway). At the laptop: `use laptop`,
   `enter password 910-CCC` (**+2**). Shane's e‑mail reveals the keypad code is
   "the first three digits of the laptop code plus two obvious digits" = **91020**.
10. `91020` (**+2** — keypad code), `put vial in machine`, `press button`
    (**+3** — the big metal box opens). `get in box` (**+5**) — the box closes
    and ports you to the field.

## Chapter 2 — The Field (suffocation maze)

The air here is poison; you have only a handful of turns before you choke to
death. The exit chain is **N,N** then **N,N,E,E** then **E,N,N** (F1→F10).

1. From F1 go **`north`,`north`** to **F3** and `dig` — you pull up a **gas
   mask**. `wear mask` immediately (the choking stops).
2. `north`,`north`, then `east` (**+2** "forge eastward") to **F6**, `east` to
   **F7** — a naked woman is chained here.
3. You need an **axe** to free her: continue `east` to F8, `north` to **F9**
   (`take rock` — a loose rock for later), `north` to **F10** and `dig` (the
   **axe** is buried at the gate). Return `south`,`south`,`west` to F7.
4. `break chain` (**+2**) — frees the woman; she explains you must find a
   **medallion** to win, then vanishes. A few turns later **a bird flies in from
   the east** carrying the medallion — wait (`look` ~5×) for it, then
   `throw rock at bird` (**+2**). The bird drops the medallion back at **F1**.
5. Walk south to **F1** (`west`,`west`,`south`×4); the medallion and the woman
   are there. `give medallion to woman` — she snatches it and flees north. **Run
   after her** (`north`×4, `east`×3, `north`): when she catches up to you the
   ground swallows her and the medallion lands at your feet.
6. `take medallion`, `wear medallion` (the gate checks for it **worn**),
   `north`, `open gate` (**+5**) — into the cells.

## Chapter 3 — The Village (six trials)

Each cell auto‑opens after the previous via timed events; act on the prop that
appears, then wait for the trial to resolve. Getting a trial wrong = death.

- **Cell 1 – the bomb.** A **blue box** (a timer ticking inside) drops from the
  ceiling. `close blue box` — smothering the timer defuses it (**+3**). Doing
  nothing = the bomb explodes and kills you.
- **Cell 2 – the child.** `take blanket`, wait for the **red box** to fall,
  `open red box`, **`take child`** (you must hold it), then
  `use blanket on child` (**+3**). Do **not** "kill child" (death).
- **Cell 3 – the box.** A **purple box** appears; `open box` to advance.
- **Cell 4 – the name.** The voice asks your son's name: `his name is Shane`.
- **Cell 5 – the lighter (orbs).** A **blank box** holds seven coloured orbs.
  Assemble the weapon by nesting them by visibility (most‑visible contains all):
  `open blank box`, take all seven orbs, then
  `put violet orb in indigo orb` → `indigo orb in blue orb` → `blue in green` →
  `green in yellow` → `yellow in orange` → `orange in red`, then `shout`
  (**+3**). Wrong order = death.
- **Cell 6 – Chip.** A **green box** holds a real lighter (the weapon).
  `open green box`, `take lighter`, then `attack chip` twice (**+5**). The
  lighter is *thrown*, so `take lighter` between swings (here Chip dies in two
  hits because the lighter boosts your Strength/Accuracy).

## Chapter 4 — The Death (combat gauntlet)

You wake among the **Death** rooms (24–32) and the voice says "move north, find
Arkot." The mechanic: kill **ten** of the Ozgat / Riven / Higher enemies (each
death increments a hidden counter); when it reaches 10 you are teleported to the
Waste Land for **+15**. The wandering Lina/Scotty/Tippant/Donna/Chip don't count
toward the ten, but — usefully — **all the NPCs are hostile to each other and
fight among themselves**, so the counter climbs even from kills you didn't land.

Route (counting enemies in brackets): 24 →`north`→ 25 *(Ozgat)* →`east`→ 26
*(Riven)* →`north`→ 28 *(Ozgat, Riven)*; detour `east` to 27 to
`take double-lighter` (a stronger weapon) then `west` back; →`west`→ 29 *(two
Rivens)* →`north`→ 30 →`north`→ 31 *(Higher)* →`northeast`→ 32 *(Ozgat, Higher,
Riven)*. In each room hammer `attack ozgat` / `attack riven` / `attack higher`,
`take lighter` between swings, and `rest` when stamina dips. The fight is
RNG‑driven but deterministic under the harness seed; the banked solution clears
the ten and pops **+15** → **Chapter Five**.

**Tip / difficulty:** type **`hard`** once in this chapter for **+15** to your
score (it trims your max stamina but you still survive, because the enemies
spend most of their turns killing each other). `easy` would add +250 stamina but
costs −15; `medium` is the neutral default. `rest` recovers stamina; the
narrator's "ask for battle tips" (`battle tip`) just prints advice.

## Chapter 5 — The Waste Land (the ending)

You land in Waste Land #1. `north`, then `east` to Waste Land #3, where **Arkot**
— the "Higher" who has been protecting you and who gave you the gift of
understanding the local language — addresses you. Just keep taking turns
(`look`); a scripted three‑step event chain plays out and Arkot's final
"gift" arrives: the box on the counter holds **your own severed head**.

**`THE END` — "Congratulations!"** — the win.

## Score table

| Source | pts |
|---|---:|
| break window / look under chair / dig (laptop) / saw door | +2 ×4 |
| enter password 910‑CCC (laptop) | +2 |
| 91020 keypad / press button / get in box | +2 / +3 / +5 |
| forge east / break chain / throw rock at bird / open gate | +2 / +2 / +2 / +5 |
| Cell 1 bomb / Cell 2 child / Cell 5 orbs / Cell 6 Chip | +3 / +3 / +3 / +5 |
| Chapter 4 "ten are dead" | +15 |
| **`hard` difficulty bonus** | **+15** |
| **Total reachable** | **75** |

### Note on the banked 73 vs. the 75 maximum

The banked `harness/light_up_solution.txt` scores **73**: every story point and
the `hard` +15, **except the laptop +2**. This is not a game limitation — the
licence‑plate/laptop puzzle works fine (it's documented above). The catch is
purely the deterministic harness: the Chapter‑4 gauntlet is a long RNG
free‑for‑all, and the two extra turns the laptop step inserts upstream reshuffle
the shared `erkyrath_random` stream enough to get the (stamina‑reduced, on
`hard`) player killed before the tenth enemy dies. A human player, who can see
the fight and rest/retreat adaptively, can collect the laptop +2 *and* clear the
gauntlet for the full **75**; the fixed turn‑list simply can't have both at once
without re‑searching the combat RNG. (On `easy` or `medium` the gauntlet is
comfortably survivable, but those difficulties forfeit the +15, so `hard`
without the laptop is the higher banked total.)

Faithful throughout — no engine changes were needed to win (SCARE reads the
non‑zero battle stats correctly; the win is the genuine `#ending2` EndGame task).
