# Weirdstuff2 — walkthrough (**best reachable state**)

- **Engine:** ADRIFT. A short horror/mystery opener: the player, amnesiac
  after "last night", meets an old flame (Donna) in a hotel lounge to
  retrieve a package left for safekeeping, then goes to find a missing
  friend (Duffy) in room 1436 on the 14th floor.
- **Result:** best-reachable, not a win — this game has no win state at
  all. `SCR_DUMP_TASKS` shows `WINTEXT []` (empty). Wired as
  `weirdstuff2_solution.txt|weirdstuff2.taf||`, no env, no win marker.
- **Entering room 1436 is a scripted, unavoidable trap:** Viveka darts the
  player with venom regardless of any preparation, and play resumes in a
  windowless "Cell". The Cell is a genuine engine-level dead end — its EXIT
  table (`SCR_DUMP_TASKS`) has zero exit entries for that room, so no
  compass direction (nor "out"/"leave"/etc.) ever leaves it, even after the
  door is opened.
- **Two apparent puzzle hooks in the Cell are both scripted red herrings**
  that lead nowhere:
  - A "black object" (its ALTCMD vocabulary reveals it's a phone) sits on
    an unreachable ledge; `take object` always fails ("too high up to
    reach") with no restriction that could ever pass.
  - A nail can be found and picked up from under the door (`examine gap`
    x2, `get nail`), but `pick lock with nail` always fails ("too big to
    fit"), and `open door` succeeds on its own with no lock check at all —
    opening it doesn't create an exit either, since the room has none.
- The script plays the full opening (lounge/package/Donna, front desk, the
  elevator ride to 14, the corridor, unlocking and entering room 1436)
  through to the Cell, then exhausts the Cell's red herrings for
  completeness before stopping — an honest best-reachable ending, per the
  Sandy/Penrhyn/TheWill precedent for games with no reachable win state.
- **Content note:** no minors appear anywhere in the game text (cast: the
  player, Donna, a hotel clerk, a bar tender, hotel guests, and Viveka).

## The walkthrough

```
n
look at woman
buy drink
give drink to donna
open package
read letter
s
ask clerk about room
open elevator
press button
wait
wait
wait
wait
wait
look
e
push 14
wait
wait
wait
look
w
wait
s
unlock door with key
open door
in

examine ledge
examine object
take object
examine crack
examine gap
get nail
examine gap
pick lock with nail
climb wall
open door
```
