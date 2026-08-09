# The Revenge Of Clueless Bob Newbie, Part 2: This Time It's Personal — walkthrough

- **Engine:** ADRIFT 4 (`cbn2.taf`, **3rd ADRIFT One-Hour Game Competition**,
  2003). Direct sequel to `CBN.taf`; the "Masterpieces of CBN" CD is now in the
  Adventure Corps archives and Bob wants it back.
- **Result:** ★ **WON, 30/30 MAX.**
- **Solution:** `goldens/cbn2_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `the archives room goes up in flames`.

## Structural verdict

32 tasks, 7 rooms, two NPCs (Lisa, Snarf). Six tasks carry `ChangeScore(+5)` —
5, 7, 10, 15, 21 and 23 — so the maximum is 30 and this route takes all six.
Task 23, `light *match*` in the Archives, is the only `EndGame(win)`.

## Route

```
        ← blank ×3   (intro "...press a key...")
        ← blank      (throwaway command for task 0)
x girl
u
nw
x desk
        ← blank
se
s
fill cup
n
d
give coffee
talk to snarf
u
tell journalists about the wife
n
take match
s
ne
light match
```

## Notes

- **The blanks.** The intro has three `...press a key...<waitkey>` pauses, and a
  fourth line is needed as the throwaway command that task 0 turns into the move
  out of room 0 *[The Story So Far…]* — hence four leading blanks. `x desk` in
  Bogg's Office then has a `<waitkey>` in the middle of its message
  (*"completely empty.`<waitkey>` Aside, that is, from an empty cup"*), which
  eats the next line, hence the blank after it.
- **`give coffee` must NOT name Lisa.** Task 4 (`[*lisa*]`) sits ahead of task
  10 (`give *coffee*`) in task order and matches *any* command containing her
  name, so `give coffee to lisa` only ever gets *"Back off, buster!"* and the
  passcard never arrives. The same trap in reverse: `x girl` (task 2) is what
  brings Lisa on stage at all — she starts off-map and is only scenery in the
  room description until then.
- The chain: `x desk` → the empty cup; `fill cup` in the toilets → "coffee"
  (yes, from the toilet); `give coffee` → the passcard, which is the sole
  restriction on task 12 (`ne`) into the Archives.
- `talk to snarf` (task 15) happens in **Reception**, not upstairs; he stomps
  off to his office and mentions the mistress. `tell journalists about the wife`
  (task 21) in the Open Area needs task 15 done — Snarf panics, goes out the
  window, and leaves both his office unlocked (the task sets the flag task 8
  tests) and his lit cigar's match behind.
