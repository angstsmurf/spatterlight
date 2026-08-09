# Wes Garden's Halting Nightmare — walkthrough / analysis

*Wes Garden's Halting Nightmare* by **Jubell** (ADRIFT 4, written for the ADRIFT
Spring Thing, 2010; "made with all of the unregistered Adrift limitations in
place"). A graphics‑heavy file (3.5 MB of embedded illustrations) but a small
game: **10 rooms, 25 tasks, 3 NPCs, 2 events**.

**Result: WON — 100/100, all 12 scoring tasks, deterministic (seed 1234).**

> **History.** This game carried an "UNWINNABLE — max 30/100, orphaned gold
> ring" verdict (banked 2026‑06, corrected 2026‑08‑02). That analysis claimed
> the severed hand holding the gold ring was never brought into play by any
> task, event, or character walk. It was wrong about the *event*: the dump's
> `EVENT` lines print **raw 1‑based** `.taf` fields, and event 1 — pointedly
> named **[Davidshand]** — has `o2=4->5, startTask=6`, which decodes (raw−1;
> `evt_move_object` dest: −1 hidden / 0 held / 1 player's room / else
> room = dest−2) to *object 3, the severed hand → room 2, the Waiting Room,
> started by task 5, `ring bell`*. Misread 0‑based, `o2=4` looks like the
> Journal — hence "no event reveals the hand". In play: ring the bell, wait
> one turn, and the receptionist's window opens and drops the hand (gold ring
> attached) with a fleshy thud. The original route rang the bell, shrugged at
> "nothing appears to happen", and never looked back. Same lesson class as
> the Mr Smith / Villains & Kings combat‑verdict reversals: decode the raw
> fields before declaring an object orphaned.

Solution file: `goldens/wes_ghn_solution.txt` (deterministic; the golden
`wes_ghn_solution.expected.txt` ends at "You've Won the Game!").

## The story / setup

Wes Garden, grieving his grandfather and (years earlier) his vanished parents,
is talked into his Uncle Copeland's meditation class and wakes inside **Mercy
Hospital**, a surreal afterlife‑ish nightmare. He can *summon the Soul Scythe*
at will (the game's combat verb) and must work his way to the Halting Chapel to
redeem the candy‑striper **Hope Endlessly** — one of the spirits of Pandora's
box — and send her home.

## Map (dump‑derived; in‑game compass labels are reliable here)

```
 Foyer(0) ─N→ Walkways(1) ─N→ Waiting(2) ─N→ Grand Corridor(3)
                                                 │  ├─E→ Halting Chapel(5)
                                                 │  └─W→ Core of Beauty(4) ─N→ Radiology(6)
                                                 └─N(optical scanner, key=eyeball)→ Medicine Cabinet(7)
                                                      └─N(door, key=vial)→ Maternaless Ward(8) → Magna Mater → …
```

The Foyer→Walkways step is one‑way (closely examining the balcony figure
destroys the path back).

## The 100‑point route (annotated)

```
talk to charity                (Foyer)      +10   then flee north (the patients turn hostile)
closely examine figure         (Walkways)   +5    opens the north door; the way back is destroyed
ring bell                      (Waiting)    0     "nothing appears to happen" — BUT it starts
                                                  event [Davidshand], length 1 turn
z                                                 the window opens; a bloody hand thuds down
take ring                                         the gold ring sits on the severed hand
summon scythe                  (Corridor)   0     your weapon
drink water                    (Core)       +5    Charity's Cupid‑and‑Psyche exposition follows
talk to hope                   (Chapel)     0     enables taking the candle
take candle                                 0     Hope turns hostile (one keypress mid‑scene!)
attack hope ×8                              +10   #Hopedies — she flees "I'll never return to that box!!!"
give ring and candle to fountain (Core)     +20   the Lovers' Fountain; drops the vial
take vial
talk to micheals               (Radiology)  0     required before the portrait
slash dr micheals portrait     (Corridor)   +5    NO period — "." splits ADRIFT input; yields the eyeball
unlock door with eyeball / open door / n          the optical‑scanner door
talk to charity                (Med.Cab.)   +10   …interrupted by Hope, reborn with the Stripper Sword
attack hope ×6                              +10   #Hopedies2 — Charity fights at your side
unlock door with vial / open door / n             the door at the base of the figure
knock on door                  (Ward)       +5    splash from the Cabinet: La Virgencita key appears
                                                  THERE (and the black shrine appears in the Chapel)
s / take key / s                                  fetch the key from the Medicine Cabinet
w / n / take scalpel / s / e                      Hope #2 dropped the Woebegotten scalpel in RADIOLOGY
n / n / unsummon scythe                           the Magna Mater door refuses the scythe
unlock door with key / open door
walk through magna mater door               +5    the birth/mother vision (eats ~8 keypresses);
                                                  returns you to the Ward holding the mother's gift
s / s / s / take prudence      (Waiting)          "a friend has left you something in the waiting room"
n / e                          (Chapel)
give sigh prudence to hope                  +5    Hope fades into stars; her Spirit remains
take spirit
put hope's spirit into box                  +10   WIN — "You've Won the Game! Congratulations!"
```

Score checkpoints: 15 after `take ring`, 30 after Hope #1, 50 after the
fountain, 55 after the portrait, 75 after Hope #2, 85 after the Magna Mater
walk, 100 at the win.

## Key mechanics and footguns

* **The bell is the whole game.** Task 5 (`ring bell`) prints only flavour
  itself, but starts event 1 [Davidshand] (Time1=Time2=1). On the event's
  finish, one turn later, the severed hand (object 3, authored Hidden) is
  moved to the Waiting Room, carrying the gold ring (object 2, authored
  `on` the hand). Everything else in the old writeup's dependency chain
  (fountain → vial → Radiology → eyeball → Medicine Cabinet → Ward →
  Prudence → spirit → win) was already verified to work.
* **Hope legitimately dies twice**, and the two +10 kill tasks are gated to be
  mutually exclusive per kill: `#Hopedies` (task 10) requires the Medicine
  Cabinet `Talk to Charity` (task 15) *undone*, `#Hopedies2` (task 17)
  requires task 15 *and* the fountain done. Task 15 respawns her — savage,
  Stripper Sword in hand — and Charity joins the second fight on your side.
  Both fights are honest Battle System combat (Soul Scythe accuracy 50 vs her
  agility 30; kill #1 falls on the 8th blow, kill #2 on the 6th under the
  seeded RNG). No combat assist.
* **Parser traps.** The attack verb is plain `attack hope` (the grammar
  rejects `attack hope with scythe`); the portrait must be slashed as
  `slash dr micheals portrait` **without the period** (ADRIFT treats `.` as a
  command separator, so `slash dr. micheals portrait` parses as two garbage
  commands); the scythe must be summoned for the portrait and both fights but
  **unsummoned** before `walk through magna mater door` (a task restriction).
* **Fetch‑quest choreography after Hope #2:** the scalpel she drops lands in
  *Radiology* (not at your feet), and `knock on door` in the Ward materialises
  the La Virgencita key back in the *Medicine Cabinet* (the splash you hear).
  Both detours are mandatory: the Magna Mater door needs the key to unlock and
  the scalpel in hand to enter.
* **Keypress sinks.** The graphics‑heavy scenes eat keypresses at specific
  points; the solution file's blank lines are exact (intro 12; post‑`talk to
  charity` 6; post‑`closely examine figure` 3; post‑`ring bell` 1; post‑`drink
  water` 20; post‑`talk to hope` 6; **one between `take candle` and the first
  attack** — miss it and attack #1 is swallowed and Hope batters you to death;
  post‑Micheals 5; post‑portrait 2; post‑Charity‑in‑Cabinet 7; two after Hope
  #2 flees; **~8 inside the Magna Mater vision**; 3 after giving the
  Prudence).
* **`open altar` (task 22) is a trap, not the win.** It re‑arms Hope as a
  stamina‑90 boss and can execute `#killplayerHope`; the win needs only
  `give sigh prudence to hope` → `take spirit` → `put hope's spirit into box`
  (task 24: EndGame/win, +10). The route skips the altar entirely.

## Reproduce

```
sh harness/build.sh
sh harness/run_v4_walkthroughs.sh wes_ghn        # PASS = transcript matches golden
```

Deterministic: 100/100 and "You've Won the Game!" on every run.
