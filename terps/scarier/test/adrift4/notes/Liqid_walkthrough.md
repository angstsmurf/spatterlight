# Liqid — walkthrough (**best reachable state, score 30/100**)

- **Title:** *The Quest For More Hair or AMU Part 1: The Smugglers*.
- **Engine:** ADRIFT, second person, battle system enabled. A large opening
  area (bedroom/village/bank/blacksmiths/river crossing) gates a much bigger
  quest (a palace subplot, Frad's riddle, a cave/rabbit warren, a rope-cliff
  mechanic, and an airport tapes/bomb puzzle) confirmed via `SCR_DUMP_TASKS`
  to be far larger than this walkthrough attempts.
- **Result:** the walkthrough clears the opening area's two real obstacles
  and stops at the far bank of the river (30/100), a natural checkpoint
  before the much larger remaining quest. Wired as
  `liqid_solution.txt|liqid.taf|I surrender, you win!`, no env.

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

## The walkthrough

```
stand
look under bed
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
buy armour
buy spear
buy gun
buy blaster
get spear
get gun
get blaster
e
e
e
fight jenkins
drop sword
shoot jenkins with blaster
ride ferry
look
score
```
