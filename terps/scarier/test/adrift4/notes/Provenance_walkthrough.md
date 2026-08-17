# Provenance — walkthrough (**WIN, 260/300**)

- **Author:** Corey W Arnett, v1.12.16, © MMV (the shipped `provenance.taf` is
  dated 02-10-2006). You inherit a house from a man you have never met, and the
  estate turns out to be a confession.
- **Engine:** **ADRIFT 4.00** (`xxd -p -l 12 games/provenance.taf | cut -c17-22`
  → `93453e`).
- **Result:** **WON** — "Look for PROVENANCE II in the summer of 2006!!! / You
  scored 260 out of the maximum 300!". Wired as
  `provenance_solution.txt|provenance.taf|Look for PROVENANCE II in the summer of 2006!!!|SCR_SKIP_WAITKEY=1`
  (no seed pin — see the EVENT 7 section below; the row briefly carried
  `SCR_SEED=2` until §10 of `RUNNER_TESTS_TODO.md` was measured and ported).
- **Source:** the author's own `walkthrough_short.txt` from `provenance.zip`
  (copy kept as `downloaded/Provenance_walkthrough_short.txt`), with two
  deliberate departures, both forced — see below. The archive's
  `walkthrough_long.txt` is a full transcript recorded 12-12-2005, i.e. against
  an **older build than the shipped .taf**, so it is kept
  (`downloaded/Provenance_walkthrough_long.txt`) as a wording reference but is
  not a reliable oracle.

## 260/300 is a win

The readme settles this outright:

> It is possible to win the game without scoring all the possible points. The
> score is just meant as a general guide. The goal of the game is not to score
> the maximum number of points, it is to collect the specified items and take
> them where you are instructed to.

The win condition is TASK 460 (`* blow * whistle *`, where=1 room=100, 19
restrictions): sixteen named objects have to be **on** object 33, the altar, and
the bosun's whistle has to be in hand. The list the game hands you under the
welcome mat is those sixteen plus the rope and the whistle itself:

> knife · rope · lantern · canteen · matches · shovel · pocket watch ·
> binoculars · helmet and headlamp · hiking boots · raincoat · map of the caves ·
> rucksack · tent · whistle · axe · air mattress · air pump

## Departure 1 — wait for the butler before `get china` / `get crystal`

The author's file takes the china and the crystal the moment the butler asks for
help with them. Here that silently loses the game 500 commands later, and the
reason is worth writing down.

TASK 81 (`* get * china *`) and TASK 87 (`* get * crystal *`) are both
`where=1 room=6` (the dining room) and both carry

```
RESTR type=3 v1=2 v2=0 v3=0        -- NPC 0 (the butler) in the same room as you
ACT   type=3 v1=7 v2=1 v3=1        -- butlermap += 1
```

At `butlermap == 2`, TASK 422 (`#Check Butlermap Variable`, `RESTR type=4 v1=9
v2=2 v3=2`) fires TASK 423, and the butler hands over the **map of the caves** —
which is one of the sixteen. Nothing else in the game sets `butlermap`.

The trap is that EVENT 33, the one that voices his request…

> "…I could really use some help moving the fine china and crystal ware to a
> safer place."

…is **pure narration**. It has `texts=S--` and no object or NPC moves; it does
not put the butler in the room. On the turn it fires he has in fact just walked
out ("The butler exits."), and `x butler` answers "You see no such thing". So
`get china` falls through to the plain library take ("You take the Sunday Best
china from the shelf."), `get crystal` prints TASK 87's failure message ("There
are too many pieces to carry by yourself."), `butlermap` stays 0, and the map is
never given. `get map` at the maze entrance then answers "Take what?".

The fix is to stand in the dining room and wait for his walk to bring him back —
nineteen turns from that point under this route — and only then take the china
and the crystal. Both then play the intended scene ("Please, Corey, let me help
you with that."), `butlermap` reaches 2, and a few turns later:

> He holds the map out to you and you hesitantly accept it.

The author's transcript was evidently recorded on a build (or a turn count) where
the butler happened to be standing there.

## Departure 2 — load the rucksack instead of carrying by hand

The author's endgame picks up eleven items in a row at the maze entrance. Under
this build that overruns both carry limits at once:

```
> get rucksack      Your hands are full at the moment.
> get binoculars    The pair of binoculars is too heavy for you to carry at the moment.
> get raincoat      The raincoat is too heavy for you to carry at the moment.
```

The rugged rucksack is the answer, and the game says so: "large enough to carry
quite a few items. It has a strap on it allowing the pack to be worn over the
shoulder." So the route takes it **first**, wears it, opens it, and then does
`get X` / `put X in rucksack` for each of rope, axe, canteen, knife, lamp,
shovel, binoculars, tent, map, raincoat and whistle. Worn, it costs no hands, and
everything fits in a single trip — which is why this route ends up shorter than
the source file despite the nineteen added waits: the author's second round trip
for the raincoat and the whistle is no longer needed.

At the altar the items come back out one at a time (`get X` / `put X on altar`,
so only one item is ever in hand), then `remove helmet` / `remove boots` /
`remove rucksack` / `put all on altar` — the rucksack itself is one of the
sixteen — and finally `get whistle` (which `put all on altar` had just deposited)
and `blow the whistle`.

## EVENT 7 and the event-length roll (was: why `SCR_SEED=2`)

EVENT 7 is an immediate event with `Time1=0 Time2=1` whose affected task is TASK
124 `#Run Gender Task`, and that task's Where list is rooms `[0, 165]`. EVENT 8
(immediate, zero-length) runs TASK 127 `#Move PLAYER to START`, which takes the
player out of room 165.

- If EVENT 7's length rolls **0** it is finished by `evt_finish_load_events()`
  during load, in index order **before** EVENT 8, while the player is still in
  room 165. The Where check passes and the player starts dressed in the brown
  tweed suit.
- If it rolls **1** it comes due at the end of turn 1, by which time the player
  is in room 17, the Where check fails, and the suit is never worn.

**Ground truth: run400 always wears the suit.** Four live runs of `run400.exe`
under Wine on the shipped `provenance.taf` (four bare Returns for the two
keypress pages, the name dialog and the gender dialog — whose default is Male —
then `i`) all print, on turn 1:

> You are wearing a brown tweed suit, and you are carrying a pocket watch.

Under Scarier's original **inclusive** `scr_randomint (time1, time2)` roll,
only some seeds rolled 0 — the default `SCR_SEED=1` rolled 1 and left the suit
unworn, so the row was first wired with a `SCR_SEED=2` pin to reproduce the
Runner. That anomaly is what raised **§10** of
`terps/scarier/RUNNER_TESTS_TODO.md`: is the Runner's event-length roll
exclusive of `Time2`?

**Answered and ported the same day (2026-08-17).** Live probes in both run400
and run390 (config `EL` in `make_arena_probe.py`, `make_39_evlenprobe.py`;
~240 draws on `1..3` ranges, every draw in {1,2}) showed both Runners roll
event lengths *and* delays as `lo + Int(Rnd * (hi - lo))` — exclusive upper
bound. Scarier now uses `scr_randomint_exclusive()` at the three event-timing
call sites, EVENT 7's `0..1` roll is length 0 on **every** seed exactly as the
four run400 runs showed, and the game wins on the default `SCR_SEED=1`: the
pin is gone. §10 has the full record, and its closure-log entry lists the
nine other rows the corpus-wide stream shift re-derived or re-pinned.

## Odds and ends

- `SCR_SKIP_WAITKEY=1` for the paged intro and the several long set pieces.
- The solution's `Corey` / `male` answer the name and gender prompts. The gender
  answer matters: the game gates tasks on it, and with the (now fixed) inverted
  `PlayerGender` enum a male player was being dressed in a house dress.
- Line 493 of the solution is a bare `a cauldron`. That is the author's own typo
  (his file has `x athanor` then `a cauldron` on the next line); it is kept
  verbatim, produces "I don't understand what you want me to do with the
  cauldron", and costs no turn.
- The route *does* walk the hedge maze (43 rooms of "Inside A Hedge Maze" before
  "At The Centre Of The Topiary" and the storm drain), because the author's
  walkthrough does. The readme says there is another way in — "the maze does not
  need to be navigated to complete the game" — but there was no reason to go
  looking for it once the author's directions replayed cleanly.
