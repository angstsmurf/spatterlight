# Vagabond — walkthrough

- **Engine:** ADRIFT 4 (`Vagabond.taf`, Scott Meridian 2004, ScummVM gameid
  `1h_vagabond`). A 1-Hour Game Comp entry: stow away on a cargo freighter.
- **Result:** ★ **WON, 1/1 MAX.** The game has exactly one point.
- **Solution:** `goldens/vagabond_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `The End`.

## Route

The game ships its own `walkthru` command; this is that route, verified live:

```
(blank)          <- "[Press any key to begin]"
west
north
west
give credits to bum
open toolbox
get torch
east
east
remove grate
enter vent
```

The bum trades you the toolbox for your credits; the toolbox holds a laser
torch; the torch cuts the grating off the ventilation shaft, and the shaft leads
to the docking bay and the freighter.

## Notes

- The game's built-in `walkthru` lists **one more `west`** than this route. That
  extra move is a *blocked* exit whose failure message triggers the guard/bum
  scene; skipping it changes nothing about the outcome and gives a cleaner
  transcript. (This was checked properly, by probing with an unparsable token:
  the 11- and 12-command routes produce identical transcripts otherwise, which
  is what made it look like an off-by-one at first.)
- The .taf contains a `0xAE` (®) byte — see the note in `SPAM_walkthrough.md`
  about `LC_ALL=C` and `grep -a`.
