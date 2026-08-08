# Mammoth Vacuum Button of Death — walkthrough (**WIN**, 11 commands)

- **Author:** Daniel Airey, written for *The penultimate not numbered New
  Year's Speed IF* (played at ClubFloyd on 8 January 2012). A speed-IF: you
  wake in an asylum cell with a cake, and the game spends more words insulting
  the player, the author and the genre than it does describing rooms.
- **Engine:** **ADRIFT 4.00**
  (`xxd -p -l 12 games/MammothVacuum.taf | cut -c17-22` → `93453e`).
- **Result:** **WON.** There is no score system (`score` reports 0 of 0), so
  reaching the foyer exit is the whole of it.
- **Source:** `downloaded/MammothVacuumButtonOfDeath_clubfloyd.html` is a group
  play session covering six speed-IF games; the Mammoth section wanders,
  jokes and backtracks and is not replayable. This route is derived from the
  task dump.
- Row: `mammoth_solution.txt|MammothVacuum.taf|After many testing trials|SCR_SKIP_WAITKEY=1`.
  The waitkey flag is needed for the dream intro and the closing screen.

## The one thing worth knowing

`strip` and `strip guard` are two different tasks and the route needs both,
in that order. The first is you taking your own clothes off in front of the
guard — which is what makes him "pass out in his chair" — and only then does
the second one work, putting his uniform on you. Doing them the other way
round, or skipping the first, leaves the guard awake and the button
unpressable. The game is a joke about this and nothing else.

Everything after that is signposted in the room text: the button behind the
guard is "clearly labelled", pressing it unlocks the west security door, and
west-then-south-then-west is the exit.

## Shape of it

`harness/mammoth_solution.txt`, 11 lines:

```
eat cake              the key is baked into it
grab key
unlock cell
n                     Utterly Pointless Hallway
n                     Guard Station
strip                 you, not him -- this is what knocks him out
strip guard           now his uniform is yours
press unlock button
w                     Utterly Pointless Hallway #2
s                     Foyer
w                     -- WIN
```
