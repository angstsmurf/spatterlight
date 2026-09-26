# Liqid — walkthrough (**best reachable state, score 60/100**)

- **Title:** *The Quest For More Hair or AMU Part 1: The Smugglers*.
- **Engine:** ADRIFT 3.90, second person, battle system enabled.
- **Result:** 60/100, the real reachable ceiling. The route clears the town
  (30 points), crosses the river, and then plays the palace, Frad's riddle,
  the cave/rabbit warren and the rope cliff (+30). It ends at the Airport,
  where the game soft-locks. Wired as
  `liqid_solution.txt|liqid.taf|I surrender, you win!`, no env.
  There is no reachable ending.

## Why 60 is the ceiling (40 points unreachable)

- **Airport chain, 20 points (T37 briefcases, T38 `ring 987`, T39 give tapes to
  the King, T40 buy the tape player).** `rope cliff` (T36) hides the rope
  (move to hidden). Its restriction is "rope held", and the reverse command
  (the same `rope cliff`) is checked against that same restriction, so the
  task can never be undone. The Airport's S/W exits and Forest 1c's N/E/W
  exits both need T36 NOT done. After roping you can only move between
  Forest 1c and the Airport. Goosebury Brothers (T40) opens only through
  event "Goose", which T37 starts. So all four tasks are out of reach.
  The ReverseMessage ("You are now free to move except up the cliff") shows
  the author meant the reversal to work. Both run390's checktask and run400's
  pre-matcher test restrictions for reverse matches too, so the Runner
  should block it as Scarier does.
- **Smuggler/hair ending, 20 points (T43 smuggler dies 5, T44 buy hair 10,
  T45 talk to the King with the hair = the only EndGame win).** Hamish's
  Battle KilledTask is 0, and no event or action executes
  `^^smugglerdiesevent^^`. The Bridge's north exit to the Hair Shop needs T43.
  The only way in is to type the internal `^^...^^` command, which is a
  loophole and is not used here.
- `rope cliff` therefore has to be the LAST scoring action. The mushrooms
  (`eat/take mushroom` in Forest 2d/4d) teleport you into an inescapable
  loop room, so avoid them.

## Money

The start gives 100 and the vault gives 750. Only the blaster (250) is
needed: once `fight jenkins` has run and the Sword is dropped,
`shoot jenkins with blaster` wins outright. Armour, spear and gun are not
needed.

## The two puzzles

**The bank vault** takes the Key found on the Drunk's body (killed with the
Sword from under the bed) to open the vault, then any 9-digit number
followed by the fixed 4-digit code `5764` for +750 Denmarkians — the money
that pays for gear at Joe's Blacksmiths.

**Jenkins the ferryman** looked unwinnable at first: buying armour/spear/gun/
blaster from Joe only places them on his workbench (`buy spear` etc. never
add to inventory — a separate `get spear` is required), and the weapon
actually wielded in combat is whichever was equipped first, here the Sword.
`shoot jenkins` while the Sword is wielded always answers "You can't shoot
with the Sword!" — there is no `wield`/`hold` verb to switch weapons.
Dropping the Sword instead makes later attack verbs prompt "What do you want
to attack Jenkins with?", answerable inline as `shoot jenkins with blaster`.
Even then, shooting does nothing until the scripted `fight jenkins` sets his
hostile combat state; once fought, `shoot jenkins with blaster` wins the
fight outright ("I surrender, you win!") and crosses the river.

## After the river (new, 2026-09-26)

- Palace (Forest 2c, R21): `look through arch` (+5). The guard-change event
  carries you in two turns later. `talk king` (+5), then `yes` gets the rope.
- Frad (up the ladder in Forest 4c): `ape`, `are`, `ore`, `owe`, `owl` (+5,
  shovel).
- Cave (east of Forest 3e): `dig` (+5), `d`, `show sock to rabbit` (+5; the
  socks come from the bedroom dresser at the start), `u`.
- Forest 1c: `rope cliff` (+5), `u` to the Airport. You are soft-locked here.

## The walkthrough

```
stand
look under bed
open dresser
get socks
s
w
attack drunk
attack drunk
attack drunk
attack drunk
get key
s
s
unlock vault
123456789
5764
n
w
w
ring bell
wait
wait
buy blaster
get blaster
e
e
e
fight jenkins
drop sword
shoot jenkins with blaster
ride ferry
e
n
e
look through arch
wait
wait
talk king
yes
s
e
n
n
w
u
ape
are
ore
owe
owl
d
e
s
e
e
dig
d
show sock to rabbit
u
w
s
s
w
w
rope cliff
u
score
```
