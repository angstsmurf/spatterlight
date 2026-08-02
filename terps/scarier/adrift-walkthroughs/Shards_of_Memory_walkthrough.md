# Shards of Memory — walkthrough

- **Engine:** ADRIFT 4 (`shardsofmemory.taf`, Stewart J. McAbney, InsideADRIFT
  Spring Comp 2004). First-person amnesia fantasy: you wake on a beach with no
  past, and the `remember` verb hands your memories back a shard at a time.
- **Result:** **WON** — the game reaches its `VICTORY` ending. There is **no
  scoring system at all**: 803 tasks in the dump and not one `ACT type=4`.
- **Solution:** `harness/shardsofmemory_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `My adventure has ended, and in victory besides`.

## Route

Straight from the author's own walkthrough, `springcomp04.zip →
walkthru/walkthru/wt-shards.txt`. It replays verbatim — no command needed
rephrasing, and nothing in the transcript is a parser failure.

```
1 / e / x floor / wait / wait / wait / wait / wait / smash east wall with flint
n / n / w / sw / in / talk to marhon / 2 / talk to marhon / 2
out / ne / e / in / pray / sw / w / sw / in
talk to marhon / 3 / out / ne / e / e / n / e / s
s / s / in / x tapestry / throw lightstone at demon / e / e / 2 / e
2 / in / out / out / 1 / kneel / kiss hand / se / 1
1 / 1 / 1 / 1 / talk to youth / 4 / 1 / 4 / 1
1 / nw / out / 6 / x guard / 1 / out / 4 / in
n / x desk / talk to cedrik / 4 / 1 / s / out / out / 5
in / touch wheel with key / touch key to wheel / touch cell with key / d / d / d / x door / touch depression with key
touch key to depression / i / use key / talk to reaper / 2 / 1 / 1 / 2 / 2
2 / out / 3 / n / n / n / n / call reaper / kill fallen with giver of sleep
```

(one command per line in the solution file, interleaved with blank lines —
see below.)

## Notes

- **The bare digits are menu answers, not commands.** The leading `1` picks
  Male at the game's opening "am I male or female?" prompt; the rest choose
  topics inside `talk to <someone>` menus, and the `3` after the Reaper
  conversation is that menu's exit.
- **45 `...press a key...` pauses**, so the solution carries 45 blank lines.
  Their positions were *measured*, not guessed: `SCR_MARK_WAITKEY=1` prints a
  marker per pause and `SCR_SKIP_WAITKEY=1` keeps the command list in sync
  while the measurement runs, so the padding falls out of one run. The two
  leading blanks answer the title card and the `remember` hint, before the
  gender prompt.
- The author's key-puzzle lines are exploratory — `touch wheel with key`,
  `touch key to wheel`, `touch cell with key`, `touch depression with key`,
  `touch key to depression`, `use key` — several of which are the same idea
  spelled different ways. They are kept verbatim rather than trimmed to the
  one phrasing that works, because the golden's job is to reproduce the
  author's transcript.
- The ending is deliberately unsatisfying by design: the victory text itself
  says "something tells me that this story has not ended", and invites a
  replay to find what the route skipped. With no score to chase, this
  walkthrough is the spine only.
