# An Evening with the Evil Chicken of Doom — walkthrough

- **Engine:** ADRIFT **3.9** (`ECOD3.taf`, ScummVM gameid `1h_ecod3`) — one of
  the few 3.9 files in this corpus, and a *different game* from the ADRIFT 5
  `ECOD3D.taf` in the a5 corpus. The Evil Chicken of Doom is performing
  *Ghostbusters 2* as a solo act; you are in the audience with an axe to grind.
- **Result:** ★ **WON**, 0/0 — the game keeps no score at all.
- **Solution:** `harness/ecod3_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Congratulations!`.

## Structural verdict

15 tasks, 4 rooms, 1 NPC (The Usher, who patrols between the Aisle and the
Bottom of Stage). There is no `ChangeScore` action anywhere; the single
`EndGame(win)` is task 14, `open trapdoor` while holding the crowbar.

## Route

```
get gum
light man on fire
e
take crowbar
n
z
z
take flashlight
fix flashlight
n
open trapdoor
```

## Notes

- **The Usher has to be in the room.** Task 6 ("In the dark shadows at the
  bottom of the stage, you swipe the flashlight from the Usher") carries a
  character restriction, and he is only there on part of his patrol — hence the
  two `z`s. Arrive at the wrong moment and you get "The Usher isn't here right
  now."
- The gum comes first because it is what repairs the flashlight's two loose
  wires (task 11), and the fixed flashlight is what task 12 checks before it
  will let you go north into the dark.
- You start holding the book of matches; `light man on fire` is what clears the
  large man out of the seat next to you and opens the east exit (task 7). He
  takes it well: *"Indeed I am. I'd better go see a doctor about this."*
- No leading blank line is needed — the intro prose is printed without a
  press-a-key pause.
