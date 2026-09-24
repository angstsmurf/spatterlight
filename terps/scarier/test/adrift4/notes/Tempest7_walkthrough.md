# Storm Tossed (tempest7) — walkthrough (**WIN, 305/305**)

- **Engine:** ADRIFT 4.00. "Storm Tossed" by David Grigg, loosely based on
  *The Tempest*. The player is Ferdinand, shipwrecked on Prospero's island,
  and wins by giving Miranda enchanted flowers (task 53, EndGame + 50).
- **Result:** **WIN, 305/305**, 143 lines (3 of them blank). Proposed row:
  `tempest7_solution.txt|tempest7.taf|Congratulations, you have won!|SCR_RNG=xoshiro`.
  The win marker is the WINTEXT, which is printed only by the EndGame action of
  task 53. Three row.sh runs are byte-identical.
- **Env:** `SCR_RNG=xoshiro` (row.sh sets it anyway). The girl's arrival at
  East Stream is timing- and RNG-dependent. The route was tuned turn by turn,
  so don't insert or remove turns without re-tuning the `ask girl her name` count.
- **Score map (305):** abandon ship T10 +10, cut vines T12 +10, kill shark
  (throw dagger) T20 +20, quicksand contraption T30 +50, turn dragon wearing
  the gloves T37 +40, lift tapestry T39 +5, put the key in the cold retort T46 +5,
  light the burner with the key in T45 +20, light the burner again with the key gone
  gold T44 +5, read the book T48 +50, ask the girl her name T49 +20, pick the flowers T54
  +10, empty the vial T58 +10, give the enchanted flowers T53 +50. Avoid T25
  (attacking Miranda, −100).

## Route by phase

1. **Ship (lines 1–26).** Two blank lines absorb the intro waitkeys. Line 3
   must be `look`: the player is still in "Hung over" (room 58) for the first
   real turn, so `open window` there says "You can't open that." Get the brass
   key through the window, unlock the door, and take the cask. `empty cask` answers
   "closed", so `open cask` + `pour wine` instead. Then get the dagger and sword from the
   armoury, `jump overboard`, and `throw dagger at shark` in the water. The brass key is lost
   in the jump, which is harmless.
2. **Landing (line 27 blank).** Task 80 (reaching land) has a `<waitkey>`.
3. **Contraption over the quicksand.** The carry limit (size 90) will not take
   the plank (81) together with the sword (27), so the route is a two-trip
   ferry: drop the pouch at the spit, get the twigs, `z` for the sword to wash up,
   cut the vines, drop the cask, vines and sword at the quicksand's south side, and
   fetch the plank alone. Then `tie plank to cask with vines`.
4. **Snake and fire.** `kill snake with sword` in the snake pit, and
   `put twigs in fire` (burning twigs light the way for 20 turns).
   The exit south from room 26 is task 28 and must be typed `south`. `s` is refused.
5. **Gloves and dragon.** `u` to the nest (`x nest` first, then `get gloves`), then climb to
   the cell. `wear gloves` then `turn dragon`.
6. **Prospero's Cell.** `lift tapestry`. `x desk` must come before `get globe`
   (the globe has to be seen). Get the small key from the drawer and put it in the cold
   retort. Light the burner twice: the first light is T45 (+20) and the second is T44 (+5).
   `turn off burner` is refused, but the second `light burner` still fires. Then
   `x table` and `x retort` before `get golden key`, which opens the bottom drawer
   and gets you the vial. `pour fluid` (T58). `open vial` only works while the vial is held,
   and gives the silver key, which unlocks the book. Then `read book` (+50).
7. **Inside the book.** Chap. 4 (room 50) holds Death, so avoid it. Take
   45 S 51 W 53 to `get spell`, then S 49 U 54 U back to the cell.
8. **Flowers.** Cell OUT, W, N, N, E to the Field of Flowers: `pick flowers`, then
   `cast spell on flowers`. That puts the bouquet down and swaps it for the
   enchanted flowers, which must then be taken as `get enchanted flowers`.
   A plain `get flowers` hits task 54 (pick) and fails "What flowers?".
9. **Miranda.** Go back down over the bridge to East Stream (S S D D SE W N N NW N).
   `ask girl her name` fails "You don't see the girl here!" until she wanders
   in. The ninth ask lands (T49), and the next turn's `give flowers to girl` wins.

## Engine oddities (not fixed)

- With `z` in place of the failed asks, the girl's movements come out different
  (she "runs away" before the give can land). A failed-restriction task turn
  and a `z` turn do not advance her the same way. This may be real Runner behaviour
  or a Scarier divergence. It has not been measured against the Runner.
- The `ask girl` disambiguation echo prints `(No female)` on the first
  attempt and `(A pretty girl)` while she is absent. It is cosmetic.
- After the quicksand the player is "wearing masses of sloppy muck".
- The end summary says "That is 99% of the game!" at 305/305. That is the
  Runner-faithful `Int(score * (100 / MaxScore))` float truncation (see the
  endgame-summary port), not a bug.
- The `<waitkey>`s inside task 53's completion text never pause in scare. The
  ending prints in full with no bridge lines needed.

## The walkthrough

See `goldens/tempest7_solution.txt` (143 lines).
