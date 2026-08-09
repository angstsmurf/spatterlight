# SPAM — walkthrough

- **Engine:** ADRIFT 4 (`SPAM.taf`, ScummVM gameid `1h_spam`). A 1-Hour Game
  Comp entry: SPAM®, Inc. is putting mind-control drugs in the potted lunchmeat
  and you are going to take the company off them.
- **Result:** ★ **WON, 20/20 MAX**, rank **"Spam King" — "The highest rank of
  all! You rule!"**
- **Solution:** `goldens/spam_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Spam King`.

## Structural verdict

Two `ChangeScore` tasks, `+10` each, and that is the whole scoring table:

1. getting into the car **with the keys**, and
2. saying `ingredients` to the receptionist.

The ending itself scores nothing — it is the CEO scene.

## Route

```
(blank)          <- the title's two "...Continue..." pauses
(blank)
take spam
north
enter car        <- fails, but this is what MOVES the keys (task 1)
south
take keys
north
enter car
start car
north
ask about ingredients
z
z
z
ask ceo about spam
```

## Notes

- **The keys are the trap.** `take keys` in the kitchen fails at first because
  the keys are not there yet: task 1 only moves them onto the counter after a
  *first, failed* `enter car`. You have to try the car, walk back, then collect
  them.
- The three `z`s are load-bearing: they let event 0 (the guard escorts you out
  of the holding cell) and event 1 (the CEO walks in) fire. Event 2 kills you
  ten turns later, so there is no slack to spare.
- The .taf contains a `0xAE` (®) byte, so any ad-hoc probing of a transcript
  needs `LC_ALL=C` and `grep -a` — BSD `grep` otherwise treats the transcript as
  binary and `grep -c` prints *nothing at all* rather than `0`. The harness
  already exports `LC_ALL=C`.
